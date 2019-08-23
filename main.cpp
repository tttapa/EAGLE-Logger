#include <array>
#include <cstring>
#include <iostream>
#include <map>

inline size_t nextWord(size_t i) { return i - (i % 4) + 4; }
inline size_t roundUpToWordSizeMultiple(size_t i) {
    return i + 3 - ((i + 3) % 4);
}

class ILoggable {
  protected:
    ILoggable(const char *id) : id(id) {}

  public:
    ~ILoggable() = default;
    size_t log(uint8_t *buffer, size_t maxLen) {
        size_t idLen = strlen(id);
        if (idLen + 1 + 4 > maxLen)
            return 0;
        strcpy((char *) buffer, id);
        size_t headerStart  = nextWord(idLen);
        buffer[headerStart] = getTypeID();
        size_t dataStart    = headerStart + 4;
        size_t dataLen      = logData(buffer + dataStart, maxLen - dataStart);
        buffer[headerStart + 1] = dataLen >> 0;
        buffer[headerStart + 2] = dataLen >> 8;
        buffer[headerStart + 3] = dataLen >> 16;
        return roundUpToWordSizeMultiple(dataStart + dataLen);
    }
    virtual size_t logData(uint8_t *buffer, size_t maxLen) = 0;
    virtual uint8_t getTypeID()                            = 0;

  private:
    const char *id;
};

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

template <class T, size_t N>
class Loggable : public ILoggable {
  public:
    Loggable(const char *id, std::array<T, N> data)
        : ILoggable(id), data(data) {}

  public:
    size_t logData(uint8_t *buffer, size_t maxLen) override {
        if (maxLen < N * sizeof(T))
            return 0;
        memcpy(buffer, data.data(), data.size() * sizeof(T));
        return N * sizeof(T);
    }
    uint8_t getTypeID() override { return ::getTypeID<T>(); }

  private:
    std::array<T, N> data;
};

char nibbleToHex(uint8_t val) {
    val &= 0x0F;
    return val >= 10 ? val + 'A' - 10 : val + '0';
}

void printHex(std::ostream &os, uint8_t val) {
    os << nibbleToHex(val >> 4) << nibbleToHex(val) << ' ';
}

struct LogEntry {
    const uint8_t *data;
    uint8_t type : 8;
    uint32_t length : 24;

    template <class T>
    T get(size_t index = 0) const {
        if (this->type != getTypeID<T>())
            throw std::runtime_error("Invalid type");
        if (index * sizeof(T) >= this->length)
            throw std::out_of_range("Index out of range");
        T result;
        memcpy(&result, data + sizeof(T) * index, sizeof(T));
        return result;
    }
};

int main() {
    Loggable<uint32_t, 4> l1 = {
        "identifier",
        {{0xDEADBEEF, 0x11223344, 0x55555555, 0x10101010}},
    };
    Loggable<double, 2> l2 = {
        "doubles",
        {{42.42, 33.3333333333333333333}},
    };
    Loggable<uint64_t, 2> l3 = {
        "longs",
        {{0x1122334455667788, 0x99AABBCCDDEEFF00}},
    };
    Loggable<uint8_t, 4> l4 = {
        "u8x4",
        {{0x11, 0x22, 0x33, 0x44}},
    };
    Loggable<uint8_t, 3> l5 = {
        "u8x3",
        {{0x11, 0x22, 0x33}},
    };
    Loggable<uint8_t, 2> l6 = {
        "u8x2",
        {{0x11, 0x22}},
    };
    Loggable<uint8_t, 1> l7 = {
        "u8x1",
        {{0x11}},
    };
    Loggable<uint8_t, 5> l8 = {
        "u8x5",
        {{0x11, 0x22, 0x33, 0x44, 0x55}},
    };
    Loggable<uint32_t, 1> l9 = {
        "deadbeef",
        {{0xEFBEADDE}},
    };
    std::array<uint8_t, 200> buffer = {};
    size_t maxSize                  = buffer.size();

    maxSize -= l1.log(buffer.end() - maxSize, maxSize);
    maxSize -= l2.log(buffer.end() - maxSize, maxSize);
    maxSize -= l3.log(buffer.end() - maxSize, maxSize);
    maxSize -= l4.log(buffer.end() - maxSize, maxSize);
    maxSize -= l5.log(buffer.end() - maxSize, maxSize);
    maxSize -= l6.log(buffer.end() - maxSize, maxSize);
    maxSize -= l7.log(buffer.end() - maxSize, maxSize);
    maxSize -= l8.log(buffer.end() - maxSize, maxSize);
    maxSize -= l9.log(buffer.end() - maxSize, maxSize);

    for (size_t i = 0; i < buffer.size(); i += 4) {
        // printHex(std::cout, i);
        std::cout << i << '\t';
        printHex(std::cout, buffer[i + 0]);
        printHex(std::cout, buffer[i + 1]);
        printHex(std::cout, buffer[i + 2]);
        printHex(std::cout, buffer[i + 3]);
        std::cout << ' '
                  << (isprint(buffer[i + 0]) ? (char) buffer[i + 0] : '.')
                  << ' ';
        std::cout << (isprint(buffer[i + 1]) ? (char) buffer[i + 1] : '.')
                  << ' ';
        std::cout << (isprint(buffer[i + 2]) ? (char) buffer[i + 2] : '.')
                  << ' ';
        std::cout << (isprint(buffer[i + 3]) ? (char) buffer[i + 3] : '.')
                  << ' ';
        std::cout << std::endl;
    }

    auto cmp = [](const char *a, const char *b) {
        return std::strcmp(a, b) < 0;
    };
    std::map<const char *, LogEntry, decltype(cmp)> parseResult{cmp};
    const uint8_t *data = buffer.data();
    while (data < buffer.end()) {
        std::cout << data - buffer.begin() << std::endl;
        const char *identifier = (const char *) data;
        size_t idLen           = strlen(identifier);
        if (idLen == 0) {
            break;
        }
        size_t headerStart = nextWord(idLen);
        uint8_t type       = data[headerStart];
        uint32_t length    = (data[headerStart + 1] << 0) |  //
                          (data[headerStart + 2] << 8) |     //
                          (data[headerStart + 2] << 16);
        size_t dataStart        = headerStart + 4;
        parseResult[identifier] = {data + dataStart, type, length};
        std::cout << identifier << " " << +type << " " << length << std::endl;
        data += dataStart + roundUpToWordSizeMultiple(length);
    }
    std::cout << parseResult["doubles"].get<double>(1) << std::endl;
}