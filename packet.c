#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include "config.h"
#include "packet.h"
#include "duart.h"

uint8_t PacketBuffer[MAXPACKET][PACKETSIZE];
uint8_t PacketState[MAXPACKET];

queue_t PacketRequestQueue;
queue_t PacketPayloadQueue;

void initializePacketRequestQueue() {
    queue_init(&PacketRequestQueue, MAXREQUEST, 4);
}

void schedulePacketRequest(uint32_t request) {
    if(request == 0x7FFFFF00) {
        newTimePacket();
    } else {
        queue_try_add(&PacketRequestQueue, &request);
    }
}

int dequeuePacketRequest(uint32_t *request) {
    return queue_try_remove(&PacketRequestQueue, request);
}

void initializePacketPayloadQueue() {
    queue_init(&PacketPayloadQueue, MAXPACKET, sizeof(PacketPointer_t));
}

void schedulePacketPayload(uint16_t buffer, uint16_t size) {
    PacketPointer_t PacketPointer;

    PacketPointer.size = size;
    PacketPointer.buffer = buffer;

    queue_try_add(&PacketPayloadQueue, &PacketPointer);
}

void dequeuePacketPayload() {
    PacketPointer_t PacketPointer;
    if(queue_try_remove(&PacketPayloadQueue, &PacketPointer)) {
        uint16_t size = PacketPointer.size;
        uint16_t buffer = PacketPointer.buffer;
        uint8_t *data = PacketBuffer[buffer];

        if(size == 0) {
            printf("\r\n90\r\n");
            duart_sendByte(0x90);
            duart_sendByte(0x10);
            sleep_ms(10);
            duart_sendByte(0xE1);
            sleep_ms(10);
        } else {
            printf("\r\n91\r\n");
            duart_sendByte(0x91);
            duart_readByteExpected(0x10, 0x7FFFFFF);
            duart_readByteExpected(0x06, 0x7FFFFFF);
            for(uint i = 0; i < size; i++) {
                printf("%02x ", data[i]);
                if(data[i] == 0x10) {
                    duart_sendByte(0x10);
                    busy_wait_us_32(100);
                    duart_sendByte(0x10);
                    busy_wait_us_32(100);
                } else {
                    duart_sendByte(data[i]);
                    busy_wait_us_32(100);
                }
                if((i & 0xf) == 0xf)
                    printf("\r\n");
            }
            printf("\r\n10 E1\r\n");
            duart_sendByte(0x10);
            busy_wait_us_32(100);
            duart_sendByte(0xE1);
            busy_wait_us_32(100);
        }
        PacketState[buffer] = 0;
    }
}

uint16_t newPacketBuffer() {
    uint16_t buffer;

    for(buffer = 0; buffer < MAXPACKET; buffer++) {
        if(PacketState[buffer] == 0) break;
    }
    if(buffer >= MAXPACKET) return 0xffff;
    PacketState[buffer] = 1;

    return buffer;
}

void assemblePacket(uint16_t size, PacketHeader_t *header, void *data) {
    uint16_t buffer = newPacketBuffer();
    uint16_t checksum = 0;

    if(buffer >= MAXPACKET) return;

    memcpy(PacketBuffer[buffer], header, 16);
    memcpy(PacketBuffer[buffer]+16, data, size);
    // TODO: calculate checksum
    memcpy(PacketBuffer[buffer]+16+size, &checksum, 2);

    schedulePacketPayload(buffer, size+18);
}

void newTimePacket() {
    uint16_t buffer = newPacketBuffer();
    uint16_t size = 9;
    uint16_t checksum = 0;
    PacketHeader_t header;
    TimePacket_t TimePacket;

    if(buffer >= MAXPACKET) return;

    header.segment[0] = 0x7F;
    header.segment[1] = 0xFF;
    header.segment[2] = 0xFF;
    header.id = 0x00;
    header.owner = 0x00;
    header.tier[0] = 0x7F;
    header.tier[1] = 0xFF;
    header.tier[2] = 0xFF;
    header.tier[3] = 0xFF;
    header.mystery[0] = 0x7F;
    header.mystery[1] = 0x80;
    header.type = 0x30;
    header.number[0] = 0x00;
    header.offset[1] = 0x00;
    header.number[0] = 0x00;
    header.offset[1] = 0x00;

    TimePacket.data[0] = 0x02;
    TimePacket.data[1] = 0x02;
    TimePacket.data[2] = 0x02;
    TimePacket.data[3] = 0x54;
    TimePacket.data[4] = 0x01;
    TimePacket.data[5] = 0x01;
    TimePacket.data[6] = 0x00;
    TimePacket.data[7] = 0x00;
    TimePacket.data[8] = 0x00;

    memcpy(PacketBuffer[buffer], &header, 16);
    memcpy(PacketBuffer[buffer]+16, &TimePacket, size);

    // TODO: calculate checksum
    PacketBuffer[buffer][16+size] = 0xd5;
    PacketBuffer[buffer][16+size] = 0x3b;

    schedulePacketPayload(buffer, size+18);
}

