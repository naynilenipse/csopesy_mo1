// Report.cpp  (Role D)
#include "Report.h"

#include <filesystem>
#include <fstream>
#include <sstream>

std::string buildReport(Scheduler& sched) {
    // Snapshot lists first so the numbers and tables agree as closely as possible.
    int used = sched.getCoresUsed();
    int avail = sched.getCoresAvailable();
    int usage = sched.getCpuUsage();
    auto running = sched.getRunningProcesses();
    auto finished = sched.getFinishedProcesses();

    std::ostringstream o;
    o << "CPU utilization: " << usage << "%\n"
      << "Cores used: " << used << "\n"
      << "Cores available: " << avail << "\n\n"
      << "----------------------------------------\n"
      << "Running processes:\n";
    for (const ProcessInfo& p : running)
        o << p.name << "  (" << p.createdAt << ")  Core: " << p.coreId << "  "
          << p.done << " / " << p.total << "\n";
    o << "\nFinished processes:\n";
    for (const ProcessInfo& p : finished)
        o << p.name << "  (" << p.createdAt << ")  Finished  "
          << p.done << " / " << p.total << "\n";
    o << "----------------------------------------\n";
    return o.str();
}

std::string saveReport(Scheduler& sched, const std::string& file) {
    std::ofstream out(file, std::ios::trunc);
    if (!out) return "";
    out << buildReport(sched);
    out.close();
    std::error_code ec;
    auto abs = std::filesystem::absolute(file, ec);
    return ec ? file : abs.string();
}
