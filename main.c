#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/util/queue.h>
#include <pico/bootrom.h>
#include <hardware/pio.h>
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"
#include "config.h"
#include "packet.h"
#include "duart.h"
#include "wifi.h"

uint16_t channelWord = 0;
int channelValid = 1;


void adapterInit() {
    duart_sendByte(0x10);
    duart_sendByte(0x06);
    duart_sendByte(0xE4);
}

void channelStatus() {
    if(!channelValid) {
        if(stdio_usb_connected())
            printf("Requesting Channel Code\r\n");
        duart_sendByte(0x9F); // Request Channel Code
        duart_sendByte(0x10);
        duart_sendByte(0xE1);
    } else {
        if(stdio_usb_connected())
            printf("Have Channel Code\r\n");
        duart_sendByte(0x1F); // Have Channel Code
        duart_sendByte(0x10);
        duart_sendByte(0xE1);
    }
}

void getReady() {
    duart_sendByte(0x10);
    duart_sendByte(0x06);
}

void changeChannel() {
    uint16_t data;

    duart_sendByte(0x10);
    duart_sendByte(0x06);

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

    duart_sendByte(0xE4);

    if(stdio_usb_connected())
        printf("New Channel Code: %04x\r\n", channelWord);

    channelValid = 1;
}

void packetRequest() {
    uint32_t request;
    uint16_t data;

    duart_sendByte(0x10);
    duart_sendByte(0x06);

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

    duart_sendByte(0xE4);

    if(stdio_usb_connected())
        printf("Packet Request: Segment %06x, Packet %02x\r\n", (request >> 8), (request & 0xff));

    schedulePacketRequest(request);
}

static void __time_critical_func(adapterLoop)() {
    for(;;) {
        uint16_t data = duart_readByteNonblocking();
        switch(data) {
        default:
            if(stdio_usb_connected())
                printf("%02x \r\n", data);
            break;
        case 0xFFFF:
        case 0xEFEF:
            break;
        case 0x01:
            if(stdio_usb_connected())
                printf("01: Channel Status\r\n");
            channelStatus();
            break;
        case 0x05:
            if(stdio_usb_connected())
                printf("05: \r\n");
            duart_sendByte(0xE4);
            break;
        case 0x0F:
            if(stdio_usb_connected())
                printf("0F: \r\n");
            break;
        case 0x1C:
            if(stdio_usb_connected())
                printf("1C: \r\n");
            break;
        case 0x1E:
            if(stdio_usb_connected())
                printf("1E: \r\n");
            duart_sendByte(0x10);
            duart_sendByte(0xE1);
            break;
        case 0x81:
            if(stdio_usb_connected())
                printf("81: \r\n");
            duart_sendByte(0x10);
            duart_sendByte(0x06);
            break;
        case 0x82: // Read Status?
            if(stdio_usb_connected())
                printf("82: Get Ready\r\n");
            getReady();
            break;
        case 0x83: // Initialize
            if(stdio_usb_connected())
                printf("83: Initialize\r\n");
            adapterInit();
            break;
        case 0x84: // Packet Request
            if(stdio_usb_connected())
                printf("84: Packet Request\r\n");
            packetRequest();
            break;
        case 0x85: // Change Channel
            if(stdio_usb_connected())
                printf("85: Change Channel\r\n");
            changeChannel();
            break;
        case 0x8F:
            if(stdio_usb_connected())
                printf("8F: \r\n");
            //duart_sendByte(0x10);
            //duart_sendByte(0x06);
            break;
        }

        // Check for queued packets ready to send
        dequeuePacketPayload();
    }
}

int main() {
    // Initializes the SYSCLOCK to 113MHz
    duart_init();

    //stdio_uart_init_full(uart0, 115200, 12, 13);
    stdio_usb_init();

    initializePacketRequestQueue();
    initializePacketPayloadQueue();

    wifiInit();

    if(stdio_usb_connected())
        printf("Pico Ready.\r\n");

    adapterLoop();
}

