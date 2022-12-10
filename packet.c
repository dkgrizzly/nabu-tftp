#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include "config.h"
#include "packet.h"
#include "duart.h"
#include "crctab.h"

uint8_t SegmentBuffer[MAXSEGMENTSIZE];
uint16_t SegmentWritePtr;

uint8_t PacketBuffer[MAXPACKETSIZE];
uint16_t PacketWritePtr;

int PacketMode;

bool PacketPrologSent = false;
uint32_t SegmentNumber;
uint16_t PacketNumber;
uint16_t PacketOffset;

uint16_t min16(uint16_t a, uint16_t b) {
    return (a < b) ? a : b;
}

uint16_t crc_update(uint16_t de, uint8_t a) {
    uint8_t bc;

    bc = (de >> 8) ^ a;

    de <<= 8;
    de ^= crc_table[bc];

    return de;
}

bool sendPacketProlog() {
    duart_sendRawByte(0x91);
    if(duart_readByteExpected(0x10, 0x7FFFFFF)) return false;
    if(duart_readByteExpected(0x06, 0x7FFFFFF)) return false;
    return true;
}

bool sendUnauthorized() {
    duart_sendRawByte(0x90);
    if(duart_readByteExpected(0x10, 0x7FFFFFF)) return false;
    if(duart_readByteExpected(0x06, 0x7FFFFFF)) return false;
    return true;
}

bool sendPacketEpilog() {
    duart_sendRawByte(0x10);
    duart_sendRawByte(0xE1);
    return true;
}

bool sendPacketizedData(uint8_t *buf, uint16_t sz) {
    if(!sendPacketProlog()) return false;
    duart_sendBytes(buf, sz);
    if(!sendPacketEpilog()) return false;
    return true;
}

#define TIMEPACKETSIZE (16+9+2)
bool sendTimePacket() {
    uint16_t crc = 0xffff;
    uint8_t data[TIMEPACKETSIZE];

    PacketBuffer[0] = 0x7F;
    PacketBuffer[1] = 0xFF;
    PacketBuffer[2] = 0xFF;
    PacketBuffer[3] = 0x00;
    PacketBuffer[4] = 0x00;
    PacketBuffer[5] = 0x7F;
    PacketBuffer[6] = 0xFF;
    PacketBuffer[7] = 0xFF;
    PacketBuffer[8] = 0xFF;
    PacketBuffer[9] = 0x7F;
    PacketBuffer[10] = 0x80;
    PacketBuffer[11] = 0x30;
    PacketBuffer[12] = 0x00;
    PacketBuffer[13] = 0x00;
    PacketBuffer[14] = 0x00;
    PacketBuffer[15] = 0x00;

    PacketBuffer[16] = 0x02;
    PacketBuffer[17] = 0x02;
    PacketBuffer[18] = 0x02;
    PacketBuffer[19] = 0x54;
    PacketBuffer[20] = 0x01;
    PacketBuffer[21] = 0x01;
    PacketBuffer[22] = 0x00;
    PacketBuffer[23] = 0x00;
    PacketBuffer[24] = 0x00;

    for(int i = 0; i < (TIMEPACKETSIZE-2); i++) {
        crc = crc_update(crc, PacketBuffer[i]);
    }

    PacketBuffer[25] = ((crc >> 8) & 0xff) ^ 0xff;
    PacketBuffer[26] = ((crc >> 0) & 0xff) ^ 0xff;

    printf("Sending Time Packet\r\n");

    return sendPacketizedData(PacketBuffer, TIMEPACKETSIZE);
}

bool ExpectPacketRequest(uint32_t ExpectedSegment, uint8_t ExpectedPacket) {
    uint32_t request;
    uint16_t data;
    bool failed = false;

    if(duart_readByteExpected(0x84, 0x7FFFFFF)) return false;

    duart_sendRawByte(0x10);
    duart_sendRawByte(0x06);

    failed |= (duart_readByteExpected(ExpectedPacket, 0x7FFFFFF)!=0);
    failed |= (duart_readByteExpected(((ExpectedSegment >> 0) & 0xFF), 0x7FFFFFF)!=0);
    failed |= (duart_readByteExpected(((ExpectedSegment >> 8) & 0xFF), 0x7FFFFFF)!=0);
    failed |= (duart_readByteExpected(((ExpectedSegment >> 16) & 0xFF), 0x7FFFFFF)!=0);

    duart_sendRawByte(0xE4);

    if(failed) {
        duart_sendRawByte(0xE4);
        sendUnauthorized();
    }

    return !failed;
}

