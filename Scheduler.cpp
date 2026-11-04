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

    void Scheduler::assignCores() {
        for (size_t i = 0; i < cores.size(); i++) {
            Core& core = cores[i];
            if (core.proc == nullptr && !readyQueue.empty()) {
                core.proc = readyQueue.front();
                readyQueue.pop_front();
                core.proc->assignCore(static_cast<int>(i));
                core.quantumUsed = 0; 
                auto it = savedDelay.find(core.proc);
                if (it != savedDelay.end()) {
                    core.delayLeft = it->second;
                    savedDelay.erase(it);
                } else {
                    core.delayLeft = 0;
                }
            }
        }
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

        assignCores();

        coresDone = 0;
        goTick = tick;
        cvGo.notify_all();
        cvDone.wait(lock, [&] { return coresDone == numCPU; });

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
            else if (core.proc != nullptr && isRR && core.quantumUsed >= quantum && !readyQueue.empty()) {
                core.proc->releaseCore();
                readyQueue.push_back(core.proc);
                if (core.delayLeft > 0) {
                    savedDelay[core.proc] = core.delayLeft;
                }
                core.proc = nullptr;
                core.quantumUsed = 0;
                core.delayLeft = 0;
            }
        }
        assignCores();
        tick++;
    }

    void Scheduler::loop () { 
        while (running) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                runOneTick(lock);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

    int Scheduler::getTick() {
        std::lock_guard<std::mutex> lock(mtx);
        return tick;
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

    SchedulerSnapshot Scheduler::getSnapshot() {
        std::lock_guard<std::mutex> lock(mtx);
        SchedulerSnapshot snapshot;
        snapshot.tick = tick;
        snapshot.coresUsed = countBusyCores();
        snapshot.coresAvailable = numCPU - snapshot.coresUsed;
        snapshot.cpuUsage = countBusyCores() * 100 / numCPU;
        
        for (size_t i = 0; i < cores.size(); i++) {
            if (cores[i].proc != nullptr) {
                ProcessInfo info;
                info.name = cores[i].proc->getName();
                info.coreId = i;
                info.done = cores[i].proc->getInstructionsDone();
                info.total = cores[i].proc->getTotalInstructions();
                info.createdAt = cores[i].proc->getCreatedAt();
                snapshot.runningProcesses.push_back(info);
            }
        }
        for (const auto& proc : finished) {
            ProcessInfo info;
            info.name = proc->getName();
            info.coreId = -1; 
            info.done = proc->getInstructionsDone();
            info.total = proc->getTotalInstructions();
            info.createdAt = proc->getCreatedAt();
            snapshot.finishedProcesses.push_back(info);
        }
        for (const auto& proc : readyQueue) {
            ProcessInfo info;
            info.name = proc->getName();
            info.coreId = -1; 
            info.done = proc->getInstructionsDone();
            info.total = proc->getTotalInstructions();
            info.createdAt = proc->getCreatedAt();
            snapshot.readyProcesses.push_back(info);
        }
        for (const auto& proc : sleeping) {
            ProcessInfo info;
            info.name = proc->getName();
            info.coreId = -1; 
            info.done = proc->getInstructionsDone();
            info.total = proc->getTotalInstructions();
            info.createdAt = proc->getCreatedAt();
            snapshot.sleepingProcesses.push_back(info);
        }

        return snapshot;
    }

// --- added for console testing
std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const Core& c : cores)
        if (c.proc && c.proc->getName() == name) return c.proc;
    for (const auto& p : readyQueue)
        if (p->getName() == name) return p;
    for (const auto& p : sleeping)
        if (p->getName() == name) return p;
    for (const auto& p : finished)
        if (p->getName() == name) return p;
    return nullptr;
}
// ---

Scheduler::~Scheduler() {
    stop();
}

