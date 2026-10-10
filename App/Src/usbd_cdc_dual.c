#include "usbd_cdc_dual.h"
#include "usbd_ctlreq.h"
#include "app_main.h"

#define CDC_REQ_SET_LINE_CODING         0x20U
#define CDC_REQ_GET_LINE_CODING         0x21U
#define CDC_REQ_SET_CONTROL_LINE_STATE  0x22U

static uint8_t USBD_CDC_Dual_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_Dual_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_Dual_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_CDC_Dual_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_Dual_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_Dual_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t *USBD_CDC_Dual_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_Dual_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_CDC_Dual = {
    USBD_CDC_Dual_Init,
    USBD_CDC_Dual_DeInit,
    USBD_CDC_Dual_Setup,
    NULL,                 /* EP0_TxSent */
    USBD_CDC_Dual_EP0_RxReady,
    USBD_CDC_Dual_DataIn,
    USBD_CDC_Dual_DataOut,
    NULL,                 /* SOF */
    NULL,                 /* IsoINIncomplete */
    NULL,                 /* IsoOUTIncomplete */
    USBD_CDC_Dual_GetFSCfgDesc,
    USBD_CDC_Dual_GetFSCfgDesc,
    USBD_CDC_Dual_GetFSCfgDesc,
    USBD_CDC_Dual_GetDeviceQualifierDesc,
};

/* Static USB Device Handle pointer and Static Driver state */
static USBD_HandleTypeDef *s_pdev = NULL;
static USBD_CDC_Dual_HandleTypeDef s_dual_cdc;

/* Static Receive Buffers (64 bytes each, 4-byte aligned) */
static __ALIGN_BEGIN uint8_t s_rx_buf0[CDC_DATA_FS_MAX_PACKET_SIZE] __ALIGN_END;
static __ALIGN_BEGIN uint8_t s_rx_buf1[CDC_DATA_FS_MAX_PACKET_SIZE] __ALIGN_END;