bool PacketizeSegment() {
    uint8_t *buf = SegmentBuffer;
    uint16_t sz = SegmentWritePtr;
    
    uint16_t PayloadSize;

    bool LastPacket = false;
    uint16_t crc = 0xffff;
    uint16_t data;
    uint32_t request;
    int i;

    if((SegmentNumber == 0) || (SegmentNumber == -1)) {
        printf("Not sending an invalid segment\r\n");
        return false;
    }

    if(sz == 0) {
        printf("Not sending an empty segment\r\n");
        return false;
    }

    PacketOffset = PacketNumber * MAXPAYLOAD;
    buf += PacketOffset;
    sz -= PacketOffset;

    if(sz == 0) {
        printf("Not sending an empty segment\r\n");
        return false;
    }

    PacketWritePtr = 0;
    PayloadSize = min16(MAXPAYLOAD, sz);
    LastPacket = (PayloadSize == sz);
    sz -= PayloadSize;

    PacketBuffer[PacketWritePtr++] = (SegmentNumber >> 16) & 0xff; // Segment MSB
    PacketBuffer[PacketWritePtr++] = (SegmentNumber >>  8) & 0xff; //
    PacketBuffer[PacketWritePtr++] = (SegmentNumber >>  0) & 0xff; // Segment LSB
    PacketBuffer[PacketWritePtr++] = PacketNumber & 0xff;          // Packet Number LSB
    PacketBuffer[PacketWritePtr++] = 0x01;                         // Owner
    PacketBuffer[PacketWritePtr++] = 0x7f;                         // Tier MSB
    PacketBuffer[PacketWritePtr++] = 0xff;                         //
    PacketBuffer[PacketWritePtr++] = 0xff;                         //
    PacketBuffer[PacketWritePtr++] = 0xff;                         // Tier LSB
    PacketBuffer[PacketWritePtr++] = 0x7f;                         // Mystery
    PacketBuffer[PacketWritePtr++] = 0x80;                         // Mystery
    PacketBuffer[PacketWritePtr++] = (LastPacket ? 0x10 : 0x00) |
      ((PacketNumber == 0) ? 0xa1 : 0x20);     // Packet Type, bit 4 (0x10) marks end of segment
    PacketBuffer[PacketWritePtr++] = (PacketNumber >> 0) & 0xff;   // Packet Number LSB
    PacketBuffer[PacketWritePtr++] = (PacketNumber >> 8) & 0xff;   // Packet Number MSB
    PacketBuffer[PacketWritePtr++] = (PacketOffset >> 8) & 0xff;   // Offset MSB
    PacketBuffer[PacketWritePtr++] = (PacketOffset >> 0) & 0xff;   // Offset LSB

    // Copy the payload
    for(i = 0; i < PayloadSize; i++) {
        PacketBuffer[PacketWritePtr++] = *buf++;
    }

    // CRC the packet
    for(i = 0; i < PacketWritePtr; i++) {
        crc = crc_update(crc, PacketBuffer[i]);
    }

    // Write the check bytes
    PacketBuffer[PacketWritePtr++] = ((crc >> 8) & 0xff) ^ 0xff;
    PacketBuffer[PacketWritePtr++] = ((crc >> 0) & 0xff) ^ 0xff;

    printf("Sending %sPacket %02x of Segment %06x, %d bytes, %d payload bytes\r\n", (LastPacket ? "Last " : ""), PacketNumber, SegmentNumber, PacketWritePtr, PayloadSize);

    return sendPacketizedData(PacketBuffer, PacketWritePtr);
}
