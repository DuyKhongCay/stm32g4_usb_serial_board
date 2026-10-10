#include "dual_usb_bridge.hpp"
#include "main.h"

DualUsbBridge g_dual_bridge;

DualUsbBridge::DualUsbBridge() = default;

void DualUsbBridge::init() {
    for (auto& ch : channels_) {
        ch.host_to_uart_ring.clear();
        ch.usb_pending_len = 0;
    }
    last_heartbeat_ms_ = HAL_GetTick();
}

void DualUsbBridge::on_usb_rx(uint8_t port, const uint8_t* buf, uint32_t len) {
    if (port >= CDC_DUAL_NUM_PORTS || !buf || len == 0) {
        return;
    }
    channels_[port].host_to_uart_ring.write(buf, len);
}

void DualUsbBridge::on_usb_tx_complete(uint8_t port) {
    (void)port;
    /* Port TxState is automatically cleared in USBD_CDC_Dual_DataIn */
}

void DualUsbBridge::process_channel(uint8_t port, HalfDuplexUartChannel& uart_channel) {
    auto& ch = channels_[port];

    // 1. DIRECTION: Servos UART RX -> Host USB CDC TX
    if (!USBD_CDC_Dual_IsTxBusy(port)) {
        if (ch.usb_pending_len > 0) {
            // Retry pending packet if previous transmission hit USBD_BUSY
            const uint8_t res = USBD_CDC_Dual_TransmitPacket(port, ch.usb_in_staging_buf.data(), static_cast<uint16_t>(ch.usb_pending_len));
            if (res == USBD_OK) {
                ch.usb_pending_len = 0;
            }
        } else {
            const size_t available = uart_channel.rx_ring().available();
            if (available > 0) {
                const size_t to_read = (available > UsbPacketSize) ? UsbPacketSize : available;
                const size_t read_bytes = uart_channel.rx_ring().read(ch.usb_in_staging_buf.data(), to_read);
                if (read_bytes > 0) {
                    const uint8_t res = USBD_CDC_Dual_TransmitPacket(port, ch.usb_in_staging_buf.data(), static_cast<uint16_t>(read_bytes));
                    if (res != USBD_OK) {
                        ch.usb_pending_len = read_bytes;
                    }
                }
            }
        }
    }

    // 2. DIRECTION: Host USB CDC RX -> Servos UART TX
    if (!uart_channel.is_tx_busy()) {
        const size_t available = ch.host_to_uart_ring.available();
        if (available > 0) {
            const size_t to_send = (available > MaxUartTxChunk) ? MaxUartTxChunk : available;
            const size_t bytes_read = ch.host_to_uart_ring.read(ch.uart_tx_staging_buf.data(), to_send);
            if (bytes_read > 0) {
                uart_channel.transmit(ch.uart_tx_staging_buf.data(), bytes_read);
            }
        }
    }
}

void DualUsbBridge::process() {
    // Process Channel 0: Zhongli
    process_channel(0, g_uart_zhongli);

    // Process Channel 1: Waveshare ST3215
    process_channel(1, g_uart_st3215);

    // Heartbeat LED (1 Hz: toggles every 500 ms)
    update_heartbeat();
}

void DualUsbBridge::update_heartbeat() {
    const uint32_t now = HAL_GetTick();
    if (now - last_heartbeat_ms_ >= 500) {
        last_heartbeat_ms_ = now;
        HAL_GPIO_TogglePin(LED_USER_GPIO_Port, LED_USER_Pin);
    }
}

