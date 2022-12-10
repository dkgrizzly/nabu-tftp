#include <stdio.h>
#include <hardware/uart.h>
#include <hardware/vreg.h>
#include <hardware/clocks.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>
#include <lwip/apps/tftp_client.h>
#include "pico_hal.h"
#include "duart.h"
#include "config.h"
#include "wifi.h"

int configured = 0;

char wifi_ssid[64];
char wifi_psk[64];
uint32_t wifi_auth;
char tftp_server_ip[64];

int custom_fgets(char *s, int size, FILE *stream) {
    int i = 0;

    while((i < size-1)) {
        int c = getc(stream);
        if((c >= 0) && (c < 0x20)) {
            break;
        }
        s[i++] = c;
    }
    s[i] = 0;
    printf("\r\n");
    return i;
}

void writeConfig() {
    int file = pico_open("config", LFS_O_WRONLY | LFS_O_CREAT);
    if(file < 0) {
        printf("Unable to write config file.\r\n");
    } else {
        uint32_t config_ver = CONFIG_VER;
        pico_write(file, &config_ver, sizeof(config_ver));
        pico_write(file, &wifi_ssid, sizeof(wifi_ssid));
        pico_write(file, &wifi_psk, sizeof(wifi_psk));
        pico_write(file, &wifi_auth, sizeof(wifi_auth));
        pico_write(file, &tftp_server_ip, sizeof(tftp_server_ip));
        pico_write(file, &hardwaremode, sizeof(hardwaremode));
        pico_close(file);
    }
}

void readConfig() {
    int file = pico_open("config", LFS_O_RDONLY);
    if(file < 0) {
        printf("Not configured.\r\n");
    } else {
        uint32_t config_ver;
        pico_read(file, &config_ver, sizeof(config_ver));
        pico_read(file, &wifi_ssid, sizeof(wifi_ssid));
        pico_read(file, &wifi_psk, sizeof(wifi_psk));
        pico_read(file, &wifi_auth, sizeof(wifi_auth));
        pico_read(file, &tftp_server_ip, sizeof(tftp_server_ip));
        pico_read(file, &hardwaremode, sizeof(hardwaremode));
        configured = (config_ver == CONFIG_VER);
        pico_close(file);
    }
}

