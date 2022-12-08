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
#include "packet.h"
#include "pico_hal.h"

char wifi_ssid[64];
char wifi_psk[64];
uint32_t wifi_auth;
char tftp_server_ip[64];
int wifi_configured = 0;

typedef struct PacketHandle_s {
    uint16_t id;
    uint8_t *buffer;
    uint16_t writeptr;
} PacketHandle_t;

PacketHandle_t PacketHandle[MAXPACKET];

ip_addr_t tftpserver;

cyw43_ev_scan_result_t networks[32];
int wifi_network_count = 0;

static int custom_fgets(char *s, int size, FILE *stream) {
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

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        for(int i = 0; i < wifi_network_count; i++) {
            if(!strcmp(networks[i].ssid, result->ssid)) {
                return 0;
            }
        }
        memcpy(&networks[wifi_network_count++], result, sizeof(cyw43_ev_scan_result_t));
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\r\n",
            result->ssid, result->rssi, result->channel,
            result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
            result->auth_mode);
    }
    return 0;
}

static void* tftp_open(const char* fname, const char* mode, u8_t is_write) {
    uint32_t packet = newPacketBuffer();
    LWIP_UNUSED_ARG(fname);
    LWIP_UNUSED_ARG(mode);

    if(packet >= MAXPACKET) return NULL;

    PacketHandle[packet].id = packet;
    PacketHandle[packet].buffer = PacketBuffer[packet];
    PacketHandle[packet].writeptr = 0;

    return (void*)(&PacketHandle[packet]);
}

static void tftp_close(void* handle) {
    PacketHandle_t *packet = (PacketHandle_t *)handle;

    schedulePacketPayload(packet->id, packet->writeptr);
}

static int tftp_read(void* handle, void* buf, int bytes) {
    LWIP_UNUSED_ARG(handle);
    LWIP_UNUSED_ARG(buf);
    return bytes;
}

static int tftp_write(void* handle, struct pbuf* p) {
    PacketHandle_t *packet = (PacketHandle_t *)handle;

    while (p != NULL) {
        int i;
        for(i = 0; i < p->len; i++) {
            if(packet->writeptr > PACKETSIZE) return 0;
            packet->buffer[packet->writeptr] = ((uint8_t*)p->payload)[i];
            packet->writeptr++;
        }
        p = p->next;
    }

    return 0;
}

static void tftp_error(void* handle, int err, const char* msg, int size) {
    char message[100];

    LWIP_UNUSED_ARG(handle);

    memset(message, 0, sizeof(message));
    MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

    if(stdio_usb_connected())
        printf("TFTP error: %d (%s)\r\n", err, message);
}

void handleRequest(uint32_t request) {
    char filename[64];
    void *f;
    err_t err;

    snprintf(filename, sizeof(filename)-1, "/nabu/%08x.pak", request);
    if(stdio_usb_connected())
        printf("Submitting TFTP Get for %s:%s\r\n", tftp_server_ip, filename);
    f = tftp_open(filename, 0, 1);
    if(f == NULL) {
        if(stdio_usb_connected())
            printf("Error allocating a packet buffer.\r\n");
    } else {
        err = tftp_get(f, &tftpserver, TFTP_PORT, filename, TFTP_MODE_OCTET);
        tftp_close(f);
        if(err != ERR_OK) {
            if(stdio_usb_connected())
                printf("TFTP Get failed\r\n");
        }
    }
}

void wifiLoop() {
    uint32_t request;

    for(;;) {
        if(dequeuePacketRequest(&request)) {
            handleRequest(request);
        }
    }
}


static const struct tftp_context tftp = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write,
  tftp_error
};

void wifiInit() {
    int ret;
    err_t err;
    int i;

    if (cyw43_arch_init()) {
        if(stdio_usb_connected())
            printf("WiFi Failed to Init.\r\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    if(!wifi_configured) {
        absolute_time_t scan_test = nil_time;
        bool scan_in_progress = false;
        bool scanning = true;
        while(!wifi_configured) {
            wifi_network_count = 0;
            while(scanning) {
                if (absolute_time_diff_us(get_absolute_time(), scan_test) < 0) {
                    if (!scan_in_progress) {
                        cyw43_wifi_scan_options_t scan_options = {0};
                        int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
                        if (err == 0) {
                            printf("\r\nScanning for WiFi Networks...\r\n");
                            scan_in_progress = true;
                        } else {
                            printf("Failed to start scan: %d\r\n", err);
                            scan_test = make_timeout_time_ms(10000); // wait 10s and scan again
                        }
                    } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
                        scan_test = nil_time;
                        scan_in_progress = false;
                        scanning = false;
                    }
                }
            }
            printf("Press [CR] to scan again.\r\n");
            printf("WiFi SSID ? ");
            custom_fgets(wifi_ssid, sizeof(wifi_ssid), stdin);

            if(strlen(wifi_ssid) == 0) {
                scanning = true;
            } else {
                char temptext[4];
                do {
                    printf("WiFi Security Type [ 0: Open, 1:WPA1, 2:WPA2 ] ? ");
                    custom_fgets(temptext, sizeof(temptext), stdin);
                    wifi_auth = strtoul(temptext, NULL, 0);
                } while(wifi_auth > 2);
                wifi_auth = (wifi_auth << 21) | (wifi_auth << 1);

                if(wifi_auth != 0) {
                    printf("WiFi Security Key ? ");
                    custom_fgets(wifi_psk, sizeof(wifi_psk), stdin);
                } else {
                    memset(wifi_psk, 0, sizeof(wifi_psk));
                }

                printf("Connecting to WiFi Network '%s'...\r\n", wifi_ssid);
                if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_psk, wifi_auth, 30000)) {
                    printf("WiFi Failed to Connect.\r\n");
                    scanning = true;
                } else {
                    printf("WiFi Connected.\r\n");

                    do {
                        printf("TFTP Server IP ? ");
                        custom_fgets(tftp_server_ip, sizeof(tftp_server_ip), stdin);
                    } while(strlen(tftp_server_ip) < 7);

                    int file = pico_open("config", LFS_O_WRONLY | LFS_O_CREAT);
                    if(file < 0) {
                        printf("Unable to write config file.\r\n");
                    } else {
                        pico_write(file, &wifi_ssid, sizeof(wifi_ssid));
                        pico_write(file, &wifi_psk, sizeof(wifi_psk));
                        pico_write(file, &wifi_auth, sizeof(wifi_auth));
                        pico_write(file, &tftp_server_ip, sizeof(tftp_server_ip));
                        pico_close(file);
                    }

                    scanning = false;
                    wifi_configured = 1;
                }
            }
        }
    } else {
        if(stdio_usb_connected())
            printf("Connecting to WiFi Network '%s'...\r\n", wifi_ssid);
        if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_psk, wifi_auth, 30000)) {
            if(stdio_usb_connected())
                printf("WiFi Failed to Connect.\r\n");
            return;
        } else {
            if(stdio_usb_connected())
                printf("WiFi Connected.\r\n");
        }
    }

    ret = ipaddr_aton(tftp_server_ip, &tftpserver);
    if(ret == 0) {
        if(stdio_usb_connected())
            printf("Parsing server address '%s' failed.\r\n", tftp_server_ip);
        return;
    }

    err = tftp_init_client(&tftp);
    if(err != ERR_OK) {
        if(stdio_usb_connected())
            printf("TFTP Client startup failed.\r\n");
        return;
    }

    multicore_launch_core1(wifiLoop);
}
