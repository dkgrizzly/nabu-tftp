#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/util/queue.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>
#include <hardware/pio.h>
#include "config.h"
#include "packet.h"
#include "duart.h"
#include "wifi.h"
#include "pico_hal.h"

uint16_t channelWord = 0;
int channelValid = 1;
uint32_t hardwaremode = 0;
bool usb_connected = false;

void adapterInit() {
    duart_sendRawByte(0x10);
    duart_sendRawByte(0x06);
    duart_sendRawByte(0xE4);
}

void channelStatus() {
    if(!channelValid) {
        printf("Requesting Channel Code\r\n");
        duart_sendRawByte(0x9F); // Request Channel Code
        duart_sendRawByte(0x10);
        duart_sendRawByte(0xE1);
    } else {
        printf("Have Channel Code\r\n");
        duart_sendRawByte(0x1F); // Have Channel Code
        duart_sendRawByte(0x10);
        duart_sendRawByte(0xE1);
    }
}

void getReady() {
    duart_sendRawByte(0x10);
    duart_sendRawByte(0x06);
}

void changeChannel() {
    uint16_t data;

    duart_sendRawByte(0x10);
    duart_sendRawByte(0x06);

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        channelWord = data;
    } else {
        channelWord = 0;
        channelValid = 0;
        return;
    }

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        channelWord |= data << 8;
    } else {
        channelWord = 0;
        channelValid = 0;
        return;
    }

    duart_sendRawByte(0xE4);

    printf("New Channel Code: %04x\r\n", channelWord);

    channelValid = 1;
}

void packetRequest() {
    uint32_t request;
    uint16_t data;

    duart_sendRawByte(0x10);
    duart_sendRawByte(0x06);

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        request = data;
    } else {
        return;
    }

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        request |= data << 8;
    } else {
        return;
    }

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        request |= data << 16;
    } else {
        return;
    }

    data = duart_readByteTimeout(0x7FFFFFFF);
    if(data < 0x100) {
        request |= data << 24;
    } else {
        return;
    }

    duart_sendRawByte(0xE4);

    printf("Packet Request: Segment %06x, Packet %02x\r\n", (request >> 8), (request & 0xff));

    handleRequest(request);
}

static void pico_reboot() {
    watchdog_reboot(0, 0, 0);
	while (1) {
		tight_loop_contents();
		asm("");
	}
}
static void displayPrompt() {
    printf("\r\n>>>");
}

static void __time_critical_func(adapterLoop)() {
    for(;;) {
        if(!usb_connected && stdio_usb_connected()) {
            usb_connected = true;
            printf("NABU TFTP Gateway Ready.\r\n");
            displayPrompt();
        } else if(usb_connected && !stdio_usb_connected()) {
            usb_connected = false;
        }
        int c=getchar_timeout_us(0);
        if(c != PICO_ERROR_TIMEOUT) {
            switch(c) {
                case 'C':
                case 'c':
                    pico_remove("config");
                case 'R':
                case 'r':
                    pico_reboot();
                    break;
                case 'X':
                case 'x':
                    duart_sendRawByte(0x10);
                    duart_sendRawByte(0xE4);
                    displayPrompt();
                    break;
                case '?':
                    printf("\r\n\r\n");
                    printf("NABU TFTP Gateway\r\n");
                    printf("    r    Reboot\r\n");
                    printf("    c    Reconfigure\r\n");
                    printf("    x    Transmit test\r\n");
                    printf("    ?    Help\r\n");
                    printf("\r\n");
                default:
                    displayPrompt();
                    break;
            }
        }
        
        uint16_t data = duart_readByteNonblocking();
        switch(data) {
        default:
            printf("\r%02X \r\n", data);
            displayPrompt();
            break;
        case 0xFFFF:
        case 0xEFEF:
            break;
        case 0x01:
            printf("\r01: Channel Status\r\n");
            channelStatus();
            break;
        case 0x05:
            printf("\r05: \r\n");
            duart_sendRawByte(0xE4);
            displayPrompt();
            break;
        case 0x0F:
            printf("\r0F: \r\n");
            displayPrompt();
            break;
        case 0x1C:
            printf("\r1C: \r\n");
            displayPrompt();
            break;
        case 0x1E:
            printf("\r1E: \r\n");
            duart_sendRawByte(0x10);
            duart_sendRawByte(0xE1);
            displayPrompt();
            break;
        case 0x81:
            printf("\r81: \r\n");
            duart_sendRawByte(0x10);
            duart_sendRawByte(0x06);
            displayPrompt();
            break;
        case 0x82: // Read Status?
            printf("\r82: Get Ready\r\n");
            getReady();
            displayPrompt();
            break;
        case 0x83: // Initialize
            printf("\r83: Initialize\r\n");
            adapterInit();
            displayPrompt();
            break;
        case 0x84: // Packet Request
            printf("\r84: Packet Request\r\n");
            packetRequest();
            displayPrompt();
            break;
        case 0x85: // Change Channel
            printf("\r85: Change Channel\r\n");
            changeChannel();
            displayPrompt();
            break;
        case 0x8F:
            printf("\r8F: \r\n");
            displayPrompt();
            break;
        }
    }
}

int main() {
    // Adjust system clock for better dividing into other clocks
    set_sys_clock_khz(CONFIG_SYSCLOCK*1000, true);

    stdio_usb_init();

    // Try mounting the LittleFS, or format if it isn't there.
    if(pico_mount(0) != LFS_ERR_OK) {
        if(pico_mount(1) != LFS_ERR_OK) {
            printf("ERROR: Unable to mount or format LittleFS.\r\n");
        } else {
            printf("LittleFS formatted.\r\n");
        }
    } else {
        readConfig();
    }

    wifiInit();

#if 0    
    if(!stdio_usb_connected()) {
        sleep_ms(5000);
    }
#endif

    if(!configured) {
        configWizard();
    } else {
        wifiConnect();
    }

    tftpStartup();

    duart_init(hardwaremode);

    adapterLoop();
}

