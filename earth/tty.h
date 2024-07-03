#define TTY_QUEUE_SIZE  128

#define UART_TX_INTR 1
#define UART_RX_INTR 2

#define UART_TXDATA 0UL
#define UART_RXDATA 4UL
#define UART_TXCTRL 8UL
#define UART_RXCTRL 12UL
#define UART_IE     16UL
#define UART_IP     20UL
#define UART_DIV    24UL

#define SPECIAL_CTRL_C   3
#define RET_SPECIAL_CHAR 2