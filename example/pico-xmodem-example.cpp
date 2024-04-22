#include <stdio.h>
#include <tusb.h>
#include "pico/stdlib.h"
#include "xmodem.hpp"

#define MAXSIZE 32768
#define MODE XMode::Original

static uint8_t buffer[MAXSIZE];
static XMODEM xmodem(MODE);

int main() {
    // NOTE: Not necessary to call `stdio_init_all()`

    xmodem.set_log_level(XLogLevel::Debug);

    uint8_t i;
    size_t size;
    while (true) {
        while (!tud_cdc_connected()) sleep_ms(100);
        printf("\r\nPico C/C++ XMODEM Library Example\r\n");

        while (true) {
            printf("Ready to receive data. Begin sending file...\r\n");
            size = xmodem.receive(buffer, MAXSIZE);
            if (size > 0) {
                printf("Received %d bytes\r\n", size);
                if (size > 8) {
                    for (i = 0; i < 8; i++) {
                        printf("%02X ", buffer[i]);
                    }
                    printf("\r\n");
                }
                printf("Sending back data. Begin receive...\r\n");
                if (xmodem.send(buffer, size)) {
                    printf("Successfully delivered!\r\n");
                } else {
                    printf("Failed to deliver.\r\n");
                }
            }
        }
    }

    return 0;
}