/* USB CDC Composite configuration descriptor */
static __ALIGN_BEGIN uint8_t s_dual_cdc_cfg_desc[CDC_CONFIG_DESC_SIZE] __ALIGN_END = {
    /* Configuration Descriptor */
    0x09,                               /* bLength */
    USB_DESC_TYPE_CONFIGURATION,        /* bDescriptorType */
    LOBYTE(CDC_CONFIG_DESC_SIZE),       /* wTotalLength */
    HIBYTE(CDC_CONFIG_DESC_SIZE),
    0x04,                               /* bNumInterfaces (2 per CDC = 4 total) */
    0x01,                               /* bConfigurationValue */
    0x00,                               /* iConfiguration */
    0xC0,                               /* bmAttributes: Self Powered */
    0x32,                               /* MaxPower: 100 mA */

    /* ========================================================================= */
    /* CDC Port 0 (Zhongli Bus) - Interfaces 0 & 1                                */
    /* ========================================================================= */
    /* IAD (Interface Association Descriptor) */
    0x08,                               /* bLength */
    0x0B,                               /* bDescriptorType: IAD */
    0x00,                               /* bFirstInterface */
    0x02,                               /* bInterfaceCount */
    0x02,                               /* bFunctionClass: CDC */
    0x02,                               /* bFunctionSubClass: ACM */
    0x01,                               /* bFunctionProtocol: AT Commands */
    0x00,                               /* iFunction */

    /* Interface 0: Communication Control */
    0x09,                               /* bLength */
    USB_DESC_TYPE_INTERFACE,            /* bDescriptorType */
    0x00,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x01,                               /* bNumEndpoints */
    0x02,                               /* bInterfaceClass: CDC */
    0x02,                               /* bInterfaceSubClass: ACM */
    0x01,                               /* bInterfaceProtocol: AT Commands */
    0x00,                               /* iInterface */

    /* Header Functional Descriptor */
    0x05, 0x24, 0x00, 0x10, 0x01,
    /* Call Management Functional Descriptor */
    0x05, 0x24, 0x01, 0x00, 0x01,
    /* ACM Functional Descriptor */
    0x04, 0x24, 0x02, 0x02,
    /* Union Functional Descriptor (Control: 0, Data: 1) */
    0x05, 0x24, 0x06, 0x00, 0x01,

    /* Endpoint 2 IN (CDC0 Command Interrupt) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC0_CMD_EP,                        /* bEndpointAddress */
    0x03,                               /* bmAttributes: Interrupt */
    LOBYTE(CDC_CMD_PACKET_SIZE),
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x10,                               /* bInterval: 16 ms */

    /* Interface 1: Data Class */
    0x09,                               /* bLength */
    USB_DESC_TYPE_INTERFACE,            /* bDescriptorType */
    0x01,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x02,                               /* bNumEndpoints */
    0x0A,                               /* bInterfaceClass: CDC Data */
    0x00,                               /* bInterfaceSubClass */
    0x00,                               /* bInterfaceProtocol */
    0x00,                               /* iInterface */

    /* Endpoint 1 OUT (CDC0 Data OUT) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC0_OUT_EP,                        /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               /* bInterval */

    /* Endpoint 1 IN (CDC0 Data IN) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC0_IN_EP,                         /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               /* bInterval */

    /* ========================================================================= */
    /* CDC Port 1 (Waveshare ST3215 Bus) - Interfaces 2 & 3                      */
    /* ========================================================================= */
    /* IAD (Interface Association Descriptor) */
    0x08,                               /* bLength */
    0x0B,                               /* bDescriptorType: IAD */
    0x02,                               /* bFirstInterface */
    0x02,                               /* bInterfaceCount */
    0x02,                               /* bFunctionClass: CDC */
    0x02,                               /* bFunctionSubClass: ACM */
    0x01,                               /* bFunctionProtocol: AT Commands */
    0x00,                               /* iFunction */

    /* Interface 2: Communication Control */
    0x09,                               /* bLength */
    USB_DESC_TYPE_INTERFACE,            /* bDescriptorType */
    0x02,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x01,                               /* bNumEndpoints */
    0x02,                               /* bInterfaceClass: CDC */
    0x02,                               /* bInterfaceSubClass: ACM */
    0x01,                               /* bInterfaceProtocol: AT Commands */
    0x00,                               /* iInterface */

    /* Header Functional Descriptor */
    0x05, 0x24, 0x00, 0x10, 0x01,
    /* Call Management Functional Descriptor */
    0x05, 0x24, 0x01, 0x00, 0x03,
    /* ACM Functional Descriptor */
    0x04, 0x24, 0x02, 0x02,
    /* Union Functional Descriptor (Control: 2, Data: 3) */
    0x05, 0x24, 0x06, 0x02, 0x03,

    /* Endpoint 4 IN (CDC1 Command Interrupt) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC1_CMD_EP,                        /* bEndpointAddress */
    0x03,                               /* bmAttributes: Interrupt */
    LOBYTE(CDC_CMD_PACKET_SIZE),
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x10,                               /* bInterval: 16 ms */

    /* Interface 3: Data Class */
    0x09,                               /* bLength */
    USB_DESC_TYPE_INTERFACE,            /* bDescriptorType */
    0x03,                               /* bInterfaceNumber */
    0x00,                               /* bAlternateSetting */
    0x02,                               /* bNumEndpoints */
    0x0A,                               /* bInterfaceClass: CDC Data */
    0x00,                               /* bInterfaceSubClass */
    0x00,                               /* bInterfaceProtocol */
    0x00,                               /* iInterface */

    /* Endpoint 3 OUT (CDC1 Data OUT) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC1_OUT_EP,                        /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,                               /* bInterval */

    /* Endpoint 3 IN (CDC1 Data IN) */
    0x07,                               /* bLength */
    USB_DESC_TYPE_ENDPOINT,             /* bDescriptorType */
    CDC1_IN_EP,                         /* bEndpointAddress */
    0x02,                               /* bmAttributes: Bulk */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00                                /* bInterval */
};

