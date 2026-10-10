#pragma once

#include "usbd_ioreq.h"
#include "usbd_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CDC_DUAL_NUM_PORTS          2U

/* CDC Endpoints configuration */
#define CDC0_IN_EP                  0x81U  /* EP1 IN: Data TX to Host */
#define CDC0_OUT_EP                 0x01U  /* EP1 OUT: Data RX from Host */
#define CDC0_CMD_EP                 0x82U  /* EP2 IN: Command/Notification */

#define CDC1_IN_EP                  0x83U  /* EP3 IN: Data TX to Host */
#define CDC1_OUT_EP                 0x03U  /* EP3 OUT: Data RX from Host */
#define CDC1_CMD_EP                 0x84U  /* EP4 IN: Command/Notification */

#define CDC_DATA_FS_MAX_PACKET_SIZE 64U
#define CDC_CMD_PACKET_SIZE         8U
#define CDC_CONFIG_DESC_SIZE        141U

/* CDC Channel Context */
typedef struct {
    uint32_t data[CDC_DATA_FS_MAX_PACKET_SIZE / 4U];
    uint8_t  CmdOpCode;
    uint8_t  CmdLength;
    uint8_t  *RxBuffer;
    uint8_t  *TxBuffer;
    uint32_t RxLength;
    uint32_t TxLength;
    volatile uint32_t TxState;
    volatile uint32_t RxState;
    USBD_CDC_LineCodingTypeDef line_coding;
} USBD_CDC_PortHandle;

/* Dual CDC Class Context */
typedef struct {
    USBD_CDC_PortHandle port[CDC_DUAL_NUM_PORTS];
} USBD_CDC_Dual_HandleTypeDef;

extern USBD_ClassTypeDef USBD_CDC_Dual;

/**
 * @brief Transmit packet on specified CDC port (0: Zhongli, 1: ST3215).
 * @return USBD_OK, USBD_BUSY, or USBD_FAIL
 */
uint8_t USBD_CDC_Dual_TransmitPacket(uint8_t port, const uint8_t *buf, uint16_t len);

/**
 * @brief Arm reception of next packet on specified CDC port.
 */
uint8_t USBD_CDC_Dual_ReceivePacket(uint8_t port);

/**
 * @brief Check if transmission endpoint is currently busy.
 */
uint8_t USBD_CDC_Dual_IsTxBusy(uint8_t port);

#ifdef __cplusplus
}
#endif
