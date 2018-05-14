
#ifndef DIRECTION_H_
#define DIRECTION_H_

#include <cstdint>
#include <QLatin1String>

enum class Direction : uint8_t {
    Forward  = 1,
    Backward = 2,
};

inline Direction operator!(Direction& direction) {
    return (direction == Direction::Forward) ?
                Direction::Backward :
                Direction::Forward;
}

inline QLatin1String to_string(Direction direction) {

    switch(direction) {
    case Direction::Forward:
        return QLatin1String("forward");
    case Direction::Backward:
        return QLatin1String("backward");
    }

    Q_UNREACHABLE();
}

#endif
