// Program.cpp
// ProgramBuilder and the random program generator.

#include "Program.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <random>
#include <stdexcept>

namespace {

constexpr std::uint64_t kLineCap = std::numeric_limits<std::uint64_t>::max();

// a * b without wrapping past the uint64 limit.
std::uint64_t mulCapped(std::uint64_t a, std::uint64_t b) {
    return (a != 0 && b > kLineCap / a) ? kLineCap : a * b;
}

std::uint64_t addCapped(std::uint64_t a, std::uint64_t b) {
    return (b > kLineCap - a) ? kLineCap : a + b;
}

// Per-thread engine: no locking, and no two threads share a sequence.
std::mt19937_64& rng() {
    thread_local std::mt19937_64 engine(
        std::random_device{}() ^
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return engine;
}

// Uniform integer in [lo, hi].
std::uint64_t pick(std::uint64_t lo, std::uint64_t hi) {
    return std::uniform_int_distribution<std::uint64_t>(lo, hi)(rng());
}

}  // namespace

// ---------------------------------------------------------------- builder

std::uint16_t ProgramBuilder::slot(const std::string& name) {
    auto it = slotOf_.find(name);
    if (it != slotOf_.end()) return it->second;
    if (prog_.varNames.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("too many variables in one program");
    auto id = static_cast<std::uint16_t>(prog_.varNames.size());
    prog_.varNames.push_back(name);
    slotOf_.emplace(name, id);
    return id;
}

std::uint32_t ProgramBuilder::text(const std::string& message) {
    auto it = textOf_.find(message);
    if (it != textOf_.end()) return it->second;
    auto id = static_cast<std::uint32_t>(prog_.texts.size());
    prog_.texts.push_back(message);
    textOf_.emplace(message, id);
    return id;
}

Operand ProgramBuilder::operand(const Value& v) {
    if (v.name.empty()) return Operand{v.literal, false};
    return Operand{slot(v.name), true};
}

void ProgramBuilder::leaf(const Instruction& in) {
    prog_.code.push_back(in);
    lineCount_.back() = addCapped(lineCount_.back(), 1);
}

ProgramBuilder& ProgramBuilder::print(const std::string& message, const std::string& var) {
    Instruction in;
    in.op = Op::Print;
    in.arg = text(message);
    if (!var.empty()) in.lhs = Operand{slot(var), true};
    leaf(in);
    return *this;
}

ProgramBuilder& ProgramBuilder::declare(const std::string& var, int value) {
    Instruction in;
    in.op = Op::Declare;
    in.dst = slot(var);
    in.lhs = operand(Value(value));
    leaf(in);
    return *this;
}

ProgramBuilder& ProgramBuilder::add(const std::string& dst, const Value& lhs, const Value& rhs) {
    Instruction in;
    in.op = Op::Add;
    in.dst = slot(dst);
    in.lhs = operand(lhs);
    in.rhs = operand(rhs);
    leaf(in);
    return *this;
}

ProgramBuilder& ProgramBuilder::subtract(const std::string& dst, const Value& lhs, const Value& rhs) {
    Instruction in;
    in.op = Op::Subtract;
    in.dst = slot(dst);
    in.lhs = operand(lhs);
    in.rhs = operand(rhs);
    leaf(in);
    return *this;
}

ProgramBuilder& ProgramBuilder::sleep(std::uint8_t ticks) {
    Instruction in;
    in.op = Op::Sleep;
    in.arg = ticks;
    leaf(in);
    return *this;
}

ProgramBuilder& ProgramBuilder::beginFor(std::uint32_t repeats) {
    if (static_cast<int>(loopStart_.size()) >= kMaxLoopDepth)
        throw std::logic_error("FOR loops nest at most 3 levels");
    Instruction in;
    in.op = Op::ForBegin;
    in.arg = repeats;
    loopStart_.push_back(static_cast<std::uint32_t>(prog_.code.size()));
    prog_.code.push_back(in);
    lineCount_.push_back(0);
    return *this;
}

ProgramBuilder& ProgramBuilder::endFor() {
    if (loopStart_.empty()) throw std::logic_error("endFor() without beginFor()");
    std::uint32_t start = loopStart_.back();
    std::uint64_t lines = mulCapped(lineCount_.back(), prog_.code[start].arg);
    loopStart_.pop_back();
    lineCount_.pop_back();

    if (lines == 0) {
        prog_.code.resize(start);  // loop never runs anything: drop it entirely
    } else {
        Instruction in;
        in.op = Op::ForEnd;
        prog_.code.push_back(in);
        lineCount_.back() = addCapped(lineCount_.back(), lines);
    }
    return *this;
}

Program ProgramBuilder::build() {
    if (!loopStart_.empty()) throw std::logic_error("build() with an unclosed FOR");
    prog_.totalLines = lineCount_.front();
    Program out = std::move(prog_);
    *this = ProgramBuilder();
    return out;
}

// -------------------------------------------------------------- generator

class ProgramGenerator {
public:
    ProgramGenerator(const std::string& processName, const GeneratorOptions& opt)
        : opt_(opt) {
        int count = std::max(1, opt.varCount);
        static const char* kNames[] = {"x", "y", "z", "w"};
        for (int i = 0; i < count; ++i)
            b_.slot(i < 4 ? kNames[i] : "v" + std::to_string(i));
        varCount_ = static_cast<std::uint16_t>(count);
        hello_ = b_.text("Hello world from " + processName + "!");
        valueText_ = opt.printVariables ? b_.text("Value from: ") : hello_;
    }

    Program run(std::uint64_t lines) {
        emit(lines, 0);
        return b_.build();
    }

private:
    std::uint16_t randomVar() { return static_cast<std::uint16_t>(pick(0, varCount_ - 1)); }
    std::uint16_t randomLiteral() { return static_cast<std::uint16_t>(pick(0, opt_.maxLiteral)); }
    Operand randomOperand() {
        return pick(0, 1) ? Operand{randomVar(), true} : Operand{randomLiteral(), false};
    }

    // Emits instructions whose expanded count is exactly `lines`. A FOR block
    // of r repeats over a body of k lines uses up r * k of the budget.
    void emit(std::uint64_t lines, int depth) {
        while (lines > 0) {
            if (depth < ProgramBuilder::kMaxLoopDepth && lines >= 2 &&
                static_cast<int>(pick(0, 99)) < opt_.forPercent) {
                std::uint64_t repeats = pick(2, std::max<std::uint64_t>(2, std::min<std::uint64_t>(opt_.maxRepeats, lines)));
                std::uint64_t body = pick(1, std::max<std::uint64_t>(1, std::min(opt_.maxLoopBody, lines / repeats)));
                b_.beginFor(static_cast<std::uint32_t>(repeats));
                emit(body, depth + 1);
                b_.endFor();
                lines -= repeats * body;
            } else {
                emitLeaf();
                --lines;
            }
        }
    }

    void emitLeaf() {
        Instruction in;
        switch (pick(0, 4)) {
            case 0:
                in.op = Op::Print;
                if (opt_.printVariables && pick(0, 1)) {
                    in.arg = valueText_;
                    in.lhs = Operand{randomVar(), true};
                } else {
                    in.arg = hello_;
                }
                break;
            case 1:
                in.op = Op::Declare;
                in.dst = randomVar();
                in.lhs = Operand{randomLiteral(), false};
                break;
            case 2:
            case 3:
                in.op = (pick(0, 1) == 0) ? Op::Add : Op::Subtract;
                in.dst = randomVar();
                in.lhs = randomOperand();
                in.rhs = randomOperand();
                break;
            default:
                in.op = Op::Sleep;
                in.arg = static_cast<std::uint32_t>(pick(1, std::max<std::uint8_t>(1, opt_.maxSleepTicks)));
                break;
        }
        b_.leaf(in);
    }

    GeneratorOptions opt_;
    ProgramBuilder b_;
    std::uint16_t varCount_ = 1;
    std::uint32_t hello_ = 0;
    std::uint32_t valueText_ = 0;
};

Program generateProgram(const std::string& processName,
                        std::uint64_t minIns, std::uint64_t maxIns,
                        const GeneratorOptions& options) {
    if (minIns > maxIns) std::swap(minIns, maxIns);
    if (minIns == 0) minIns = 1;
    if (maxIns < minIns) maxIns = minIns;
    return ProgramGenerator(processName, options).run(pick(minIns, maxIns));
}
