#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

struct Entry {
    int pid = 0;
    int ppid = 0;
    char state = '?';
    unsigned long long user_ticks = 0;
    unsigned long long system_ticks = 0;
    long rss_pages = 0;
    std::string command;
};

bool read_process(const fs::path& proc_dir, Entry& entry) {
    const auto pid_text = proc_dir.filename().string();
    try { entry.pid = std::stoi(pid_text); } catch (...) { return false; }
    std::ifstream stat(proc_dir / "stat");
    if (!stat) return false;
    std::string line;
    std::getline(stat, line);
    const auto open = line.find('(');
    const auto close = line.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) return false;
    entry.command = line.substr(open + 1, close - open - 1);
    std::istringstream fields(line.substr(close + 2));
    long long ppid = 0;
    long long ignored = 0;
    if (!(fields >> entry.state >> ppid)) return false;
    entry.ppid = static_cast<int>(ppid);
    // Fields 5 through 13 are skipped; utime/stime are fields 14/15.
    for (int i = 0; i < 9; ++i) if (!(fields >> ignored)) return false;
    if (!(fields >> entry.user_ticks >> entry.system_ticks)) return false;
    // Skip cutime, cstime, priority, nice, num_threads, itrealvalue, starttime, vsize.
    for (int i = 0; i < 8; ++i) if (!(fields >> ignored)) return false;
    if (!(fields >> entry.rss_pages)) return false;
    return true;
}

void display() {
    std::vector<Entry> entries;
    std::error_code ec;
    for (const auto& item : fs::directory_iterator("/proc", ec)) {
        if (!item.is_directory(ec)) continue;
        Entry entry;
        if (read_process(item.path(), entry)) entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.pid < b.pid; });
    const long page_kb = ::sysconf(_SC_PAGESIZE) / 1024;
    std::cout << "\033[2J\033[H";
    std::cout << "NEXOS PROCESS MONITOR  |  " << entries.size() << " processes\n";
    std::cout << std::left << std::setw(8) << "PID" << std::setw(8) << "PPID"
              << std::setw(8) << "STATE" << std::setw(12) << "RSS(KB)"
              << std::setw(14) << "CPU TICKS" << "COMMAND\n";
    std::cout << std::string(78, '-') << '\n';
    for (const auto& e : entries) {
        const auto rss_kb = e.rss_pages * page_kb;
        std::cout << std::left << std::setw(8) << e.pid << std::setw(8) << e.ppid
                  << std::setw(8) << e.state << std::setw(12) << rss_kb
                  << std::setw(14) << (e.user_ticks + e.system_ticks) << e.command << '\n';
    }
    std::cout << "\nRefreshes every second. Press Ctrl+C to exit.\n" << std::flush;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--once") {
        display();
        return 0;
    }
    for (;;) {
        display();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
