#include "Scheduler.h"
#include <iostream>
#include <chrono>

Scheduler::Scheduler(int numCPU, bool isRR, int quantum, int delayPerExec)
    : numCPU(numCPU), isRR(isRR), quantum(quantum), delayPerExec(delayPerExec) 
    {
        cores.resize(numCPU);
    }

    void Scheduler::addProcess(std::shared_ptr<Process> process) {
        std::lock_guard<std::mutex> lock(mtx);
        readyQueue.push_back(process);
    }

    void Scheduler::runOneTick(std::unique_lock<std::mutex>& lock) {

        for (size_t t= 0; t < sleeping.size();) {
            if (!sleeping[t]->isSleeping(tick)) {
                readyQueue.push_back(sleeping[t]);
                sleeping.erase(sleeping.begin() + t);
            } else {
                t++;
            }
        }

        for (size_t i = 0; i < cores.size(); i++) {
            Core& core = cores[i];
            if (core.proc == nullptr && !readyQueue.empty()) {
                core.proc = readyQueue.front();
                readyQueue.pop_front();
                core.proc->assignCore(static_cast<int>(i));
                core.quantumUsed = 0; 
                core.delayLeft = 0;
            }
        }

        coresDone = 0;
        goTick = tick;
        cvGo.notify_all();
        cvDone.wait(lock, [&] { return coresDone == numCPU; });

        //printStatus(); debugging onlyyyy roar

        for (Core& core : cores) {
            if (core.proc != nullptr && core.proc->isFinished()) {
                core.proc->releaseCore();
                finished.push_back(core.proc);
                core.proc = nullptr;
                core.quantumUsed = 0;
                core.delayLeft = 0;
            }
            else if (core.proc != nullptr && core.proc->isSleeping(tick)) {
                core.proc->releaseCore();
                sleeping.push_back(core.proc);
                core.proc = nullptr;
                core.quantumUsed = 0;
                core.delayLeft = 0;
            }
            else if (core.proc != nullptr && isRR && core.quantumUsed >= quantum) {
                core.proc->releaseCore();
                readyQueue.push_back(core.proc);
                core.proc = nullptr;
                core.quantumUsed = 0;
                core.delayLeft = 0;
            }
        }
        tick++;
    }

    void Scheduler::loop () { 
        while (running) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                runOneTick(lock);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void Scheduler::start() {
        running = true;
        for (int i = 0; i < numCPU; i++) {
            coreThreads.emplace_back(&Scheduler::coreWorker, this, i);
        }
        schedulerThread = std::thread(&Scheduler::loop, this);
    }

    void Scheduler::stop() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            running = false;
        }
        if (schedulerThread.joinable()) {
            schedulerThread.join();
        }
        cvGo.notify_all(); //Wake up any waiting core threads
        for (std::thread& t : coreThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
        coreThreads.clear();
    }

    void Scheduler::runCore(Core& core) {
        if (core.proc != nullptr) {
                if (core.delayLeft > 0) {
                    core.delayLeft--;
                } else {
                    core.proc->executeStep(tick);
                    core.delayLeft = delayPerExec;
                }
                core.quantumUsed++;
            }
    }

    void Scheduler::coreWorker(int id) {
        int lastTick = -1;
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);

           //Wait for the scheduler to signal that it's time to run a tick
            cvGo.wait(lock, [&] { return goTick != lastTick || !running;});

            //If the scheduler has stopped, exit the loop
            if (goTick == lastTick) {
                return;
            }
            lastTick = goTick;

            lock.unlock();
            runCore(cores[id]);
            lock.lock();

            coresDone++;
            if (coresDone == numCPU) {
                cvDone.notify_one(); //Notify the scheduler that all cores are done
            }
        }
    }

    int Scheduler::countBusyCores() {
        int count = 0;
        for (const Core& core : cores) {
            if (core.proc != nullptr) {
                count++;
            }
        }
        return count;
    }

    int Scheduler::getCoresUsed() {
        std::lock_guard<std::mutex> lock(mtx);
        return countBusyCores();
    }

    int Scheduler::getCoresAvailable() {
        std::lock_guard<std::mutex> lock(mtx);
        return numCPU - countBusyCores();
    }

    int Scheduler::getCpuUsage() {
        std::lock_guard<std::mutex> lock(mtx);
        return countBusyCores() * 100 / numCPU;
    }

    std::vector<ProcessInfo> Scheduler::getRunningProcesses() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<ProcessInfo> result;
        for (size_t i = 0; i < cores.size(); i++) {
            if (cores[i].proc != nullptr) {
                ProcessInfo info;
                info.name = cores[i].proc->getName();
                info.coreId = i;
                info.done = cores[i].proc->getInstructionsDone();
                info.total = cores[i].proc->getTotalInstructions();
                info.createdAt = cores[i].proc->getCreatedAt();
                result.push_back(info);
            }
        }
        return result;
    }

    std::vector<ProcessInfo> Scheduler::getFinishedProcesses() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<ProcessInfo> result;
        for (const auto& proc : finished) {
            ProcessInfo info;
            info.name = proc->getName();
            info.coreId = -1; 
            info.done = proc->getInstructionsDone();
            info.total = proc->getTotalInstructions();
            info.createdAt = proc->getCreatedAt();
            result.push_back(info);
        }
        return result;
    }
/*
    void Scheduler::printStatus() {
    std::cout << "Tick " << tick << ": ";

    for (size_t t = 0; t < cores.size(); t++) {
        std::cout << "Core " << t << "=";
        if (cores[t].proc != nullptr) {
            std::cout << cores[t].proc->getName();
        } else {
            std::cout << "idle";
        }
        std::cout << "  ";
    }

    std::cout << "\n";
}
*/
Scheduler::~Scheduler() {
    stop();
}