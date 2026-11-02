#pragma once
// Program.h
// A process's instruction list in a compact, flat form, plus the tools that
// build one: ProgramBuilder for hand-written programs, generateProgram for
// random ones.
//
// Instruction set
//   PRINT(msg [+ var])        log a message, optionally followed by a variable's value
//   DECLARE(var, value)       var = value
//   ADD(var1, var2/v, var3/v) var1 = a + b, clamped to 65535
//   SUBTRACT(var1, a, b)      var1 = a - b, clamped to 0
//   SLEEP(X)                  leave the CPU for X ticks (X is uint8)
//   FOR([instr...], repeats)  repeat a block; nests up to 3 levels
//
// Layout
//   FOR blocks are flattened into ForBegin ... ForEnd markers, so the whole
//   program is one vector the interpreter walks with an instruction pointer.
//   Variable names are resolved to slot numbers when the program is built,
//   so running an instruction is plain array indexing with no string lookups.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class Op : std::uint8_t {
    Print,     // arg = text id; lhs is the optional variable (lhs.isVar)
    Declare,   // vars[dst] = lhs (literal)
    Add,       // vars[dst] = lhs + rhs, clamped to 65535
    Subtract,  // vars[dst] = lhs - rhs, clamped to 0
    Sleep,     // arg = ticks off the CPU
    ForBegin,  // arg = repeat count
    ForEnd     // closes the nearest open ForBegin
};

// A variable slot or a uint16 literal.
struct Operand {
    std::uint16_t value = 0;  // slot number when isVar, literal otherwise
    bool isVar = false;
};

// 16 bytes. Fields unused by an opcode are left at zero.
struct Instruction {
    Op op = Op::Print;
    std::uint16_t dst = 0;
    Operand lhs;
    Operand rhs;
    std::uint32_t arg = 0;
};

struct Program {
    std::vector<Instruction> code;
    std::vector<std::string> texts;     // PRINT messages, indexed by text id
    std::vector<std::string> varNames;  // variable names, indexed by slot
    std::uint64_t totalLines = 0;       // instructions executed in a full run (loops expanded)
};

// Input for ADD / SUBTRACT: a variable name or a literal.
//   b.add("x", "x", 1);  b.subtract("y", 500, "x");
struct Value {
    Value(const char* var) : name(var) {}
    Value(std::string var) : name(std::move(var)) {}
    Value(int literal)
        : literal(static_cast<std::uint16_t>(literal < 0 ? 0 : literal > 65535 ? 65535 : literal)) {}

    std::string name;  // empty means literal
    std::uint16_t literal = 0;
};

// Builds a Program one instruction at a time.
//
//   Program p = ProgramBuilder()
//       .declare("x", 0)
//       .beginFor(3)
//           .add("x", "x", 1)
//           .print("Value from: ", "x")
//       .endFor()
//       .build();
//
// A FOR block that never runs a leaf instruction (0 repeats or an empty body)
// is dropped at endFor(), so the interpreter never spins on an empty loop.
class ProgramBuilder {
public:
    static constexpr int kMaxLoopDepth = 3;

    ProgramBuilder& print(const std::string& text, const std::string& var = "");
    ProgramBuilder& declare(const std::string& var, int value);
    ProgramBuilder& add(const std::string& dst, const Value& lhs, const Value& rhs);
    ProgramBuilder& subtract(const std::string& dst, const Value& lhs, const Value& rhs);
    ProgramBuilder& sleep(std::uint8_t ticks);
    ProgramBuilder& beginFor(std::uint32_t repeats);  // throws past kMaxLoopDepth
    ProgramBuilder& endFor();                         // throws with no open FOR

    // Throws if a FOR is still open. The builder is empty afterwards.
    Program build();

    // Slot number for a variable, creating it on first use.
    std::uint16_t slot(const std::string& name);
    // Text id for a PRINT message, reusing identical messages.
    std::uint32_t text(const std::string& message);

private:
    friend class ProgramGenerator;

    void leaf(const Instruction& in);  // appends a non-loop instruction
    Operand operand(const Value& v);

    Program prog_;
    std::unordered_map<std::string, std::uint16_t> slotOf_;
    std::unordered_map<std::string, std::uint32_t> textOf_;
    std::vector<std::uint32_t> loopStart_;    // code index of each open ForBegin
    std::vector<std::uint64_t> lineCount_{0}; // leaf count per open block; [0] is top level
};

// Settings for random programs. Defaults keep generated processes readable
// and quick to build.
struct GeneratorOptions {
    int forPercent = 10;              // chance (0-100) that a slot becomes a FOR block
    std::uint32_t maxRepeats = 5;     // FOR repeats are drawn from [2, maxRepeats]
    std::uint64_t maxLoopBody = 10;   // max leaf instructions inside one FOR body
    std::uint8_t maxSleepTicks = 5;   // SLEEP ticks are drawn from [1, maxSleepTicks]
    std::uint16_t maxLiteral = 65535; // literals are drawn from [0, maxLiteral]
    int varCount = 4;                 // variables used: x, y, z, w, v4, v5, ...
    bool printVariables = false;      // false: every PRINT is "Hello world from <name>!"
};

// Random program whose full run executes exactly N instructions, with N drawn
// from [minIns, maxIns] (loops expanded). Instruction types are picked with
// equal odds. Safe to call from several threads at once.
//
// CONNECT (config.txt, scheduler-start, screen -s): pass min-ins and max-ins
// from the parsed config here whenever a process is created. Delete this note
// once both process creators read those values from the config.
Program generateProgram(const std::string& processName,
                        std::uint64_t minIns, std::uint64_t maxIns,
                        const GeneratorOptions& options = {});
