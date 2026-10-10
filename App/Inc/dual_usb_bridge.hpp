#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "half_duplex_uart_channel.hpp"
#include "spsc_ring_buffer.hpp"
#include "usbd_cdc_dual.h"

class DualUsbBridge {
public:
    static constexpr size_t UsbPacketSize = 64;
    static constexpr size_t RingBufferSize = 1024;
    static constexpr size_t MaxUartTxChunk = 128;

    DualUsbBridge();

    void init();
    void process();

    void on_usb_rx(uint8_t port, const uint8_t* buf, uint32_t len);
    void on_usb_tx_complete(uint8_t port);

private:
    struct ChannelState {
        SpscRingBuffer<uint8_t, RingBufferSize> host_to_uart_ring{};
        alignas(4) std::array<uint8_t, UsbPacketSize> usb_in_staging_buf{};
        size_t usb_pending_len{0};
        alignas(4) std::array<uint8_t, MaxUartTxChunk> uart_tx_staging_buf{};
    };

    std::array<ChannelState, CDC_DUAL_NUM_PORTS> channels_{};
    uint32_t last_heartbeat_ms_{0};

    void process_channel(uint8_t port, HalfDuplexUartChannel& uart_channel);
    void update_heartbeat();
};

extern DualUsbBridge g_dual_bridge;

