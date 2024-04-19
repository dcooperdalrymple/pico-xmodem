#include "xmodem.hpp"

#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include "pico/stdlib.h"
#include "pico/malloc.h"
#include "pico/printf.h"

XMODEM::XMODEM() {
    stdio_init_all();
	stdio_set_translate_crlf(&stdio_usb, false); // Disable automatic carriage return
    bzero(&this->config, sizeof(this->config));
    this->reset_log();
};

XMODEM::XMODEM(const XConfig config) {
    XMODEM();
    this->set_config(config);
};

XMODEM::XMODEM(XMode mode) {
    XMODEM();
    bzero(&this->config, sizeof(this->config));
    this->set_mode(mode);
};

XMODEM::~XMODEM() {
    delete this->log_buffer;
};

// Configuration

void XMODEM::set_config(const XConfig config) {
    bcopy(&config, &this->config, sizeof(this->config));
};

void XMODEM::set_mode(XMode mode) {
    switch (mode) {
        case XMode::CRC:
            this->config.use_crc = true;
            this->config.require_crc = true;
            this->config.use_escape = false;
            break;
        default:
            this->config.use_crc = false;
            this->config.require_crc = false;
            this->config.use_escape = false;
            break;
    }
};

void XMODEM::set_log_level(XLogLevel level) {
    this->config.log_level = level;
};

void XMODEM::set_escaping(bool use_escape) {
    this->config.use_escape = use_escape;
};

void XMODEM::print_config() {
	printf("Serial protocol: XMODEM+CRC\r\n");
	printf("\tBlock size: %d bytes\r\n", XMODEM_BLOCKSIZE);
	printf("\tCRC: %s\r\n", this->config.use_crc ? "on" : "off");
    if (this->config.use_crc) printf("\tCRC Required: %s\r\n", this->config.require_crc ? "yes" : "no");
	printf("\tEscaping: %s\r\n", this->config.use_escape ? "on" : "off");
	printf("\tLog level: %s (%d)\r\n", this->config.get_log_level_name(), this->config.log_level);
};

// Logging

void XMODEM::reset_log() {
    if (this->log_buffer != nullptr) delete this->log_buffer;
    this->log_buffer = (char *)malloc(XMODEM_LOGSIZE * sizeof(char));
    memset(this->log_buffer, 0x00, XMODEM_LOGSIZE);
    this->log_index = 0;
};

void XMODEM::clear_log() {
    this->log_buffer[0] = 0;
    this->log_index = 0;
};

void XMODEM::dump_log() {
    if (!this->log_index) return;
    puts(this->log_buffer);
};

void XMODEM::log(char * message) {
    this->log(XLogLevel::Fatal, message);
};

void XMODEM::log(XLogLevel level, char * message) {
    if (!this->is_log_level(level)) return;

    if (this->log_index + strlen(message) + 3 >= XMODEM_LOGSIZE) {
        this->log_index = XMODEM_LOGSIZE;
        return;
    }

    strcpy(this->log_buffer + this->log_index, message);
    this->log_index += strlen(message);

    // Add new line and null terminator
    this->log_buffer[this->log_index++] = '\r';
    this->log_buffer[this->log_index++] = '\n';
    this->log_buffer[this->log_index] = 0;
};

void XMODEM::logf(char * message, ...) {
    va_list args;
    va_start(args, message);
    this->logf(XLogLevel::Fatal, message, args);
    va_end(args);
};

void XMODEM::logf(XLogLevel level, char * message, ...) {
    if (!this->is_log_level(level)) return;

    // BUG: Calculating strlen before formatting message
    if (this->log_index + strlen(message) + 3 >= XMODEM_LOGSIZE) {
        this->log_index = XMODEM_LOGSIZE;
        return;
    }

    int result;
    va_list args;
    va_start(args, message);
    result = snprintf(this->log_buffer + this->log_index, XMODEM_LOGSIZE - this->log_index - 3, message, args);
    va_end(args);
    if (result <= 0) return;

    while (this->log_buffer[this->log_index] != 0) {
        this->log_index++;
    }

    // Add new line and null terminator
    this->log_buffer[this->log_index++] = '\r';
    this->log_buffer[this->log_index++] = '\n';
    this->log_buffer[this->log_index] = 0;
}

bool XMODEM::is_log_level(XLogLevel level) {
    return level <= this->config.log_level;
};

// Send

