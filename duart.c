#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include "duart_tx.pio.h"
#include "duart_rx.pio.h"
#include "suart_tx.pio.h"
#include "suart_rx.pio.h"
#include "config.h"
#include "packet.h"
#include "duart.h"

uint8_t duart_mode = 0;

uint16_t __time_critical_func(duart_readByteNonblocking)() {
    switch(duart_mode) {
    case 0:
        if(uart_is_readable(uart0)) {
            return uart_getc(uart0);
        }
        return 0xffff;

    case 1:
        if(uart_is_readable(uart1)) {
            return uart_getc(uart1);
        }
        return 0xffff;

    case 2:
        return duart_rx_program_get(duart_pio, U2_RX_SM);

    case 3:
        return suart_rx_program_get(duart_pio, U2_RX_SM);
    }

    return 0xffff;
}

uint8_t __time_critical_func(duart_readByteBlocking)() {
    uint16_t data;
    for(;;) {
        data = duart_readByteNonblocking();
        if(data < 0x100) {
            return data;
        }
    }
}

uint16_t __time_critical_func(duart_readByteExpected)(uint8_t ExpectedChar, uint32_t Timeout) {
    uint16_t data;
    while(Timeout--) {
        data = duart_readByteNonblocking();
        if(data < 0x100) {
            if(data == ExpectedChar)
                return 0;

            printf("Unexpected byte %02x while waiting for %02x\r\n", data, ExpectedChar);

            return 1;
        }
    }

    printf("Timeout while waiting for %02x\r\n", ExpectedChar);

    return 2;
}

uint16_t __time_critical_func(duart_readByteTimeout)(uint32_t Timeout) {
    uint16_t data;
    while(Timeout--) {
        data = duart_readByteNonblocking();
        if(data < 0x100) {
            return data;
        }
    }
    return 0xffff;
}

void __time_critical_func(duart_sendRawByte)(uint8_t ch) {
    switch(duart_mode) {
    case 0:
        uart_putc_raw(uart0, ch);
        return;

    case 1:
        uart_putc_raw(uart1, ch);
        return;

    case 2:
        duart_tx_program_put(duart_pio, U2_TX_SM, ch);
        return;

    case 3:
        suart_tx_program_put(duart_pio, U2_TX_SM, ch);
        return;
    }
}

void __time_critical_func(duart_sendRawBytes)(uint8_t *buf, uint16_t size) {
    uint16_t i;

    for(i = 0; i < size; i++) {
        duart_sendRawByte(buf[i]);
    }
}

void __time_critical_func(duart_sendByte)(uint8_t ch) {
    if(ch == 0x10) {
        duart_sendRawByte(0x10);
        duart_sendRawByte(0x10);
    } else {
        duart_sendRawByte(ch);
    }
}

void __time_critical_func(duart_sendBytes)(uint8_t *buf, uint16_t size) {
    uint16_t i;

    for(i = 0; i < size; i++) {
        duart_sendByte(buf[i]);
    }
}


void duart_init(int hardwaremode) {
    uint txoffset, rxoffset;

    switch(hardwaremode >> 24) {
    case 0:
        duart_mode = 0;
        switch(hardwaremode & 3) {
        case 0:
            gpio_set_function(0, GPIO_FUNC_UART);
            gpio_set_function(1, GPIO_FUNC_UART);
            break;
        case 1:
            gpio_set_function(12, GPIO_FUNC_UART);
            gpio_set_function(13, GPIO_FUNC_UART);
            break;
        case 2:
            gpio_set_function(16, GPIO_FUNC_UART);
            gpio_set_function(17, GPIO_FUNC_UART);
            break;
        case 3:
            gpio_set_function(28, GPIO_FUNC_UART);
            gpio_set_function(29, GPIO_FUNC_UART);
            break;
        }
        printf("UART0 Baudrate: %d\r\n", uart_init(uart0, CONFIG_SERIAL_BAUD));
        uart_set_format(uart0, 8, 2, UART_PARITY_NONE);
        return;

    case 1:
        duart_mode = 1;
        switch(hardwaremode & 3) {
        case 0:
            gpio_set_function(4, GPIO_FUNC_UART);
            gpio_set_function(5, GPIO_FUNC_UART);
            break;
        case 1:
            gpio_set_function(8, GPIO_FUNC_UART);
            gpio_set_function(9, GPIO_FUNC_UART);
            break;
        case 2:
            gpio_set_function(20, GPIO_FUNC_UART);
            gpio_set_function(21, GPIO_FUNC_UART);
            break;
        case 3:
            gpio_set_function(24, GPIO_FUNC_UART);
            gpio_set_function(25, GPIO_FUNC_UART);
            break;
        }
        printf("UART1 Baudrate: %d\r\n", uart_init(uart1, CONFIG_SERIAL_BAUD));
        uart_set_format(uart1, 8, 2, UART_PARITY_NONE);
        return;

    case 2:
        duart_mode = 2;
        txoffset = pio_add_program(duart_pio, &duart_tx_program);
        rxoffset = pio_add_program(duart_pio, &duart_rx_program);

        duart_tx_program_init(duart_pio, U2_TX_SM, txoffset, (hardwaremode >> 8) & 0x1f, CONFIG_SERIAL_BAUD);
        duart_rx_program_init(duart_pio, U2_RX_SM, rxoffset, (hardwaremode >> 0) & 0x1f, CONFIG_SERIAL_BAUD);
        return;

    case 3:
        duart_mode = 3;
        txoffset = pio_add_program(duart_pio, &suart_tx_program);
        rxoffset = pio_add_program(duart_pio, &suart_rx_program);

        suart_tx_program_init(duart_pio, U2_TX_SM, txoffset, (hardwaremode >> 8) & 0x1f, CONFIG_SERIAL_BAUD);
        suart_rx_program_init(duart_pio, U2_RX_SM, rxoffset, (hardwaremode >> 0) & 0x1f, CONFIG_SERIAL_BAUD);
        return;
    }

}
