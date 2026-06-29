#include "TimingUtils.h"

bool isWithinTimeWindow(time_t now, const char* startHHMM, const char* endHHMM) {
    int nowMinutes = getMinutesSinceMidnight(now);
    int startMinutes = parseTimeString(startHHMM);
    int endMinutes = parseTimeString(endHHMM);

    if (startMinutes < 0 || endMinutes < 0) {
        // Invalid time string, assume always in window
        return true;
    }

    if (startMinutes <= endMinutes) {
        // Normal window (e.g., 07:00 - 21:00)
        return (nowMinutes >= startMinutes && nowMinutes < endMinutes);
    } else {
        // Cross-midnight window (e.g., 22:00 - 06:00)
        return (nowMinutes >= startMinutes || nowMinutes < endMinutes);
    }
}

int getMinutesSinceMidnight(time_t now) {
    struct tm* timeinfo = localtime(&now);
    return timeinfo->tm_hour * 60 + timeinfo->tm_min;
}

int parseTimeString(const char* hhmm) {
    if (hhmm == nullptr || strlen(hhmm) < 5) return -1;

    int hours = 0, minutes = 0;
    if (sscanf(hhmm, "%d:%d", &hours, &minutes) != 2) {
        return -1;
    }

    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
        return -1;
    }

    return hours * 60 + minutes;
}

String formatTime(time_t t) {
    struct tm* timeinfo = localtime(&t);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
    return String(buffer);
}

String formatTimeHHMM(time_t t) {
    struct tm* timeinfo = localtime(&t);
    char buffer[10];
    strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
    return String(buffer);
}

bool initNTP(long gmtOffsetSec, int daylightOffsetSec) {
    configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.google.com", "europe.pool.ntp.org");
    log_i("NTP initialized (offset=%ld, DST=%d)", gmtOffsetSec, daylightOffsetSec);
    return true;
}

bool waitForNTPSync(int timeoutSec) {
    time_t now = time(nullptr);
    int attempts = 0;
    int maxAttempts = timeoutSec * 2;  // Check every 500ms

    while (now < 100000 && attempts < maxAttempts) {
        delay(500);
        now = time(nullptr);
        attempts++;
    }

    if (now >= 100000) {
        struct tm* timeinfo = localtime(&now);
        log_i("NTP synced: %s", formatTime(now).c_str());
        return true;
    }

    log_w("NTP sync timeout after %d seconds", timeoutSec);
    return false;
}