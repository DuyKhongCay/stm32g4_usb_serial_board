#include "half_duplex_uart_channel.hpp"
#include "usart.h"
#include <cstring>

HalfDuplexUartChannel g_uart_zhongli(&huart1, USART1);
HalfDuplexUartChannel g_uart_st3215(&huart3, USART3);

HalfDuplexUartChannel::HalfDuplexUartChannel(UART_HandleTypeDef* huart, USART_TypeDef* uart_instance)
    : huart_(huart), uart_instance_(uart_instance) {}

void HalfDuplexUartChannel::init() {
    last_dma_rx_head_ = 0;
    tx_busy_ = false;
    rx_ring_.clear();
}

void HalfDuplexUartChannel::start_rx() {
    if (!huart_ || !uart_instance_) {
        return;
    }
    last_dma_rx_head_ = 0;

    // Start continuous Circular DMA Reception
    HAL_UART_Receive_DMA(huart_, dma_rx_buffer_.data(), static_cast<uint16_t>(dma_rx_buffer_.size()));

    // Enable UART IDLE Line Interrupt to detect end-of-frame
    __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);
}

bool HalfDuplexUartChannel::transmit(const uint8_t* data, size_t len) {
    if (!data || len == 0 || len > dma_tx_buffer_.size() || tx_busy_ || !huart_ || !uart_instance_) {
        return false;
    }

    std::memcpy(dma_tx_buffer_.data(), data, len);
    tx_busy_ = true;

    // 1. HARDWARE RE MUTING: Disable Receiver to eliminate self-echo on single-wire line
    uart_instance_->CR1 &= ~USART_CR1_RE;

    // 2. Launch Non-blocking DMA Transmit
    if (HAL_UART_Transmit_DMA(huart_, dma_tx_buffer_.data(), static_cast<uint16_t>(len)) != HAL_OK) {
        // Re-enable RX if transmit launch failed
        uart_instance_->CR1 |= USART_CR1_RE;
        tx_busy_ = false;
        return false;
    }

    return true;
}

void HalfDuplexUartChannel::on_tx_complete_isr() {
    if (uart_instance_) {
        // UNMUTE RECEIVER: Turn Receiver back on immediately in <10ns
        uart_instance_->CR1 |= USART_CR1_RE;
    }
    tx_busy_ = false;
}

void HalfDuplexUartChannel::on_rx_idle_isr() {
    if (!huart_ || !huart_->hdmarx) {
        return;
    }

    // Current position written by DMA circular buffer
    const size_t current_dma_pos = dma_rx_buffer_.size() - __HAL_DMA_GET_COUNTER(huart_->hdmarx);

    if (current_dma_pos != last_dma_rx_head_) {
        if (current_dma_pos > last_dma_rx_head_) {
            // Linear segment
            const size_t len = current_dma_pos - last_dma_rx_head_;
            rx_ring_.write(&dma_rx_buffer_[last_dma_rx_head_], len);
        } else {
            // Circular wrap-around
            const size_t part1 = dma_rx_buffer_.size() - last_dma_rx_head_;
            const size_t part2 = current_dma_pos;
            rx_ring_.write(&dma_rx_buffer_[last_dma_rx_head_], part1);
            if (part2 > 0) {
                rx_ring_.write(&dma_rx_buffer_[0], part2);
            }
        }
        last_dma_rx_head_ = current_dma_pos;
    }
}

