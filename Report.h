// Report.h  (Role D)
// Text shared by "screen -ls" (printed) and "report-util" (saved to csopesy-log.txt).
#pragma once

#include "Scheduler.h"

#include <string>

std::string buildReport(Scheduler& sched);

// Writes the report to `file`; returns the absolute path, or "" on failure.
std::string saveReport(Scheduler& sched, const std::string& file = "csopesy-log.txt");