bool XMODEM::send(uint8_t * buffer, size_t size) {
    return this->send(buffer, size, 30000, 10000);
};
bool XMODEM::send(uint8_t * buffer, size_t size, uint wait_timeout, uint read_timeout) {
    bool use_crc = false;
    bool result = true;
    char c;
    uint i;

    // Indicate start of transmission and determine CRC mode
    absolute_time_t timeout = make_timeout_time_ms(wait_timeout);
    while (true) {
        if (absolute_time_diff_us(timeout, get_absolute_time()) > 0) {
            this->log(XLogLevel::Warning, "Timeout");
            result = false;
            break;
        }
        
        c = getchar_timeout_us(read_timeout);
        if (c == PICO_ERROR_TIMEOUT) continue;

        if (c == XMODEM_CRC) {
            use_crc = true;
            this->log(XLogLevel::Info, "CRC enabled");
            break;
        } else if (c == XMODEM_NAK) {
            if (this->config.use_crc && this->config.require_crc) {
                this->log("Host must be configured for CRC-16");
                result = false;
                break;
            }
            use_crc = false;
            this->log(XLogLevel::Info, "CRC disabled");
            break;
        } else if (c != XMODEM_BS) {
            this->logf(XLogLevel::Info, "Unexpected character %02X received - expected %02X or %02X", c, XMODEM_CRC, XMODEM_NAK);
        }
    }

    // Send data packets
    if (result) {
        for (i = 0; (i+1)*XMODEM_BLOCKSIZE <= size; i++) {
            if (!this->send_packet(buffer, size, i, use_crc, read_timeout)) {
                this->log(XLogLevel::Warning, "Packet transmission failed");
                result = false;
                break;
            }
        }
    }

    // Indicate end of file
    if (result) {
        i = 0;
        while (true) {
            putchar(XMODEM_EOT);
            c = getchar_timeout_us(read_timeout);
            if (c == XMODEM_ACK) {
                break;
            } else if (c == XMODEM_NAK) {
                continue;
            } else if (c == XMODEM_CAN && getchar_timeout_us(read_timeout) == XMODEM_CAN) {
                result = false;
                break;
            }
            if (++i > 10) {
                if (i >= 9) this->log(XLogLevel::Error, "EOT Timeout");
                result = false;
                break;
            }
        }
    }

    if (!result) this->abort();

    this->dump_log();
    return result;
};

bool XMODEM::send_packet(uint8_t * buffer, size_t size, uint block) {
    return this->send_packet(buffer, size, block, this->config.use_crc);
};

bool XMODEM::send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc) {
    return this->send_packet(buffer, size, block, use_crc, 1000);
};

bool XMODEM::send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc, uint timeout) {
    return this->send_packet(buffer, size, block, use_crc, timeout, 10);
};

// Timeout in us
bool XMODEM::send_packet(uint8_t * buffer, size_t size, uint block, bool use_crc, uint timeout, uint8_t attempts) {
    size_t start = block * XMODEM_BLOCKSIZE;
    if (start > size) {
        this->log(XLogLevel::Fatal, "Packet index in excess of data");
        return false;
    }

    size_t end = (block + 1) * XMODEM_BLOCKSIZE;
    if (end > size) end = size;
    size_t len = end - start;

    this->logf(XLogLevel::Debug, "Sending packet %d: %04X-%04X", block+1, start, end);

    uint8_t * packet = (uint8_t *)malloc(XMODEM_BLOCKSIZE * sizeof(uint8_t));
    memcpy(packet, buffer + start, len);
    if (len < XMODEM_BLOCKSIZE) {
        // Pad packet with SUB
        memset(packet + len, XMODEM_SUB, XMODEM_BLOCKSIZE - len);
    }
    
    char c;
    uint16_t checksum;
    size_t i;
    bool result = false;
    for (uint8_t attempt = 0; attempt < attempts; attempt++) {
        // Block header
        putchar(XMODEM_SOH);
        putchar((char)(block+1));
        putchar(255-(char)(block+1));

        // Send bytes and calculate checksum
        checksum = 0;
        for (i = 0; i < XMODEM_BLOCKSIZE; i++) {
            c = packet[i];
            putchar(c);
            checksum = calculate_checksum(checksum, c, use_crc);
        }

        // Send checksum
        if (use_crc) putchar((char)(checksum >> 8));
        putchar((char)checksum);
        if (use_crc) {
            this->logf(XLogLevel::Debug, "Checksum for packet: %04X", checksum);
        } else {
            this->logf(XLogLevel::Debug, "Checksum for packet: %02X", checksum & 0xff);
        }

        // Handle response
        // BUG: PICO_ERROR_TIMEOUT?
        c = getchar_timeout_us(timeout);
        if (c == XMODEM_ACK) {
            result = true;
            break;
        } else if (c == XMODEM_CAN && getchar_timeout_us(timeout) == XMODEM_CAN) {
            this->log("Transmission cancelled by host");
            break;
        } else if (c != XMODEM_NAK) {
            this->logf(XLogLevel::Debug, "Unknown response %02X, retrying packet %d", c, block+1);
        }
        this->logf(XLogLevel::Debug, "Retrying packet %d", block+1);
    }

    if (!result) this->logf(XLogLevel::Info, "Failed to deliver packet %d", block+1);

    delete packet;
    return result;
};

// Receive

size_t XMODEM::receive(uint8_t * buffer, size_t size) {
    return this->receive(buffer, size, 3000000, 10000);
};

