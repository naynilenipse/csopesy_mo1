#pragma once
// Process.h
// One emulated process: its identity, its program, its variables and its
// PRINT log, plus the interpreter that runs the program one instruction per
// CPU step.
//
// Threads
//   The core running the process is the only caller of executeStep(); the
//   scheduler never hands one process to two cores at once, so interpreter
//   state needs no lock. Progress, core, state and logs may be read from any
//   thread (console, screen -ls, report-util) while the process runs.

#include "Program.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

class Process {
public:
    enum class State : std::uint8_t { Ready, Running, Sleeping, Finished };

    // One PRINT result. The message is rebuilt from the text id when shown,
    // so a log line costs 16 bytes no matter how long the message is.
    struct LogEntry {
        std::time_t time;
        std::uint32_t textId;
        std::uint16_t value;   // variable value at the time of the PRINT
        std::int8_t core;
        bool hasValue;
    };

    // IDs are handed out in creation order starting at 1.
    Process(std::string name, Program program);

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // ---- Scheduler interface -------------------------------------------
    // Called by the Scheduler's tick loop and core threads.

    // Runs the next instruction. `tick` is the current CPU tick.
    void executeStep(std::uint64_t tick);
    bool isFinished() const { return state_.load(std::memory_order_acquire) == State::Finished; }
    // True while a SLEEP is still running at `tick`. Flips the state back to
    // Ready once it has expired.
    bool isSleeping(std::uint64_t tick) const;

    // The scheduler calls assignCore(i) when core i picks the process up and
    // releaseCore() whenever it leaves that core (finished, sleeping or
    // preempted). PRINT logs and screen -ls read the core from here.
    void assignCore(int core);
    void releaseCore();

    std::string getName() const { return name_; }

    // ---- Read-only views ------------------------------------------------
    // getInstructionsDone / getTotalInstructions / getCreatedAt feed the
    // scheduler's getRunningProcesses() and getFinishedProcesses().
    int getId() const { return id_; }
    State getState() const { return state_.load(std::memory_order_acquire); }
    int getCore() const { return core_.load(std::memory_order_acquire); }  // -1 when off-core
    std::uint64_t getInstructionsDone() const { return executed_.load(std::memory_order_acquire); }
    std::uint64_t getTotalInstructions() const { return totalLines_; }
    std::string getCreatedAt() const { return formatTime(createdAt_); }    // MM/DD/YYYY HH:MM:SSAM
    std::time_t getCreatedTime() const { return createdAt_; }               // raw, for sorting

    std::vector<LogEntry> getLogs() const;            // snapshot copy
    std::string formatLog(const LogEntry& e) const;   // (MM/DD/YYYY HH:MM:SSAM) Core:0 "msg"

    // Full process-smi block:
    //   Process name / ID / Logs, then either the current instruction line and
    //   lines of code, or "Finished!" once the program is done.
    //
    // CONNECT (screen): print this for "process-smi" inside a process screen.
    // Delete this note once the screen command calls it.
    std::string smiReport() const;

    // "MM/DD/YYYY HH:MM:SSAM" in local time. Shared with screen -ls / report-util.
    static std::string formatTime(std::time_t t);

private:
    struct Loop {
        std::uint32_t bodyStart;
        std::uint32_t remaining;
    };

    std::uint16_t read(const Operand& o) const { return o.isVar ? vars_[o.value] : o.value; }
    void skipLoopMarkers();  // moves ip_ to the next leaf instruction or the end
    void finish();

    // Identity
    const int id_;
    const std::string name_;
    const std::time_t createdAt_;
    const std::uint64_t totalLines_;

    // Interpreter state (touched only by the running core)
    std::vector<Instruction> code_;
    std::vector<std::uint16_t> vars_;  // all variables start at 0
    std::array<Loop, ProgramBuilder::kMaxLoopDepth> loops_{};
    int loopDepth_ = 0;
    std::size_t ip_ = 0;

    // Shared with readers
    const std::vector<std::string> texts_;  // immutable PRINT messages
    std::atomic<std::uint64_t> executed_{0};
    std::atomic<std::uint64_t> wakeTick_{0};
    std::atomic<int> core_{-1};
    mutable std::atomic<State> state_{State::Ready};

    mutable std::mutex logMutex_;
    std::vector<LogEntry> logs_;
};
