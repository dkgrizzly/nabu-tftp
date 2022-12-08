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

typedef struct PacketHandle_s {
    uint16_t id;
    uint8_t *buffer;
    uint16_t writeptr;
} PacketHandle_t;

PacketHandle_t PacketHandle[MAXPACKET];

ip_addr_t tftpserver;

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
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

    printf("TFTP error: %d (%s)", err, message);
}

void handleRequest(uint32_t request) {
    char filename[64];
    void *f;
    err_t err;

    snprintf(filename, sizeof(filename)-1, "/nabu/%08x.pak", request);
    printf("Submitting TFTP Get for %s:%s\r\n", TFTP_SERVER_IP, filename);
    f = tftp_open(filename, 0, 1);
    if(f == NULL) {
        printf("Error allocating a packet buffer.\r\n");
    } else {
        err = tftp_get(f, &tftpserver, TFTP_PORT, filename, TFTP_MODE_OCTET);
        tftp_close(f);
        if(err != ERR_OK) {
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

    if (cyw43_arch_init()) {
        printf("WiFi Failed to Init.\r\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("WiFi Failed to Connect.\n");
        return;
    } else {
        printf("WiFi Connected.\n");
    }

    ret = ipaddr_aton(TFTP_SERVER_IP, &tftpserver);
    if(ret == 0) {
        printf("Parsing server address '%s' failed.\n", TFTP_SERVER_IP);
        return;
    }

    err = tftp_init_client(&tftp);
    if(err != ERR_OK) {
        printf("TFTP Client startup failed.\n");
        return;
    }

    multicore_launch_core1(wifiLoop);
}
