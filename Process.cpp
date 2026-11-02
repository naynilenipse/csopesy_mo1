// Process.cpp
// The instruction interpreter and the process's text output.

#include "Process.h"

#include <utility>

namespace {
std::atomic<int> nextId{1};
constexpr std::uint32_t kMaxU16 = 65535;
}  // namespace

Process::Process(std::string name, Program program)
    : id_(nextId.fetch_add(1, std::memory_order_relaxed)),
      name_(std::move(name)),
      createdAt_(std::time(nullptr)),
      totalLines_(program.totalLines),
      code_(std::move(program.code)),
      vars_(program.varNames.size(), 0),
      texts_(std::move(program.texts)) {
    skipLoopMarkers();
    if (ip_ >= code_.size()) finish();  // empty program
}

void Process::executeStep(std::uint64_t tick) {
    if (ip_ >= code_.size()) return;
    const Instruction& in = code_[ip_];

    switch (in.op) {
        case Op::Print: {
            LogEntry e{std::time(nullptr), in.arg, 0,
                       static_cast<std::int8_t>(core_.load(std::memory_order_relaxed)), in.lhs.isVar};
            if (e.hasValue) e.value = vars_[in.lhs.value];
            std::lock_guard<std::mutex> lock(logMutex_);
            logs_.push_back(e);
            break;
        }
        case Op::Declare:
            vars_[in.dst] = in.lhs.value;
            break;
        case Op::Add: {
            std::uint32_t sum = std::uint32_t{read(in.lhs)} + read(in.rhs);
            vars_[in.dst] = static_cast<std::uint16_t>(sum > kMaxU16 ? kMaxU16 : sum);
            break;
        }
        case Op::Subtract: {
            std::uint16_t a = read(in.lhs), b = read(in.rhs);
            vars_[in.dst] = a > b ? static_cast<std::uint16_t>(a - b) : 0;
            break;
        }
        case Op::Sleep:
            // Off the CPU for ticks tick+1 .. tick+X; ready again at tick+X+1.
            if (in.arg > 0) {
                wakeTick_.store(tick + in.arg + 1, std::memory_order_release);
                state_.store(State::Sleeping, std::memory_order_release);
            }
            break;
        case Op::ForBegin:
        case Op::ForEnd:
            break;  // never reached: skipLoopMarkers() steps over these
    }

    ++ip_;
    skipLoopMarkers();
    executed_.fetch_add(1, std::memory_order_release);
    if (ip_ >= code_.size()) finish();
}

void Process::skipLoopMarkers() {
    while (ip_ < code_.size()) {
        const Instruction& in = code_[ip_];
        if (in.op == Op::ForBegin) {
            loops_[loopDepth_++] = Loop{static_cast<std::uint32_t>(ip_ + 1), in.arg};
            ++ip_;
        } else if (in.op == Op::ForEnd) {
            Loop& loop = loops_[loopDepth_ - 1];
            if (--loop.remaining > 0) {
                ip_ = loop.bodyStart;
            } else {
                --loopDepth_;
                ++ip_;
            }
        } else {
            return;
        }
    }
}

void Process::finish() {
    // Variables and code live until the process ends; texts stay for the logs.
    std::vector<Instruction>().swap(code_);
    std::vector<std::uint16_t>().swap(vars_);
    ip_ = 0;
    finishedAt_.store(std::time(nullptr), std::memory_order_release);
    state_.store(State::Finished, std::memory_order_release);
}

bool Process::isSleeping(std::uint64_t tick) const {
    State s = state_.load(std::memory_order_acquire);
    if (s != State::Sleeping) return false;
    if (wakeTick_.load(std::memory_order_acquire) > tick) return true;
    state_.compare_exchange_strong(s, State::Ready, std::memory_order_acq_rel);
    return false;
}

void Process::assignCore(int core) {
    core_.store(core, std::memory_order_release);
    State s = State::Ready;
    state_.compare_exchange_strong(s, State::Running, std::memory_order_acq_rel);
}

void Process::releaseCore() {
    core_.store(-1, std::memory_order_release);
    State s = State::Running;
    state_.compare_exchange_strong(s, State::Ready, std::memory_order_acq_rel);
}

std::vector<Process::LogEntry> Process::getLogs() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return logs_;
}

std::string Process::formatTime(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%m/%d/%Y %I:%M:%S%p", &tm);
    return buf;
}

std::string Process::formatLog(const LogEntry& e) const {
    std::string line;
    line.reserve(64);
    line += '(';
    line += formatTime(e.time);
    line += ") Core:";
    line += std::to_string(e.core);
    line += " \"";
    line += texts_[e.textId];
    if (e.hasValue) line += std::to_string(e.value);
    line += '"';
    return line;
}

std::string Process::smiReport() const {
    // Read progress before the logs so the line count never runs ahead of them.
    bool finished = isFinished();
    std::uint64_t line = getCurrentLine();
    std::vector<LogEntry> logs = getLogs();

    std::string out;
    out.reserve(64 + logs.size() * 64);
    out += "Process name: " + name_ + "\n";
    out += "ID: " + std::to_string(id_) + "\n";
    out += "Logs:\n";
    for (const LogEntry& e : logs) {
        out += formatLog(e);
        out += '\n';
    }
    out += '\n';
    if (finished) {
        out += "Finished!\n";
    } else {
        out += "Current instruction line: " + std::to_string(line) + "\n";
        out += "Lines of code: " + std::to_string(totalLines_) + "\n";
    }
    return out;
}
