#include <nexos/nexos.hpp>
#include <nexos/pipe.hpp>
#include <nexos/process.hpp>

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split_pipeline(const std::string& line) {
    std::vector<std::string> commands;
    std::string current;
    bool single = false, quoted = false, escaped = false;
    for (char ch : line) {
        if (escaped) { current += ch; escaped = false; continue; }
        if (ch == '\\' && !single) { escaped = true; continue; }
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

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> args;
    std::string token;
    char quote = 0;
    bool escaped = false, active = false;
    for (char ch : input) {
        if (escaped) { token += ch; escaped = false; active = true; continue; }
        if (ch == '\\' && quote != '\'') { escaped = true; active = true; continue; }
        if ((ch == '\'' || ch == '"')) {
            if (quote == 0) { quote = ch; active = true; continue; }
            if (quote == ch) { quote = 0; continue; }
        }
        if ((ch == ' ' || ch == '\t') && quote == 0) {
            if (active) { args.push_back(token); token.clear(); active = false; }
        } else { token += ch; active = true; }
    }
    if (escaped) token += '\\';
    if (active) args.push_back(token);
    return args;
}
}

int main() {
    std::cout << "Nexos Shell v" << nexos::version() << "\n"
              << "Type 'exit' to quit. Pipelines with | are supported.\n";
    std::string line;
    while (std::cout << "nex$ " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        auto segments = split_pipeline(line);
        std::vector<std::vector<std::string>> commands;
        bool invalid = false;
        for (const auto& segment : segments) {
            auto args = tokenize(segment);
            if (args.empty()) { invalid = true; break; }
            commands.push_back(std::move(args));
        }
        if (invalid) { std::cerr << "nex: invalid empty command in pipeline\n"; continue; }

        if (commands.size() == 1 && commands[0][0] == "exit") break;
        if (commands.size() == 1 && commands[0][0] == "cd") {
            const std::string target = commands[0].size() > 1 ? commands[0][1] : std::getenv("HOME");
            if (target.empty() || ::chdir(target.c_str()) != 0) perror("nex: cd");
            continue;
        }
        if (commands.size() == 1 && commands[0][0] == "help") {
            std::cout << "Built-ins: cd [dir], help, exit\n"
                      << "Run external programs and connect them with '|'.\n"
                      << "Example: ls -l | grep cpp | wc -l\n";
            continue;
        }

        try {
            std::vector<nexos::Pipe> pipes;
            for (std::size_t i = 1; i < commands.size(); ++i) pipes.emplace_back();
            std::vector<int> all_fds;
            for (const auto& pipe : pipes) {
                all_fds.push_back(pipe.read_fd());
                all_fds.push_back(pipe.write_fd());
            }

            std::vector<nexos::Process> children;
            for (std::size_t i = 0; i < commands.size(); ++i) {
                const int input = i == 0 ? -1 : pipes[i - 1].read_fd();
                const int output = i + 1 == commands.size() ? -1 : pipes[i].write_fd();
                children.push_back(nexos::Process::spawn(commands[i], input, output, all_fds));
            }
            for (auto& pipe : pipes) { pipe.close_read(); pipe.close_write(); }
            int last_status = 0;
            for (auto& child : children) last_status = child.wait().exit_code;
            if (last_status != 0) std::cerr << "nex: command exited with status " << last_status << '\n';
        } catch (const std::exception& error) {
            std::cerr << "nex: " << error.what() << '\n';
        }
    }
    return 0;
}
