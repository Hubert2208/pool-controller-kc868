#ifndef TIMING_UTILS_H
#define TIMING_UTILS_H

#include <Arduino.h>
#include <time.h>

// Check if the given time falls within a window defined by HH:MM strings
// Supports cross-midnight windows (e.g., start="22:00", end="06:00")
bool isWithinTimeWindow(time_t now, const char* startHHMM, const char* endHHMM);

// Get minutes since midnight for a given time_t
int getMinutesSinceMidnight(time_t now);

// Parse "HH:MM" string to minutes since midnight
// Returns -1 on parse error
int parseTimeString(const char* hhmm);

// Format time_t to ISO-like string
String formatTime(time_t t);

// Format time_t to "HH:MM"
String formatTimeHHMM(time_t t);

// Initialize NTP
bool initNTP(long gmtOffsetSec = 7200, int daylightOffsetSec = 3600);

// Wait for NTP sync with timeout
bool waitForNTPSync(int timeoutSec = 30);

#endif // TIMING_UTILS_H