/* Device Qualifier Descriptor */
static __ALIGN_BEGIN uint8_t s_device_qualifier[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0xEF,
    0x02,
    0x01,
    0x40,
    0x01,
    0x00
};

static uint8_t USBD_CDC_Dual_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    s_pdev = pdev;
    pdev->pClassData = &s_dual_cdc;

    /* Initialize Port 0 */
    s_dual_cdc.port[0].RxBuffer = s_rx_buf0;
    s_dual_cdc.port[0].TxState = 0;
    s_dual_cdc.port[0].RxState = 0;
    s_dual_cdc.port[0].line_coding.bitrate = 1000000;
    s_dual_cdc.port[0].line_coding.format = 0;
    s_dual_cdc.port[0].line_coding.paritytype = 0;
    s_dual_cdc.port[0].line_coding.datatype = 8;

    /* Initialize Port 1 */
    s_dual_cdc.port[1].RxBuffer = s_rx_buf1;
    s_dual_cdc.port[1].TxState = 0;
    s_dual_cdc.port[1].RxState = 0;
    s_dual_cdc.port[1].line_coding.bitrate = 1000000;
    s_dual_cdc.port[1].line_coding.format = 0;
    s_dual_cdc.port[1].line_coding.paritytype = 0;
    s_dual_cdc.port[1].line_coding.datatype = 8;

    /* Open Port 0 Endpoints */
    (void)USBD_LL_OpenEP(pdev, CDC0_IN_EP, USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
    (void)USBD_LL_OpenEP(pdev, CDC0_OUT_EP, USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
    (void)USBD_LL_OpenEP(pdev, CDC0_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);

    /* Open Port 1 Endpoints */
    (void)USBD_LL_OpenEP(pdev, CDC1_IN_EP, USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
    (void)USBD_LL_OpenEP(pdev, CDC1_OUT_EP, USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
    (void)USBD_LL_OpenEP(pdev, CDC1_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);

    /* Arm reception on both OUT endpoints */
    (void)USBD_LL_PrepareReceive(pdev, CDC0_OUT_EP, s_rx_buf0, CDC_DATA_FS_MAX_PACKET_SIZE);
    (void)USBD_LL_PrepareReceive(pdev, CDC1_OUT_EP, s_rx_buf1, CDC_DATA_FS_MAX_PACKET_SIZE);

    return USBD_OK;
}

static uint8_t USBD_CDC_Dual_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    (void)USBD_LL_CloseEP(pdev, CDC0_IN_EP);
    (void)USBD_LL_CloseEP(pdev, CDC0_OUT_EP);
    (void)USBD_LL_CloseEP(pdev, CDC0_CMD_EP);

    (void)USBD_LL_CloseEP(pdev, CDC1_IN_EP);
    (void)USBD_LL_CloseEP(pdev, CDC1_OUT_EP);
    (void)USBD_LL_CloseEP(pdev, CDC1_CMD_EP);

    pdev->pClassData = NULL;
    s_pdev = NULL;
    return USBD_OK;
}

static uint8_t USBD_CDC_Dual_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req) {
    uint8_t port = (req->wIndex >= 2U) ? 1U : 0U;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS:
            if (req->wLength != 0U) {
                if ((req->bmRequest & 0x80U) != 0U) {
                    /* Device-to-Host */
                    if (req->bRequest == CDC_REQ_GET_LINE_CODING) {
                        (void)USBD_CtlSendData(pdev, (uint8_t *)&s_dual_cdc.port[port].line_coding, 7U);
                    }
                } else {
                    /* Host-to-Device */
                    if (req->bRequest == CDC_REQ_SET_LINE_CODING) {
                        s_dual_cdc.port[port].CmdOpCode = req->bRequest;
                        s_dual_cdc.port[port].CmdLength = (uint8_t)req->wLength;
                        (void)USBD_CtlPrepareRx(pdev, (uint8_t *)&s_dual_cdc.port[port].line_coding, req->wLength);
                    }
                }
            } else {
                /* Zero-length control request (e.g., SET_CONTROL_LINE_STATE) */
                (void)USBD_CtlSendStatus(pdev);
            }
            break;

        default:
            break;
    }
    return USBD_OK;
}

