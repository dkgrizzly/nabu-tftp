#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"
#include "config.h"
#include "packet.h"
#include "duart.h"

uint16_t __time_critical_func(duart_readByteNonblocking)() {
    return uart_rx_program_get(duart_pio, U2_RX_SM);
}

uint8_t __time_critical_func(duart_readByteBlocking)() {
    uint16_t data;
    for(;;) {
        data = uart_rx_program_get(duart_pio, U2_RX_SM);
        if(data < 0x100) {
            return data;
        }
    }
}

uint16_t __time_critical_func(duart_readByteExpected)(uint8_t ExpectedChar, uint32_t Timeout) {
    uint16_t data;
    while(Timeout--) {
        data = uart_rx_program_get(duart_pio, U2_RX_SM);
        if(data < 0x100) {
            if(data == ExpectedChar)
                return 0;

            if(stdio_usb_connected())
                printf("Unexpected byte %02x while waiting for %02x\r\n", data, ExpectedChar);

            return 1;
        }
    }

    if(stdio_usb_connected())
        printf("Timeout while waiting for %02x\r\n", ExpectedChar);

    return 2;
}

uint16_t __time_critical_func(duart_readByteTimeout)(uint32_t Timeout) {
    uint16_t data;
    while(Timeout--) {
        data = uart_rx_program_get(duart_pio, U2_RX_SM);
        if(data < 0x100) {
            return data;
        }
    }
    return 0xffff;
}

void __time_critical_func(duart_sendByte)(uint8_t ch) {
    uart_tx_program_put(duart_pio, U2_TX_SM, ch);
}

void duart_init() {
    // Adjust system clock for better dividing into other clocks
    set_sys_clock_khz(CONFIG_SYSCLOCK*1000, true);

    uint txoffset = pio_add_program(duart_pio, &uart_tx_program);
    uint rxoffset = pio_add_program(duart_pio, &uart_rx_program);

    uart_tx_program_init(duart_pio, U2_TX_SM, txoffset, CONFIG_PIN_U2_TX, CONFIG_SERIAL_BAUD);
    uart_rx_program_init(duart_pio, U2_RX_SM, rxoffset, CONFIG_PIN_U2_RX, CONFIG_SERIAL_BAUD);

    uart_tx_program_init(duart_pio, U3_TX_SM, txoffset, CONFIG_PIN_U3_TX, CONFIG_SERIAL_BAUD);
    uart_rx_program_init(duart_pio, U3_RX_SM, rxoffset, CONFIG_PIN_U3_RX, CONFIG_SERIAL_BAUD);
}

