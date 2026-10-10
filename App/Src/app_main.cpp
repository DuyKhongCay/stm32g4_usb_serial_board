#include "app_main.h"
#include "half_duplex_uart_channel.hpp"
#include "dual_usb_bridge.hpp"

extern "C" {

void app_init(void) {
    g_uart_zhongli.init();
    g_uart_st3215.init();
    g_dual_bridge.init();

    // Start continuous Circular DMA Reception on both Half-Duplex buses
    g_uart_zhongli.start_rx();
    g_uart_st3215.start_rx();
}

void app_loop(void) {
    g_dual_bridge.process();
}

void app_on_uart_tx_complete(uint8_t port_idx) {
    if (port_idx == 0) {
        g_uart_zhongli.on_tx_complete_isr();
    } else if (port_idx == 1) {
        g_uart_st3215.on_tx_complete_isr();
    }
}

void app_on_uart_rx_idle(uint8_t port_idx) {
    if (port_idx == 0) {
        g_uart_zhongli.on_rx_idle_isr();
    } else if (port_idx == 1) {
        g_uart_st3215.on_rx_idle_isr();
    }
}

void app_on_usb_rx(uint8_t port_idx, const uint8_t* buf, uint32_t len) {
    g_dual_bridge.on_usb_rx(port_idx, buf, len);
}

void app_on_usb_tx_complete(uint8_t port_idx) {
    g_dual_bridge.on_usb_tx_complete(port_idx);
}

} // extern "C"
