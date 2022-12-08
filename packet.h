
#define MAXREQUEST 4
#define MAXPACKET 8
#define PACKETSIZE 1024

typedef struct PacketHeader_s {
    uint8_t segment[3];
    uint8_t id;
    uint8_t owner;
    uint8_t tier[4];
    uint8_t mystery[2];
    uint8_t type;
    uint8_t number[2];
    uint8_t offset[2];
} PacketHeader_t;

typedef struct PacketPointer_s {
    uint16_t size;
    uint16_t buffer;
} PacketPointer_t;

typedef struct TimePacket_s {
    uint8_t data[9];
} TimePacket_t;

extern uint8_t PacketBuffer[MAXPACKET][PACKETSIZE];
extern uint8_t PacketState[MAXPACKET];

extern void initializePacketRequestQueue();
extern void schedulePacketRequest(uint32_t request);
extern int dequeuePacketRequest(uint32_t *request);

extern void initializePacketPayloadQueue();
extern void schedulePacketPayload(uint16_t buffer, uint16_t size);
extern void dequeuePacketPayload();

extern uint16_t newPacketBuffer();
extern void assemblePacket(uint16_t size, PacketHeader_t *header, void *data);
extern void newTimePacket();

