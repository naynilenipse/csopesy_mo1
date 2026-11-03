// Config.h  (Role D)
// Reads and validates config.txt for the "initialize" command.
#pragma once

#include <cstdint>
#include <string>

struct Config {
    int           numCpu           = 4;      // [1, 128]
    bool          roundRobin       = true;   // "rr" or "fcfs"
    std::uint64_t quantumCycles    = 5;      // [1, 2^32]
    std::uint64_t batchProcessFreq = 1;      // [1, 2^32]
    std::uint64_t minIns           = 1000;   // [1, 2^32]
    std::uint64_t maxIns           = 2000;   // [1, 2^32]
    std::uint64_t delayPerExec     = 0;      // [0, 2^32]
};

// Parses `path`. On success fills `out` and returns true. On failure returns
// false and `error` holds one line per problem (already newline-terminated),
// ready to print.
bool loadConfig(const std::string& path, Config& out, std::string& error);
