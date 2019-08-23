#include <array>
#include <cstring>
#include <iostream>
#include <map>

#include "LoggerTypes.hpp"

inline size_t nextWord(size_t i) { return i - (i % 4) + 4; }
inline size_t roundUpToWordSizeMultiple(size_t i) {
    return i + 3 - ((i + 3) % 4);
}

class Logger;

class ILoggable {
  protected:
    ILoggable(const char *id) : id(id) {
        // Append to the linked list of instances
        if (first == nullptr)
            first = this;
        this->previous = last;
        if (this->previous != nullptr)
            this->previous->next = this;
        last       = this;
        this->next = nullptr;
    }

  public:
    virtual ~ILoggable() {
        // Remove from the linked list of instances
        if (this->previous != nullptr)
            this->previous->next = this->next;
        if (this == last)
            last = this->previous;
        if (this->next != nullptr)
            this->next->previous = this->previous;
        if (this == first)
            first = this->next;
    }

    virtual bool log(Logger &logger) = 0;
    const char *getID() const { return id; }

    static bool logAll(Logger &logger) {
        bool success = true;
        for (ILoggable *el = first; el != nullptr; el = el->next)
            success &= el->log(logger);
        return success;
    }

  private:
    const char *id;

    static ILoggable *first;
    static ILoggable *last;
    ILoggable *next;
    ILoggable *previous;
};

ILoggable *ILoggable::first = nullptr;
ILoggable *ILoggable::last  = nullptr;

class Logger {
  public:
    bool log(const char *identifier, const uint8_t *data, uint8_t typeID,
             size_t length) {
        size_t idLen    = strlen(identifier);
        size_t entryLen = roundUpToWordSizeMultiple(idLen + 1) + 4 +
                          roundUpToWordSizeMultiple(length);
        if (entryLen > maxLen)
            return false;
        strcpy((char *) bufferwritelocation, identifier);
        size_t headerStart               = nextWord(idLen);
        bufferwritelocation[headerStart] = typeID;
        size_t dataStart                 = headerStart + 4;
        memcpy(bufferwritelocation + dataStart, data, length);
        bufferwritelocation[headerStart + 1] = length >> 0;
        bufferwritelocation[headerStart + 2] = length >> 8;
        bufferwritelocation[headerStart + 3] = length >> 16;
        size_t paddedLen = roundUpToWordSizeMultiple(dataStart + length);
        maxLen -= paddedLen;
        bufferwritelocation += paddedLen;
        if (maxLen > 0)
            bufferwritelocation[0] = 0x00;  // Null terminate
        return true;
    }
    template <class T, size_t N>
    bool log(const char *identifier, const std::array<T, N> &data) {
        return log(identifier, reinterpret_cast<const uint8_t *>(data.data()),
                   getTypeID<T>(), N * sizeof(T));
    }
    template <class T, size_t N>
    bool log(const char *identifier, const T (&data)[N]) {
        return log(identifier, reinterpret_cast<const uint8_t *>(data),
                   getTypeID<T>(), N * sizeof(T));
    }
    bool log(const char *identifier, const char *data) {
        return log(identifier, reinterpret_cast<const uint8_t *>(data),
                   getTypeID<char>(), strlen(data));
    }

    bool log(ILoggable &loggable) { return loggable.log(*this); }

  private:
    constexpr static size_t buffersize = 280;
    size_t maxLen                      = buffersize;
    std::array<uint8_t, buffersize> buffer;
    uint8_t *bufferwritelocation = buffer.begin();

  public:
    const std::array<uint8_t, buffersize> &getBuffer() const { return buffer; }
};

template <class T, size_t N>
class Loggable : public ILoggable {
  public:
    Loggable(const char *id, std::array<T, N> data)
        : ILoggable(id), data(data) {}

  public:
    bool log(Logger &logger) override {
        return logger.log(getID(),                                         //
                          reinterpret_cast<const uint8_t *>(data.data()),  //
                          getTypeID<T>(),                                  //
                          N * sizeof(T));                                  //
    }

  private:
    std::array<T, N> data = {};
};

char nibbleToHex(uint8_t val) {
    val &= 0x0F;
    return val >= 10 ? val + 'A' - 10 : val + '0';
}

void printHex(std::ostream &os, uint8_t val) {
    os << nibbleToHex(val >> 4) << nibbleToHex(val) << ' ';
}

#include <iomanip>

inline void printBuffer(const uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; i += 4) {
        std::cout << std::setw(4) << i << "   ";
        printHex(std::cout, buffer[i + 0]);
        printHex(std::cout, buffer[i + 1]);
        printHex(std::cout, buffer[i + 2]);
        printHex(std::cout, buffer[i + 3]);
        std::cout << "  "
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
}

class LogEntry {
  public:
    struct LogElement {
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

        std::string getString() const {
            if (this->type != getTypeID<char>())
                throw std::runtime_error("Invalid type: should be char");
            return std::string(this->data, this->data + this->length);
        }
    };

    static LogEntry parse(const uint8_t *buffer, size_t length) {
        std::map<const char *, LogElement, strcmp> parseResult{};
        const uint8_t *data = buffer;
        const uint8_t *end  = buffer + length;
        while (data < end) {
            const char *identifier = (const char *) data;
            size_t idLen           = strlen(identifier);
            if (idLen == 0)
                break;
            std::cout << data - buffer << '\t';
            size_t headerStart = nextWord(idLen);
            uint8_t type       = data[headerStart];
            uint32_t length    = (data[headerStart + 1] << 0) |  //
                              (data[headerStart + 2] << 8) |     //
                              (data[headerStart + 2] << 16);
            size_t dataStart        = headerStart + 4;
            parseResult[identifier] = {data + dataStart, type, length};
            std::cout << identifier << '\t' << +type << '\t' << length
                      << std::endl;
            data += dataStart + roundUpToWordSizeMultiple(length);
        }
        return {std::move(parseResult)};
    }

    LogElement operator[](const char *key) const { return parseResult.at(key); }
    auto begin() { return parseResult.begin(); }
    auto begin() const { return parseResult.begin(); }
    auto end() { return parseResult.end(); }
    auto end() const { return parseResult.end(); }

  private:
    struct strcmp {
        bool operator()(const char *a, const char *b) const {
            return std::strcmp(a, b) < 0;
        }
    };
    std::map<const char *, LogElement, strcmp> parseResult{};

    LogEntry(std::map<const char *, LogElement, strcmp> &&parseResult)
        : parseResult(std::move(parseResult)) {}
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

    uint32_t carray[]                = {0x11223344, 0x55667788};
    std::array<uint32_t, 2> stdarray = {0x11223344, 0x55667788};

    Logger logger;
    if (!ILoggable::logAll(logger))
        std::cout << "Warning: buffer full. Not all Loggables have been logged"
                  << std::endl;
    logger.log("string", "test-string");
    logger.log("c-array", carray);
    logger.log("std::array", stdarray);

    auto buffer = logger.getBuffer();

    printBuffer(buffer.data(), buffer.size());

    const LogEntry logEntry = LogEntry::parse(buffer.data(), buffer.size());

    std::cout << logEntry["doubles"].get<double>(1) << std::endl;
    std::cout << logEntry["string"].getString() << std::endl;
    std::cout << logEntry["std::array"].get<uint32_t>(0) << std::endl;

    for (auto &el : logEntry) {
        std::cout << el.first << std::endl;
    }
}