size_t XMODEM::receive(uint8_t * buffer, size_t size, uint wait_timeout, uint read_timeout) {
    uint block = 0;
    bool error = false;
    char c;

    // Transmission loop
    while (true) {
        if (block == 0) {
            // Indicate ready to receive
            if (this->config.use_crc) {
                putchar(XMODEM_BS);
                putchar(XMODEM_CRC);
            } else {
                putchar(XMODEM_NAK);
            }
        }

        // TODO: Max attempts
        c = getchar_timeout_us(wait_timeout);
        if (c == PICO_ERROR_TIMEOUT) continue;

        if (c == XMODEM_EOT) {
            this->log(XLogLevel::Info, "EOT => ACK");
            putchar(XMODEM_ACK);
            break;
        } else if (c == XMODEM_CAN) {
            this->log(XLogLevel::Info, "CAN => ACK");
            putchar(XMODEM_ACK);
            error = true;
            break;
        } else if (c != XMODEM_SOH) {
            this->logf(XLogLevel::Info, "Unexpectd character %02X received, expected SOH or EOT", c);
            continue;
        }

        this->logf(XLogLevel::Debug, "Got SOH for packet %d", block+1);

        if ((block+1) * XMODEM_BLOCKSIZE > size) {
            this->log(XLogLevel::Info, "Transmission exceeds maximum size");
            this->abort();
            error = true;
            break;
        }

        if (this->receive_packet(buffer, size, block)) block++;
    }

    if (block == 0 || error) this->log(XLogLevel::Warning, "Failed to receive data");

    this->dump_log();
    return !error ? block * XMODEM_BLOCKSIZE : 0;
};

bool XMODEM::receive_packet(uint8_t * buffer, size_t size, uint block) {
    return this->receive_packet(buffer, size, block, 10000, 100000);
};

bool XMODEM::receive_packet(uint8_t * buffer, size_t size, uint block, uint read_timeout, uint wait_timeout) {
    // BUG: Support partial block?
    if ((block+1)*XMODEM_BLOCKSIZE > size) {
        this->log(XLogLevel::Fatal, "Packet index in excess of data buffer");
        return false;
    }

    size_t i;

    // Get packet header (block index)
    char * header = (char *)malloc(2 * sizeof(char));
    for (i = 0; i < 2; i++) {
        header[i] = getchar_timeout_us(i == 0 ? wait_timeout : read_timeout);
        if (header[i] == PICO_ERROR_TIMEOUT) {
            putchar(XMODEM_NAK);
            delete header;
            return false;
        }
    }

    // Receive packet and calculate checksum
    bool result = true;
    uint16_t checksum = 0;
    bool escape = false;
    char c;
    char * packet = (char *)malloc(XMODEM_BLOCKSIZE * sizeof(char));
    i = 0;
    while (i < XMODEM_BLOCKSIZE) {
        c = getchar_timeout_us(read_timeout);
        if (c == PICO_ERROR_TIMEOUT) {
            result = false;
            break;
        }

        if (this->config.use_escape && c == XMODEM_DLE) {
            escape = true;
            continue;
        }
        if (escape) {
            c = c ^ 0x40;
            escape = false;
        }

        packet[i++] = c;
        checksum = this->calculate_checksum(checksum, c);
    }

    // Get packet footer (checksum)
    uint8_t footer_size = (this->config.use_crc ? 2 : 1);
    char * footer = (char *)malloc(footer_size * sizeof(char));
    if (result) {
        for (i = 0; i < footer_size; i++) {
            footer[i] = getchar_timeout_us(read_timeout);
            if (footer[i] == PICO_ERROR_TIMEOUT) {
                result = false;
                break;
            }
        }
    }

    // Bad block index
    if (result && (header[0] != (block+1) || header[1] != (255-(block+1)))) {
        result = false;
    }

    // Bad checksum
    if (result) {
        // TODO: Improve logic
        if (this->config.use_crc && (footer[0] != (checksum >> 8) & 0xff || footer[1] != checksum & 0xff)) {
            result = false;
        } else if (footer[0] != checksum & 0xff) {
            result = false;
        }
    }

    if (result) {
        this->log(XLogLevel::Info, "ACK");
        putchar(XMODEM_ACK);

        // Copy packet into data buffer
        // TODO: Support SUB
        memcpy(buffer+(block*XMODEM_BLOCKSIZE), packet, XMODEM_BLOCKSIZE);
    } else {
        this->log(XLogLevel::Info, "NAK");
        putchar(XMODEM_NAK);
    }

    delete header;
    delete packet;
    delete footer;

    // TODO: Return size of block without SUB
    return result;
};

void XMODEM::abort() {
    this->abort(8, 1000);
};

void XMODEM::abort(uint8_t count, uint timeout) {
    for (uint8_t i = 0; i < count; i++) {
        putchar(XMODEM_CAN);
    }
    while (getchar_timeout_us(timeout) != PICO_ERROR_TIMEOUT) { }
};

uint16_t XMODEM::calculate_checksum(uint16_t checksum, uint8_t value) {
    return this->calculate_checksum(checksum, value, this->config.use_crc);
};

uint16_t XMODEM::calculate_checksum(uint16_t checksum, uint8_t value, bool use_crc) {
    if (use_crc) {
        checksum = checksum ^ (uint16_t)value << 8;
        for (uint8_t i = 0; i < 8; i++) {
            if (checksum & 0x8000) {
                checksum = checksum << 1 ^ 0x1021;
            } else {
                checksum = checksum << 1;
            }
        }
    } else {
        checksum += value;
    }
    return checksum;
};
