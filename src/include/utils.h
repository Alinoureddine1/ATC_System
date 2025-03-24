#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <time.h>

/** Airspace boundaries **/
const double AIRSPACE_X_MIN = 0.0;
const double AIRSPACE_X_MAX = 100000.0;
const double AIRSPACE_Y_MIN = 0.0;
const double AIRSPACE_Y_MAX = 100000.0;
const double AIRSPACE_Z_MIN = 0.0;
const double AIRSPACE_Z_MAX = 25000.0;

/** Convert bool->string **/
static inline std::string boolToString(bool value) {
    return value ? "true" : "false";
}

/** Check if within airspace bounds **/
static inline bool isPositionWithinBounds(double x, double y, double z) {
    return (x >= AIRSPACE_X_MIN && x <= AIRSPACE_X_MAX &&
            y >= AIRSPACE_Y_MIN && y <= AIRSPACE_Y_MAX &&
            z >= AIRSPACE_Z_MIN && z <= AIRSPACE_Z_MAX);
}

/** Print a timestamp for logging **/
static inline std::string printTimeStamp() {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", localtime(&now));
    return std::string(buf);
}

#endif // UTILS_H
