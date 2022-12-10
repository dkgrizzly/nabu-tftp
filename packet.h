#define MAXSEGMENTSIZE 65536
#define MAXPACKETSIZE 1024
#define MAXPAYLOAD 991

extern uint8_t SegmentBuffer[MAXSEGMENTSIZE];
extern uint16_t SegmentWritePtr;

extern uint8_t PacketBuffer[MAXPACKETSIZE];
extern int PacketMode;

extern bool PacketPrologSent;
extern uint32_t SegmentNumber;
extern uint16_t PacketWritePtr;
extern uint16_t PacketNumber;
extern uint16_t PacketOffset;

extern bool sendTimePacket();
extern bool PacketizeSegment();
extern bool sendPacketizedData(uint8_t *buf, uint16_t sz);
extern bool sendUnauthorized();
extern bool sendPacketProlog();
extern bool sendPacketEpilog();

