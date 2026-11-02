#pragma once
#include <string>
#include <ctime>

class MockProcess {
    public: 
        MockProcess(std::string name, int count)
            : name(name), count(count), total(count), createdAt(makeTimestamp()) {}

    

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

    int getTotalInstructions() const {
        return total;
    }

    int getInstructionsDone() const {
        return total - count;
    }

    std::string getCreatedAt() const {
        char buf[32];
        std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", std::localtime(&createdAt));
        return buf;
    }    
    private:
        std::string name;
        int count;
        int sleepAt = -1;
        int wakeAt = -1;
        int total;
        std::time_t createdAt;

        static std::time_t makeTimestamp() {
            return std::time(nullptr);
        }
};