static uint8_t USBD_CDC_Dual_EP0_RxReady(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return USBD_OK;
}

static uint8_t USBD_CDC_Dual_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    (void)pdev;
    uint8_t port = 0U;
    if (epnum == (CDC0_IN_EP & 0x7FU)) {
        port = 0U;
    } else if (epnum == (CDC1_IN_EP & 0x7FU)) {
        port = 1U;
    } else {
        return USBD_OK;
    }

    s_dual_cdc.port[port].TxState = 0U;
    app_on_usb_tx_complete(port);
    return USBD_OK;
}

static uint8_t USBD_CDC_Dual_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    uint8_t port = 0U;
    uint8_t *rx_buf = NULL;
    uint8_t out_ep = 0U;

    if (epnum == CDC0_OUT_EP) {
        port = 0U;
        rx_buf = s_rx_buf0;
        out_ep = CDC0_OUT_EP;
    } else if (epnum == CDC1_OUT_EP) {
        port = 1U;
        rx_buf = s_rx_buf1;
        out_ep = CDC1_OUT_EP;
    } else {
        return USBD_OK;
    }

    const uint32_t rx_len = USBD_LL_GetRxDataSize(pdev, epnum);
    if (rx_len > 0U) {
        app_on_usb_rx(port, rx_buf, rx_len);
    }

    /* Re-arm reception for next packet */
    (void)USBD_LL_PrepareReceive(pdev, out_ep, rx_buf, CDC_DATA_FS_MAX_PACKET_SIZE);
    return USBD_OK;
}

static uint8_t *USBD_CDC_Dual_GetFSCfgDesc(uint16_t *length) {
    *length = (uint16_t)sizeof(s_dual_cdc_cfg_desc);
    return s_dual_cdc_cfg_desc;
}

static uint8_t *USBD_CDC_Dual_GetDeviceQualifierDesc(uint16_t *length) {
    *length = (uint16_t)sizeof(s_device_qualifier);
    return s_device_qualifier;
}

uint8_t USBD_CDC_Dual_TransmitPacket(uint8_t port, const uint8_t *buf, uint16_t len) {
    if (port >= CDC_DUAL_NUM_PORTS || s_pdev == NULL || buf == NULL || len == 0U) {
        return USBD_FAIL;
    }

    if (s_dual_cdc.port[port].TxState != 0U) {
        return USBD_BUSY;
    }

    s_dual_cdc.port[port].TxState = 1U;
    const uint8_t ep = (port == 0U) ? CDC0_IN_EP : CDC1_IN_EP;
    return USBD_LL_Transmit(s_pdev, ep, (uint8_t *)buf, len);
}

uint8_t USBD_CDC_Dual_ReceivePacket(uint8_t port) {
    if (port >= CDC_DUAL_NUM_PORTS || s_pdev == NULL) {
        return USBD_FAIL;
    }
    const uint8_t ep = (port == 0U) ? CDC0_OUT_EP : CDC1_OUT_EP;
    uint8_t *buf = (port == 0U) ? s_rx_buf0 : s_rx_buf1;
    return USBD_LL_PrepareReceive(s_pdev, ep, buf, CDC_DATA_FS_MAX_PACKET_SIZE);
}

uint8_t USBD_CDC_Dual_IsTxBusy(uint8_t port) {
    if (port >= CDC_DUAL_NUM_PORTS) {
        return 1U;
    }
    return (uint8_t)s_dual_cdc.port[port].TxState;
}

