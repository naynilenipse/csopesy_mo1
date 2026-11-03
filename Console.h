// Console.h for console and screen
#pragma once

#include "Process.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Console owns no scheduler and no config; everything needed from other roles come in through ConsoleHooks
struct ConsoleHooks{
    // read config.txt, build, and start Scheduler
    // if config is bad, return false and print why (console stays uninitialized)
    std::function<bool()> initialize;

    // start or stop batch generator thread
    std::function<void()> schedulerStart;
    std::function<void()> schedulerStop;

    // print CPU utilization and running/finished summary
    // save it to csopesy-log.txt
    std::function<void()> screenLs;
    std::function<void()> reportUtil;

    // finds process by name in any state, returns nullptr if it does not exist
    std::function<std::shared_ptr<Process>(const std::string&)> findProcess;

    // builds process from generateProgram(), submitted to scheduler's ready queue, then returned
    std::function<std::shared_ptr<Process>(const std::string&)> createProcess;
};

class Console{
    public:
    explicit Console(ConsoleHooks hooks) : hooks_(std::move(hooks)) {}

    void run();

    private:
    static constexpr const char* kPrompt = "root:\\>";

    bool mainCommand(const std::vector<std::string>& args);
    void screen(const std::vector<std::string>& args);
    void handleInitialize();

    void enterScreen(const std::shared_ptr<Process>& proc);

    static std::vector<std::string> tokenize(const std::string& line);
    static void clearScreen();
    static void printBanner();
 
    ConsoleHooks hooks_;
    bool initialized_ = false;
};