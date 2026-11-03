#pragma once
#include "Process.h"
#include <deque>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <cstdint>

struct Core {
    std::shared_ptr<Process> proc = nullptr;
    int quantumUsed = 0;
    int delayLeft = 0;
};

struct ProcessInfo {
    std::string name;
    int coreId;
    std::uint64_t done;
    std::uint64_t total;
    std::string createdAt;
};

class Scheduler {
public:
    Scheduler(int numCPU, bool isRR, int quantum, int delayPerExec);
    void addProcess(std::shared_ptr<Process> process);
    
    void start();
    void stop();

    int getCoresUsed();
    int getCoresAvailable();
    int getCpuUsage();

    std::vector<ProcessInfo> getRunningProcesses();
    std::vector<ProcessInfo> getFinishedProcesses();
    // --- added for console testing
    std::shared_ptr<Process> findProcess(const std::string& name);
    // ----
    
    ~Scheduler();
    
    private:
    int numCPU = 1;
    bool isRR = false;
    int quantum = 1;
    int delayPerExec = 0;
    int tick = 0;
    int goTick = -1;
    int coresDone = 0;
    
    std::vector<Core> cores;
    
    std::deque<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> finished;
    std::vector<std::shared_ptr<Process>> sleeping;
    std::vector<std::thread> coreThreads;
    
    std::atomic<bool> running{false};
    std::thread schedulerThread;
    std::mutex mtx;

    std::condition_variable cvGo;
    std::condition_variable cvDone;
    
    void printStatus();
    void runOneTick(std::unique_lock<std::mutex>& lock);
    void loop();
    void runCore(Core& core);
    void coreWorker(int id);

    int countBusyCores();
};