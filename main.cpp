// main.cpp - the real entry point; wires every role together.
//
// Build:  make            (see Makefile)   ->  ./csopesy
#include "BatchGenerator.h"
#include "Config.h"
#include "Console.h"
#include "Report.h"
#include "Scheduler.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <memory>

int main() {
    // Declaration order matters: gen is destroyed before sched on exit.
    Config cfg;
    std::unique_ptr<Scheduler> sched;
    std::unique_ptr<BatchGenerator> gen;

    ConsoleHooks h;

    h.initialize = [&]() -> bool {
        std::string err;
        if (!loadConfig("config.txt", cfg, err)) {
            std::cout << err << "\n";
            return false;
        }
        auto cap = [](std::uint64_t v) { return static_cast<int>(std::min<std::uint64_t>(v, INT_MAX)); };
        sched = std::make_unique<Scheduler>(cfg.numCpu, cfg.roundRobin,
                                            cap(cfg.quantumCycles), cap(cfg.delayPerExec));
        sched->start();
        gen = std::make_unique<BatchGenerator>(*sched, cfg);
        return true;
    };

    h.createProcess = [&](const std::string& name) {
        auto p = std::make_shared<Process>(name, generateProgram(name, cfg.minIns, cfg.maxIns));
        sched->addProcess(p);
        return p;
    };

    h.findProcess = [&](const std::string& name) { return sched->findProcess(name); };

    h.schedulerStart = [&] {
        if (gen->start()) std::cout << "Scheduler started: generating a process every "
                                    << cfg.batchProcessFreq << " CPU tick(s).\n\n";
        else std::cout << "Scheduler is already generating processes.\n\n";
    };
    h.schedulerStop = [&] {
        if (gen->stop()) std::cout << "Scheduler stopped generating processes.\n\n";
        else std::cout << "Scheduler is not running.\n\n";
    };

    h.screenLs = [&] { std::cout << buildReport(*sched) << "\n"; };

    h.reportUtil = [&] {
        std::string path = saveReport(*sched);
        if (path.empty()) std::cout << "Could not write csopesy-log.txt.\n\n";
        else std::cout << "Report generated at " << path << "!\n\n";
    };

    Console(std::move(h)).run();
}