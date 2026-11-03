// BatchGenerator.h  (Role D)
// scheduler-start / scheduler-stop: every batch-process-freq CPU ticks, one new
// dummy process (p01, p02, ...) is created and put in the ready queue.
#pragma once

#include "Config.h"
#include "Scheduler.h"

#include <atomic>
#include <thread>

class BatchGenerator {
public:
    BatchGenerator(Scheduler& sched, const Config& cfg) : sched_(sched), cfg_(cfg) {}
    ~BatchGenerator() { stop(); }

    bool start();                  // false if already running
    bool stop();                   // false if not running
    bool isRunning() const { return running_; }

private:
    void loop();

    Scheduler& sched_;
    Config cfg_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int nextNumber_ = 1;           // names continue across stop/start: p01, p02, ...
};
