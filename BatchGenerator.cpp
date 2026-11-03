// BatchGenerator.cpp  (Role D)
#include "BatchGenerator.h"

#include <chrono>
#include <cstdio>

bool BatchGenerator::start() {
    if (running_) return false;
    if (thread_.joinable()) thread_.join();
    running_ = true;
    thread_ = std::thread(&BatchGenerator::loop, this);
    return true;
}

bool BatchGenerator::stop() {
    bool was = running_.exchange(false);
    if (thread_.joinable()) thread_.join();
    return was;
}

void BatchGenerator::loop() {
    long long lastSpawn = sched_.getTick();
    while (running_) {
        long long now = sched_.getTick();
        if (now - lastSpawn >= static_cast<long long>(cfg_.batchProcessFreq)) {
            lastSpawn = now;

            // Next free name; skip any a user already took with "screen -s p07".
            std::string name;
            do {
                char buf[16];
                std::snprintf(buf, sizeof buf, "p%02d", nextNumber_++);
                name = buf;
            } while (sched_.findProcess(name));

            sched_.addProcess(std::make_shared<Process>(
                name, generateProgram(name, cfg_.minIns, cfg_.maxIns)));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
