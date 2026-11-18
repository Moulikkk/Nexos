#include <nexos/file.hpp>
#include <nexos/nexos.hpp>
#include <nexos/pipe.hpp>
#include <nexos/process.hpp>

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// One word from the command line. op is true only for an UNQUOTED < > or >>,
// so a quoted ">" is still just ordinary text.
struct Token {
    std::string text;
    bool op = false;
};

// One command of a pipeline, plus any file redirections attached to it.
struct Command {
    std::vector<std::string> args;
    std::string input_path;
    std::string output_path;
    bool has_input = false;
    bool has_output = false;
    bool append = false;
};

std::vector<std::string> split_pipeline(const std::string& line) {
    std::vector<std::string> commands;
    std::string current;
    bool single = false, quoted = false, escaped = false;
    for (char ch : line) {
        // Backslashes are KEPT here so tokenize() can still see that a
        // character (like \> or \|) was escaped.
        if (escaped) { current += ch; escaped = false; continue; }
        if (ch == '\\' && !single) { current += ch; escaped = true; continue; }
        if (ch == '\'' && !quoted) { single = !single; current += ch; continue; }
        if (ch == '"' && !single) { quoted = !quoted; current += ch; continue; }
        if (ch == '|' && !single && !quoted) {
            commands.push_back(current);
            current.clear();
        } else current += ch;
    }
    commands.push_back(current);
    return commands;
}

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    std::string token;
    char quote = 0;
    bool escaped = false, active = false;

    auto flush = [&] {
        if (active) { tokens.push_back({token, false}); token.clear(); active = false; }
    };

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (escaped) { token += ch; escaped = false; active = true; continue; }
        if (ch == '\\' && quote != '\'') { escaped = true; active = true; continue; }
        if (ch == '\'' || ch == '"') {
            if (quote == 0) { quote = ch; active = true; continue; }
            if (quote == ch) { quote = 0; continue; }
        }
        if (quote == 0 && (ch == ' ' || ch == '\t')) { flush(); continue; }
        if (quote == 0 && (ch == '<' || ch == '>')) {
            flush();
            std::string op(1, ch);
            if (ch == '>' && i + 1 < input.size() && input[i + 1] == '>') { op += '>'; ++i; }
            tokens.push_back({op, true});
            continue;
        }
        token += ch;
        active = true;
    }
    if (escaped) token += '\\';
    flush();
    return tokens;
}

// Turns one pipeline segment into a Command: plain words become arguments,
// and each "< file", "> file" or ">> file" is moved into the redirection fields.
Command parse_command(const std::string& segment) {
    Command cmd;
    const auto tokens = tokenize(segment);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (!t.op) { cmd.args.push_back(t.text); continue; }

        if (i + 1 >= tokens.size() || tokens[i + 1].op) {
            throw std::invalid_argument("expected a file name after '" + t.text + "'");
        }
        const std::string& path = tokens[++i].text;
        if (t.text == "<") {
            if (cmd.has_input) throw std::invalid_argument("duplicate input redirection");
            cmd.has_input = true;
            cmd.input_path = path;
        } else {
            if (cmd.has_output) throw std::invalid_argument("duplicate output redirection");
            cmd.has_output = true;
            cmd.append = (t.text == ">>");
            cmd.output_path = path;
        }
    }
    return cmd;
}

bool is_builtin(const std::string& name) {
    return name == "exit" || name == "cd" || name == "help";
}
}

int main() {
    std::cout << "Nexos Shell v" << nexos::version() << "\n"
              << "Type 'exit' to quit. Pipelines with | and redirection with < > >> are supported.\n";
    std::string line;
    while (std::cout << "nex$ " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        auto segments = split_pipeline(line);
        std::vector<Command> commands;
        try {
            for (const auto& segment : segments) commands.push_back(parse_command(segment));
            for (std::size_t i = 0; i < commands.size(); ++i) {
                if (commands[i].args.empty()) {
                    throw std::invalid_argument("invalid empty command in pipeline");
                }
                if (commands[i].has_input && i != 0) {
                    throw std::invalid_argument("input redirection '<' is only allowed on the first command");
                }
                if (commands[i].has_output && i + 1 != commands.size()) {
                    throw std::invalid_argument("output redirection is only allowed on the last command");
                }
            }
        } catch (const std::invalid_argument& error) {
            std::cerr << "nex: " << error.what() << '\n';
            continue;
        }

        if (commands.size() == 1 && is_builtin(commands[0].args[0]) &&
            (commands[0].has_input || commands[0].has_output)) {
            std::cerr << "nex: redirection is not supported for built-ins\n";
            continue;
        }
        if (commands.size() == 1 && commands[0].args[0] == "exit") break;
        if (commands.size() == 1 && commands[0].args[0] == "cd") {
            const std::string target = commands[0].args.size() > 1 ? commands[0].args[1] : std::getenv("HOME");
            if (target.empty() || ::chdir(target.c_str()) != 0) perror("nex: cd");
            continue;
        }
        if (commands.size() == 1 && commands[0].args[0] == "help") {
            std::cout << "Built-ins: cd [dir], help, exit\n"
                      << "Run external programs and connect them with '|'.\n"
                      << "Redirect with: cmd < in.txt, cmd > out.txt (overwrite), cmd >> out.txt (append)\n"
                      << "Example: ls -l | grep cpp | wc -l\n";
            continue;
        }

        try {
            // 1. Open every redirection file BEFORE starting any process, so a bad
            //    path fails cleanly (FileError) with nothing running yet.
            std::vector<nexos::File> files;
            files.reserve(2);
            int input_override = -1, output_override = -1;
            if (commands.front().has_input) {
                files.emplace_back(commands.front().input_path, true, false);
                input_override = files.back().native_handle();
            }
            if (commands.back().has_output) {
                files.emplace_back(commands.back().output_path, false, true,
                                   commands.back().append ? nexos::WriteMode::Append
                                                          : nexos::WriteMode::Overwrite,
                                   true);
                output_override = files.back().native_handle();
            }

            // 2. Create the pipes between commands.
            std::vector<nexos::Pipe> pipes;
            for (std::size_t i = 1; i < commands.size(); ++i) pipes.emplace_back();
            std::vector<int> all_fds;
            for (const auto& pipe : pipes) {
                all_fds.push_back(pipe.read_fd());
                all_fds.push_back(pipe.write_fd());
            }

            // 3. Start the children. A redirection file replaces the pipe (or the
            //    terminal) on that side of the command.
            std::vector<nexos::Process> children;
            try {
                for (std::size_t i = 0; i < commands.size(); ++i) {
                    const int input = i == 0 ? input_override : pipes[i - 1].read_fd();
                    const int output = i + 1 == commands.size() ? output_override : pipes[i].write_fd();
                    children.push_back(nexos::Process::spawn(commands[i].args, input, output, all_fds));
                }
            } catch (...) {
                // If a later spawn fails, close our pipe ends before the already
                // running children are waited for. Otherwise a child could block
                // forever on a pipe we still hold open.
                for (auto& pipe : pipes) { pipe.close_read(); pipe.close_write(); }
                throw;
            }

            // 4. The parent no longer needs the pipes or the files; the children own copies.
            for (auto& pipe : pipes) { pipe.close_read(); pipe.close_write(); }
            files.clear();

            int last_status = 0;
            for (auto& child : children) last_status = child.wait().exit_code;
            if (last_status != 0) std::cerr << "nex: command exited with status " << last_status << '\n';
        } catch (const std::exception& error) {
            std::cerr << "nex: " << error.what() << '\n';
        }
    }
    return 0;
}
