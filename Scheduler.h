#pragma once
#include "MockProcess.h"
#include <deque>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>

struct Core {
    std::shared_ptr<MockProcess> proc = nullptr;
    int quantumUsed = 0;
    int delayLeft = 0;
};

struct ProcessInfo {
    std::string name;
    int coreId;
    int done;
    int total;
    std::string createdAt;
};

class Scheduler {
public:
    Scheduler(int numCPU, bool isRR, int quantum, int delayPerExec);
    void addProcess(std::shared_ptr<MockProcess> process);
    
    void start();
    void stop();

    int getCoresUsed();
    int getCoresAvailable();
    int getCpuUsage();

    std::vector<ProcessInfo> getRunningProcesses();
    std::vector<ProcessInfo> getFinishedProcesses();
    
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
    
    std::deque<std::shared_ptr<MockProcess>> readyQueue;
    std::vector<std::shared_ptr<MockProcess>> finished;
    std::vector<std::shared_ptr<MockProcess>> sleeping;
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