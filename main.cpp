#include "Scheduler.h"
#include <iostream>

int main() {
    Scheduler s(4, false, 5, 0);
    s.addProcess(std::make_shared<Process>("p01", generateProgram("p01", 3, 3)));
    s.addProcess(std::make_shared<Process>("p02", generateProgram("p02", 6, 6)));
    s.addProcess(std::make_shared<Process>("p03", generateProgram("p03", 6, 6)));
    s.start();
    for (int i = 0; i < 8; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "  -> used " << s.getCoresUsed()
                << ", available " << s.getCoresAvailable()
                << ", util " << s.getCpuUsage() << "%\n";
    }
    s.stop();
}