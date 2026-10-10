#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "stm32g4xx_hal.h"
#include "spsc_ring_buffer.hpp"

class HalfDuplexUartChannel {
public:
    static constexpr size_t DmaBufferSize = 256;
    static constexpr size_t RingBufferSize = 1024;

    HalfDuplexUartChannel(UART_HandleTypeDef* huart, USART_TypeDef* uart_instance);

    void init();
    void start_rx();

    bool transmit(const uint8_t* data, size_t len);

    void on_tx_complete_isr();
    void on_rx_idle_isr();

    bool is_tx_busy() const { return tx_busy_; }
    SpscRingBuffer<uint8_t, RingBufferSize>& rx_ring() { return rx_ring_; }

private:
    UART_HandleTypeDef* huart_{nullptr};
    USART_TypeDef* uart_instance_{nullptr};

    alignas(4) std::array<uint8_t, DmaBufferSize> dma_rx_buffer_{};
    alignas(4) std::array<uint8_t, DmaBufferSize> dma_tx_buffer_{};

    size_t last_dma_rx_head_{0};
    volatile bool tx_busy_{false};

    SpscRingBuffer<uint8_t, RingBufferSize> rx_ring_{};
};

extern HalfDuplexUartChannel g_uart_zhongli;
extern HalfDuplexUartChannel g_uart_st3215;

