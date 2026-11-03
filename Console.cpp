// Console.cpp for console and screen
#include "Console.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

std::vector<std::string> Console::tokenize(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}
 
void Console::clearScreen(){
#ifdef _WIN32
    std::system("cls");
#else
    std::cout << "\033[2J\033[3J\033[H" << std::flush;
#endif
}
 
void Console::printBanner(){
    std::cout << R"BANNER(
  ____ ____   ___  ____  _____ ______   __
 / ___/ ___| / _ \|  _ \| ____/ ___\ \ / /
| |   \___ \| | | | |_) |  _| \___ \\ V /
| |___ ___) | |_| |  __/| |___ ___) || |
 \____|____/ \___/|_|   |_____|____/ |_|
)BANNER";
    std::cout << "----------------------------------------\n"
                 "Welcome to CSOPESY Emulator!\n"
                 "\nDevelopers:\n"
                 "  Espineli, Nyan\n"
                 "  Guarin, Raine\n"
                 "  Montano, Rovin\n"
                 "  Tolentino, Winelle\n"
                 "\nLast updated: 11-03-2026\n"
                 "----------------------------------------\n\n";
}
 
// main loop
void Console::run(){
    clearScreen();
    printBanner();
 
    std::string line;
    while(true){
        std::cout << kPrompt << std::flush;
        if(!std::getline(std::cin, line)) break;
 
        std::vector<std::string> args = tokenize(line);
        if(args.empty()) continue;
        if(!mainCommand(args)) break;
    }
}
 
bool Console::mainCommand(const std::vector<std::string>& args){
    const std::string& cmd = args[0];
 
    // exit works even before initialize
    if(cmd == "exit"){
        if(args.size() != 1) {
            std::cout << "Usage: exit\n";
            return true;
        }
        return false;
    }
 
    // until successfully initialized, everything else is locked
    if(!initialized_){
        if(cmd == "initialize"){
            if (args.size() != 1) std::cout << "Usage: initialize\n\n";
            else handleInitialize();
        } else{
            std::cout << "Please run 'initialize' first.\n\n";
        }
        return true;
    }
 
    if(cmd == "initialize"){
        std::cout << "Already initialized.\n\n";
    } else if(cmd == "screen"){
        screen(args);
    } else if(cmd == "scheduler-start" && args.size() == 1){
        hooks_.schedulerStart();
    } else if(cmd == "scheduler-stop" && args.size() == 1){
        hooks_.schedulerStop();
    } else if(cmd == "report-util" && args.size() == 1){
        hooks_.reportUtil();
    } else if(cmd == "scheduler-start" || cmd == "scheduler-stop" || cmd == "report-util") {
        std::cout << "Usage: " << cmd << "\n";
    } else{
        std::cout << "Unknown command: " << cmd << "\n";
    }
    return true;
}
 
void Console::handleInitialize(){
    if(hooks_.initialize()) {
        initialized_ = true;
        std::cout << "Initialized.\n\n";
    }
}
 
// screen command
void Console::screen(const std::vector<std::string>& args) {
    const char* usage =
        "Usage:\n"
        "  screen -s <process name>\n"
        "  screen -r <process name>\n"
        "  screen -ls\n\n";
 
    if(args.size() == 2 && args[1] == "-ls") {
        hooks_.screenLs();
        return;
    }
 
    if(args.size() == 3 && args[1] == "-s"){
        const std::string& name = args[2];
        if (hooks_.findProcess(name)) {
            std::cout << "Process " << name << " already exists.\n\n";
            return;
        }
        std::shared_ptr<Process> proc = hooks_.createProcess(name);
        if (!proc) {
            std::cout << "Could not create process " << name << ".\n\n";
            return;
        }
        enterScreen(proc);
        return;
    }
 
    if(args.size() == 3 && args[1] == "-r"){
        const std::string& name = args[2];
        std::shared_ptr<Process> proc = hooks_.findProcess(name);

        if (!proc || proc->isFinished()) {
            std::cout << "Process " << name << " not found.\n\n";
            return;
        }
        enterScreen(proc);
        return;
    }
 
    std::cout << usage;
}
 
// process screen
void Console::enterScreen(const std::shared_ptr<Process>& proc){
    clearScreen();
    std::cout << proc->smiReport() << "\n";
 
    std::string line;
    while (true) {
        std::cout << kPrompt << std::flush;
        if (!std::getline(std::cin, line)) break;
 
        std::vector<std::string> args = tokenize(line);
        if (args.empty()) continue;
 
        if (args.size() == 1 && args[0] == "exit") {
            break;
        } else if (args.size() == 1 && args[0] == "process-smi") {
            std::cout << "\n" << proc->smiReport() << "\n";
        } else {
            std::cout << "Unknown command. Available: process-smi, exit\n\n";
        }
    }
 
    clearScreen();
    printBanner();
}