// Report.cpp  (Role D)
#include "Report.h"

#include <filesystem>
#include <fstream>
#include <sstream>

std::string buildReport(Scheduler& sched) {
    // Snapshot lists first so the numbers and tables agree as closely as possible.
    SchedulerSnapshot snap = sched.getSnapshot();
    int used = snap.coresUsed;
    int avail = snap.coresAvailable;
    int usage = snap.cpuUsage;
    auto running = snap.runningProcesses;
    auto finished = snap.finishedProcesses;

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
    o << "\nReady processes:\n";
    for (const ProcessInfo& p : snap.readyProcesses)
        o << p.name << "  (" << p.createdAt << ")  Ready  "
          << p.done << " / " << p.total << "\n";
    o << "\nSleeping processes:\n";
    for (const ProcessInfo& p : snap.sleepingProcesses)
        o << p.name << "  (" << p.createdAt << ")  Sleeping  "
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
