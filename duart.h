#define duart_pio pio0

enum {
    U2_TX_SM = 0,
    U2_RX_SM = 1,
    U3_TX_SM = 2,
    U3_RX_SM = 3,
};

extern uint16_t __time_critical_func(duart_readByteNonblocking)();
extern uint8_t __time_critical_func(duart_readByteBlocking)();
extern uint16_t __time_critical_func(duart_readByteExpected)(uint8_t ExpectedChar, uint32_t Timeout);
extern uint16_t __time_critical_func(duart_readByteTimeout)(uint32_t Timeout);
extern void __time_critical_func(duart_sendByte)(uint8_t ch);
extern void duart_init();
