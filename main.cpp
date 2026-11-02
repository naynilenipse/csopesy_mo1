#include "Scheduler.h"
#include <iostream>

int main() {
    Scheduler s(2, false, 5, 0);
    s.addProcess(std::make_shared<MockProcess>("p01", 3));
    s.addProcess(std::make_shared<MockProcess>("p02", 8));
    s.addProcess(std::make_shared<MockProcess>("p03", 8));
    s.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    std::cout << "Running processes:\n";
    for (const ProcessInfo& p : s.getRunningProcesses())
        std::cout << p.name << "  (" << p.createdAt << ")  Core: " << p.coreId
                << "  " << p.done << " / " << p.total << "\n";

    std::cout << "\nFinished processes:\n";
    for (const ProcessInfo& p : s.getFinishedProcesses())
        std::cout << p.name << "  (" << p.createdAt << ")  Finished  "
                << p.done << " / " << p.total << "\n";
}