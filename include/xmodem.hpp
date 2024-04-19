#pragma once
#include "pico/stdlib.h"

const size_t XMODEM_LOGSIZE = 65536;

const uint8_t XMODEM_SOH = 0x01;
const uint8_t XMODEM_EOT = 0x04;
const uint8_t XMODEM_ACK = 0x06;
const uint8_t XMODEM_BS = 0x08;
const uint8_t XMODEM_DLE = 0x10;
const uint8_t XMODEM_NAK = 0x15;
const uint8_t XMODEM_CAN = 0x18;
const uint8_t XMODEM_SUB = 0x1a;
const uint8_t XMODEM_CRC = 'C';

const size_t XMODEM_BLOCKSIZE = 128;

enum class XMode : uint8_t {
    Original = 0,
    CRC
};

enum class XLogLevel : uint8_t {
    Fatal = 0,
    Error,
    Warning,
    Info,
    Debug
};

static const char * XLogLevelNames[5] = {
    "Fatal",
    "Error",
    "Warning",
    "Info",
    "Debug"
};

typedef struct xmodem_config_t {
    XLogLevel log_level;
    bool use_crc;
    bool require_crc;
    bool use_escape;
    const char * get_log_level_name() {
        return XLogLevelNames[static_cast<uint8_t>(this->log_level)];
    };
} XConfig;

class XMODEM {

public:

    XMODEM();
    XMODEM(const XConfig config);
    XMODEM(XMode mode);
    ~XMODEM();

    void set_config(const XConfig config);
    void set_mode(XMode mode);
    void set_log_level(XLogLevel level);
    void set_escaping(bool use_escape);

    void print_config();

    bool send(uint8_t * buffer, size_t size);
    bool send(uint8_t * buffer, size_t size, uint wait_timeout, uint read_timeout);

    size_t receive(uint8_t * buffer, size_t size);
    size_t receive(uint8_t * buffer, size_t size, uint wait_timeout, uint read_timeout);

private:

    XConfig config;
    char * log_buffer;
    size_t log_index = 0;

    void reset_log();
    void clear_log();
    void dump_log();
    void log(char * message);
    void log(XLogLevel level, char * message);
    void logf(char * message, ...);
    void logf(XLogLevel level, char * message, ...);
    bool is_log_level(XLogLevel level);

    bool send_packet(uint8_t * buffer, size_t size, uint block);
    bool send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc);
    bool send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc, uint timeout);
    bool send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc, uint timeout, uint8_t attempts);

    bool receive_packet(uint8_t * buffer, size_t size, uint block);
    bool receive_packet(uint8_t * buffer, size_t size, uint block, uint read_timeout, uint wait_timeout);

    void abort();
    void abort(uint8_t count, uint timeout);

    uint16_t calculate_checksum(uint16_t checksum, uint8_t value);
    uint16_t calculate_checksum(uint16_t checksum, uint8_t value, bool use_crc);

};
