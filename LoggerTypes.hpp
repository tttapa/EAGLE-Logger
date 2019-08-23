#pragma once

#include <cstdint>

template <class T>
uint8_t getTypeID();

#define ADD_TYPE(type, value)                                                  \
    namespace LoggerType {                                                     \
    constexpr uint8_t type_##type = (value);                                   \
    }                                                                          \
    template <>                                                                \
    uint8_t getTypeID<type>() {                                                \
        return LoggerType::type_##type;                                        \
    }

ADD_TYPE(uint32_t, 1)
ADD_TYPE(int32_t, 2)
ADD_TYPE(uint64_t, 3)
ADD_TYPE(int64_t, 4)
ADD_TYPE(float, 5)
ADD_TYPE(double, 6)
ADD_TYPE(bool, 7)
ADD_TYPE(uint8_t, 8)
ADD_TYPE(char, 9)