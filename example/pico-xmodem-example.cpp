#include <stdio.h>
#include "pico/stdlib.h"
#include "xmodem.hpp"

#define MAXSIZE 32768
static uint8_t buffer[MAXSIZE];

static XMODEM xmodem(XMode::CRC);

int main() {
    // NOTE: Not necessary to call `stdio_init_all()`
    printf("Pico C/C++ XMODEM Library Example\r\n");

    xmodem.set_log_level(XLogLevel::Debug);

    uint8_t i;
    size_t size;
    while (true) {
        size = xmodem.receive(buffer, MAXSIZE);
        if (size > 0) {
            printf("Received %d bytes\r\n", size);
        }
        if (size > 8) {
            for (i = 0; i < 8; i++) {
                printf("%02X ", buffer[i]);
            }
            printf("\r\n");
        }
    }

    return 0;
}
