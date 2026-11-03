// For testing console and screen :P

// Build: g++ -std=c++17 -pthread console_test_main.cpp Console.cpp Scheduler.cpp Process.cpp Program.cpp -o app

#include "Console.h"
#include "Scheduler.h"

#include <iostream>
#include <memory>

int main(){
    std::unique_ptr<Scheduler> sched;

    ConsoleHooks h;
    h.initialize = [&] {
        sched = std::make_unique<Scheduler>(4, true, 5, 0);  //  placeholder for config.txt
        sched->start();
        return true;
    };
    h.createProcess = [&](const std::string& name) {
        auto p = std::make_shared<Process>(name, generateProgram(name, 100, 200));  // placeholder for min/max-ins from config
        sched->addProcess(p);
        return p;
    };
    h.findProcess = [&](const std::string& name) { return sched->findProcess(name); };
    h.schedulerStart = [] { std::cout << "(stub) scheduler-start\n"; };
    h.schedulerStop = [] { std::cout << "(stub) scheduler-stop\n"; };
    h.screenLs = [&] {
        std::cout << "CPU utilization: " << sched->getCpuUsage() << "%\n"
                  << "Cores used: " << sched->getCoresUsed() << "\n"
                  << "Cores available: " << sched->getCoresAvailable() << "\n\n";
    };
    h.reportUtil = [] { std::cout << "(stub) report-util\n"; };

    Console(std::move(h)).run();
}