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
#include "duart.h"
#include "config.h"

cyw43_ev_scan_result_t networks[16];
int wifi_network_count = 0;
uint32_t next_request = -1;

uint32_t dummy;
void *dummyhandle = (void*)&dummy;

ip_addr_t tftpserver;

static bool handling_request = false;
static bool request_handled = false;

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        for(int i = 0; i < wifi_network_count; i++) {
            if(!strcmp(networks[i].ssid, result->ssid)) {
                return 0;
            }
        }
        if(wifi_network_count >= (sizeof(networks) / sizeof(cyw43_ev_scan_result_t)))
            return 0;
        memcpy(&networks[wifi_network_count++], result, sizeof(cyw43_ev_scan_result_t));

        printf("SSID: %-32s\r\n\t\tSignal Strength: %4d  Channel: %3d  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
            result->ssid, result->rssi, result->channel,
            result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5]);
        if(result->auth_mode >= 4) {
            printf(" Security: WPA2\r\n\n");
        } else if(result->auth_mode >= 2) {
            printf(" Security: WPA1\r\n\n");
        } else if(result->auth_mode >= 1) {
            printf(" Security: WEP\r\n\n");
        } else {
            printf(" Security: OPEN\r\n\n");
        }
    }
    return 0;
}

static void* tftp_open(const char* fname, const char* mode, u8_t is_write) {
    LWIP_UNUSED_ARG(fname);
    LWIP_UNUSED_ARG(mode);
    LWIP_UNUSED_ARG(is_write);

    return dummyhandle;
}

static void tftp_segment_open() {
    PacketMode = 0;
    SegmentWritePtr = 0;
    PacketOffset = 0;
    PacketNumber = 0;
}

static void tftp_packet_open() {
    PacketMode = 1;
    PacketWritePtr = 0;
    SegmentWritePtr = 0;

    PacketPrologSent = false;
}

static int tftp_read(void* handle, void* buf, int bytes) {
    LWIP_UNUSED_ARG(handle);
    LWIP_UNUSED_ARG(buf);
    return bytes;
}

static void tftp_close(void* handle) {
    LWIP_UNUSED_ARG(handle);

    if(SegmentNumber == 0) return;

    if(PacketMode) {
        if(!PacketPrologSent) {
            sendUnauthorized();
        } else {
            sendPacketEpilog();
        }
    } else {
        if(!SegmentWritePtr) {
            sendUnauthorized();
        } else {
            printf("Segment is %d bytes.\r\n", SegmentWritePtr);
            PacketizeSegment();
            handling_request = false;
            return;
        }
    }

    SegmentNumber = 0;
    PacketNumber = 0;
    PacketOffset = 0;
    handling_request = false;
    PacketPrologSent = false;
}


static int tftp_write(void* handle, struct pbuf* p) {
    int i;

    if(PacketMode) {
        if(!PacketPrologSent) {
            PacketPrologSent = true;
            duart_sendRawByte(0x91);
            duart_readByteExpected(0x10, 0x7FFFFFF);
            duart_readByteExpected(0x06, 0x7FFFFFF);
        }
        while (p != NULL) {
            duart_sendBytes((uint8_t*)p->payload, p->len);
            p = p->next;
        }
    } else {
        while (p != NULL) {
            i = 0;
            while(i < p->len) {
                SegmentBuffer[SegmentWritePtr++] = ((uint8_t*)p->payload)[i++];
            }
            p = p->next;
        }
    }

    return 0;
}

static void tftp_error(void* handle, int err, const char* msg, int size) {
    char message[100];

    LWIP_UNUSED_ARG(handle);

    memset(message, 0, sizeof(message));
    MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

    printf("TFTP error: %d (%s)\r\n", err, message);
}

bool handleRequest(uint32_t request) {
    char filename[64];
    err_t err;

    if(!PacketMode && (SegmentNumber == (request >> 8))) {
        PacketNumber = request & 0xFF;        
        return PacketizeSegment();
    }

    SegmentNumber = request >> 8;
    SegmentWritePtr = 0;

    if(SegmentNumber == 0x7fffff) {
        printf("Sending time packet.\r\n");
        return sendTimePacket();
    }

    request_handled = false;
    handling_request = true;

    // if this is a reqest for the first packet of a segment, try to get the entire segment
    if((request & 0xff) == 0) {
        snprintf(filename, sizeof(filename)-1, "/nabu/%06x.nabu", SegmentNumber);
        printf("Submitting TFTP Get for %s:%s\r\n", tftp_server_ip, filename);

        tftp_segment_open();
        err = tftp_get(dummyhandle, &tftpserver, TFTP_PORT, filename, TFTP_MODE_OCTET);
        if(err == ERR_OK) {
            while(handling_request) {
                sleep_ms(500);
            }
            request_handled = true;
        }
    }

    if(!request_handled) {
        // .nabu file wasn't found, try looking for pre-packetized files
        snprintf(filename, sizeof(filename)-1, "/nabu/%08x.pak", request);
        printf("Submitting TFTP Get for %s:%s\r\n", tftp_server_ip, filename);

        tftp_packet_open();
        err = tftp_get(dummyhandle, &tftpserver, TFTP_PORT, filename, TFTP_MODE_OCTET);
        if(err == ERR_OK) {
            while(handling_request) {
                sleep_ms(500);
            }
            request_handled = true;
        }
    }

    if(!request_handled) {
        printf("TFTP Get for segment %08x failed.\r\n", request);

        duart_sendRawByte(0x90);
        duart_sendRawByte(0x10);
        duart_sendRawByte(0x06);
    }

    return request_handled;
}

static const struct tftp_context tftp = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write,
  tftp_error
};

void wifiInit() {
    if (cyw43_arch_init()) {
        if(stdio_usb_connected())
            printf("WiFi Failed to Init.\r\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
}

void wifiScan() {
    cyw43_wifi_scan_options_t scan_options = {0};
    wifi_network_count = 0;

    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
    if (err == 0) {
        printf("\r\nScanning for WiFi Networks...\r\n");
     } else {
        printf("Failed to start scan: %d\r\n", err);
    }

    while(cyw43_wifi_scan_active(&cyw43_state)) {
        sleep_ms(1000);
    }
}

int wifiConnect() {
    printf("Connecting to WiFi Network '%s'...\r\n", wifi_ssid);
    if (cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_psk, wifi_auth, 30000)) {
        printf("WiFi Failed to Connect.\r\n");
        return 1;
    } else {
        printf("WiFi Connected.\r\n");
        return 0;
    }
}

void tftpStartup() {
    int ret;
    err_t err;

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
}

