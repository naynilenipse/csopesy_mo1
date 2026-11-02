#pragma once
#include <string>

class MockProcess {
    public: 
        MockProcess(std::string name, int count)
            : name(name), count(count) {}

    

    void executeStep(int currentTick) {
        count--;
        if (count == sleepAt) {
            wakeAt = currentTick + 3;
        }
    }

    bool isFinished() const {
        return count <= 0;
    }

    bool isSleeping(int currentTick) const {
        return wakeAt > currentTick;
    }

    void setSleep(int sleepAt) {
        this->sleepAt = sleepAt;
    }

    std::string getName() const {
        return name;
    }

    private:
        std::string name;
        int count;
        int sleepAt = -1;
        int wakeAt = -1;
};