void configWizard() {
    char temptext[4];
    uint32_t pinconfig;
    uint32_t txpin;
    uint32_t rxpin;

    do {
        do {
            wifiScan();
            printf("Press [CR] to scan again.\r\n");
            printf("WiFi SSID ? ");
            custom_fgets(wifi_ssid, sizeof(wifi_ssid), stdin);
        } while(strlen(wifi_ssid) == 0);

        do {
            printf("WiFi Security Type [ 0: Open, 1:WPA1, 2:WPA2 ] ? ");
            custom_fgets(temptext, sizeof(temptext), stdin);
            wifi_auth = strtoul(temptext, NULL, 0);
            if(temptext[0] != (wifi_auth + '0'))
               wifi_auth = -1;
        } while(wifi_auth > 2);
        wifi_auth = (wifi_auth << 21) | (wifi_auth << 1);

        if(wifi_auth == 0) {
            memset(wifi_psk, 0, sizeof(wifi_psk));
        } else do {
            printf("WiFi Security Key ? ");
            custom_fgets(wifi_psk, sizeof(wifi_psk), stdin);
        } while (strlen(wifi_psk) == 0);
    } while(wifiConnect());

    do {
        printf("TFTP Server IP [ Type ? for help ] ? ");
        custom_fgets(tftp_server_ip, sizeof(tftp_server_ip), stdin);
        if(tftp_server_ip[0] == '?') {
            printf("The IPV4 address of the TFTP server on your local lan in dotted quad notation.\r\n");
            printf("Ex: 192.168.0.100\r\n");
            tftp_server_ip[0] = 0;
        }
    } while(strlen(tftp_server_ip) < 7);

    do {
        printf("UART Type [ Type ? for help ] ? ");
        custom_fgets(temptext, sizeof(temptext), stdin);
        if(temptext[0] == '?') {
            printf("0: UART0 with SE signalling\r\n");
            printf("1: UART1 with SE signalling\r\n");
            printf("2: PIO with LVD signalling\r\n");
            printf("3: PIO with SE signalling\r\n");
            hardwaremode = -1;
        } else {
            hardwaremode = strtoul(temptext, NULL, 0);
        }
    } while(hardwaremode >= 4);

    hardwaremode <<= 24;

    do {
        switch(hardwaremode >> 24) {
        case 0:
            printf("UART Pin Configuration [ Type ? for help ] ? ");
            custom_fgets(temptext, sizeof(temptext), stdin);
            if(temptext[0] == '?') {
                printf("0: RS422 Transceiver on pins  0 &  1\r\n");
                printf("1: RS422 Transceiver on pins 12 & 13\r\n");
                printf("2: RS422 Transceiver on pins 16 & 17\r\n");
                printf("3: RS422 Transceiver on pins 28 & 29\r\n");
                pinconfig = -1;
            } else {
                pinconfig = strtoul(temptext, NULL, 0);
                if(pinconfig > 3) {
                    pinconfig = -1;
                } else {
                    switch(pinconfig) {
                    case 0:
                        printf("UART0 on pins 0 & 1\r\n");
                        break;
                    case 1:
                        printf("UART0 on pins 12 & 13\r\n");
                        break;
                    case 2:
                        printf("UART0 on pins 16 & 17\r\n");
                        break;
                    case 3:
                        printf("UART0 on pins 28 & 29\r\n");
                        break;
                    }
                    hardwaremode |= pinconfig;
                }
            }
            break;
        case 1:
            printf("UART Pin Configuration [ Type ? for help ] ? ");
            custom_fgets(temptext, sizeof(temptext), stdin);
            if(temptext[0] == '?') {
                printf("0: RS422 Transceiver on pins  4 &  5\r\n");
                printf("1: RS422 Transceiver on pins  8 &  9\r\n");
                printf("2: RS422 Transceiver on pins 20 & 21\r\n");
                printf("3: RS422 Transceiver on pins 24 & 25\r\n");
                pinconfig = -1;
            } else {
                pinconfig = strtoul(temptext, NULL, 0);
                if(pinconfig > 3) {
                    pinconfig = -1;
                } else {
                    switch(pinconfig) {
                    case 0:
                        printf("UART1 on pins 4 & 5\r\n");
                        break;
                    case 1:
                        printf("UART1 on pins 8 & 9\r\n");
                        break;
                    case 2:
                        printf("UART1 on pins 20 & 21\r\n");
                        break;
                    case 3:
                        printf("UART1 on pins 24 & 25\r\n");
                        break;
                    }
                    hardwaremode |= pinconfig;
                }
            }
            break;
        case 2:
            printf("Pin for TX+ ? ");
            custom_fgets(temptext, sizeof(temptext), stdin);
            pinconfig = strtoul(temptext, NULL, 0);
            if(pinconfig > 28) {
                pinconfig = -1;
            } else {
                txpin = pinconfig;
                pinconfig = -1;

                printf("Pin for RX+ ? ");
                custom_fgets(temptext, sizeof(temptext), stdin);
                pinconfig = strtoul(temptext, NULL, 0);
                if(pinconfig > 28) {
                    pinconfig = -1;
                } else {
                    rxpin = pinconfig;
                    hardwaremode |= (txpin << 8) | rxpin;
                    pinconfig = 0;

                    printf("PIO Low Voltage Differential on pins %d,%d & %d,%d\r\n", txpin, txpin+1, rxpin, rxpin+1);
                }

            }
            break;
        case 3:
            printf("Pin for TX ? ");
            custom_fgets(temptext, sizeof(temptext), stdin);
            pinconfig = strtoul(temptext, NULL, 0);
            if(pinconfig > 29) {
                pinconfig = -1;
            } else {
                txpin = pinconfig;
                pinconfig = -1;

                printf("Pin for RX ? ");
                custom_fgets(temptext, sizeof(temptext), stdin);
                pinconfig = strtoul(temptext, NULL, 0);
                if(pinconfig > 29) {
                    pinconfig = -1;
                } else {
                    rxpin = pinconfig;
                    hardwaremode |= (txpin << 8) | rxpin;
                    pinconfig = 0;

                    printf("PIO Single Ended on pins %d & %d\r\n", txpin, rxpin);
                }
            }
            break;
        }
    } while(pinconfig == -1);

    writeConfig();

    configured = 1;
}
