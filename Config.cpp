// Config.cpp  (Role D)
#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace {

constexpr std::uint64_t kMax32 = 4294967296ULL;  // 2^32

// Digits only: rejects "-1", "1.5", "abc", "" and overflow.
bool parseUnsigned(const std::string& s, std::uint64_t& out) {
    if (s.empty() || s.size() > 19) return false;
    std::uint64_t v = 0;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        v = v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    out = v;
    return true;
}

std::string stripQuotes(std::string s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                          (s.front() == '\'' && s.back() == '\'')))
        s = s.substr(1, s.size() - 2);
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Reads an unsigned value in [lo, hi] into `dst`; appends to `err` on failure.
void readRange(const std::string& key, const std::string& val, std::uint64_t lo,
               std::uint64_t hi, std::uint64_t& dst, std::string& err) {
    std::uint64_t v;
    if (!parseUnsigned(val, v)) {
        err += "config.txt: '" + key + "' must be a whole number, got '" + val + "'.\n";
    } else if (v < lo || v > hi) {
        err += "config.txt: '" + key + "' must be in [" + std::to_string(lo) + ", " +
               std::to_string(hi) + "], got " + val + ".\n";
    } else {
        dst = v;
    }
}

}  // namespace

bool loadConfig(const std::string& path, Config& out, std::string& error) {
    error.clear();
    std::ifstream in(path);
    if (!in) {
        error = "Could not open '" + path + "'. Place config.txt next to the program.\n";
        return false;
    }

    Config cfg;
    std::set<std::string> seen;
    std::string err, line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        std::istringstream ss(line);
        std::vector<std::string> tok;
        for (std::string t; ss >> t;) tok.push_back(t);
        if (tok.empty()) continue;

        if (tok.size() != 2) {
            err += "config.txt line " + std::to_string(lineNo) +
                   ": expected '<parameter> <value>'.\n";
            continue;
        }
        std::string key = tok[0];
        const std::string& val = tok[1];
        if (key == "delays-per-exec") key = "delay-per-exec";  // spec table spelling
        seen.insert(key);

        if (key == "num-cpu") {
            std::uint64_t v = cfg.numCpu;
            readRange(key, val, 1, 128, v, err);
            cfg.numCpu = static_cast<int>(v);
        } else if (key == "scheduler") {
            std::string s = lower(stripQuotes(val));
            if (s == "rr") cfg.roundRobin = true;
            else if (s == "fcfs") cfg.roundRobin = false;
            else err += "config.txt: 'scheduler' must be \"fcfs\" or \"rr\", got " + val + ".\n";
        } else if (key == "quantum-cycles") {
            readRange(key, val, 1, kMax32, cfg.quantumCycles, err);
        } else if (key == "batch-process-freq") {
            readRange(key, val, 1, kMax32, cfg.batchProcessFreq, err);
        } else if (key == "min-ins") {
            readRange(key, val, 1, kMax32, cfg.minIns, err);
        } else if (key == "max-ins") {
            readRange(key, val, 1, kMax32, cfg.maxIns, err);
        } else if (key == "delay-per-exec") {
            readRange(key, val, 0, kMax32, cfg.delayPerExec, err);
        } else {
            err += "config.txt line " + std::to_string(lineNo) + ": unknown parameter '" + tok[0] + "'.\n";
        }
    }

    for (const char* k : {"num-cpu", "scheduler", "quantum-cycles", "batch-process-freq",
                          "min-ins", "max-ins", "delay-per-exec"})
        if (!seen.count(k)) err += std::string("config.txt: missing parameter '") + k + "'.\n";

    if (err.empty() && cfg.minIns > cfg.maxIns)
        err += "config.txt: 'min-ins' (" + std::to_string(cfg.minIns) +
               ") must not exceed 'max-ins' (" + std::to_string(cfg.maxIns) + ").\n";

    if (!err.empty()) { error = err; return false; }
    out = cfg;
    return true;
}
