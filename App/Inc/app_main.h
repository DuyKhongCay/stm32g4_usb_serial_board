#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all App subsystems (Bridge, UART driver, RingBuffers).
 * Called once from main.c USER CODE BEGIN 2.
 */
void app_init(void);

/**
 * @brief Main periodic application loop.
 * Called continuously from main.c while(1) loop.
 */
void app_loop(void);

/**
 * @brief Hook called when UART DMA TX completes for a given channel.
 * @param port_idx 0 for Zhongli (USART1), 1 for ST3215 (USART3)
 */
void app_on_uart_tx_complete(uint8_t port_idx);

/**
 * @brief Hook called when UART IDLE line interrupt fires for a given channel.
 * @param port_idx 0 for Zhongli (USART1), 1 for ST3215 (USART3)
 */
void app_on_uart_rx_idle(uint8_t port_idx);

/**
 * @brief Hook called when USB CDC receives a packet from Host PC.
 * @param port_idx 0 for Zhongli (CDC0), 1 for ST3215 (CDC1)
 */
void app_on_usb_rx(uint8_t port_idx, const uint8_t* buf, uint32_t len);

/**
 * @brief Hook called when USB CDC finishes transmitting a packet to Host PC.
 * @param port_idx 0 for Zhongli (CDC0), 1 for ST3215 (CDC1)
 */
void app_on_usb_tx_complete(uint8_t port_idx);

#ifdef __cplusplus
}
#endif

#endif // APP_MAIN_H

