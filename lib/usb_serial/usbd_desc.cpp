/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * USB Device Descriptor
 */


#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

// USB Device Descriptor Data
#define VID                   1155
#define PID                   22336
#define LANGID_STRING         1033
#define MANUFACTURER_STRING   "STMicroelectronics"
#define PRODUCT_STRING        "SX127x Probe Serial"
#define CONFIGURATION_STRING  "CDC Config"
#define INTERFACE_STRING      "CDC Interface"

#define  SERIAL_STRING_LEN    (2*13)

  
static void FormatSerial();
static void FormatHex(uint32_t value, uint8_t* pbuf, uint8_t len);

static uint8_t* GetDeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetLangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetSerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* GetInterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);


USBD_DescriptorsTypeDef usbSerialDeviceDescriptor = {
    GetDeviceDescriptor,
    GetLangIDStrDescriptor,
    GetManufacturerStrDescriptor,
    GetProductStrDescriptor,
    GetSerialStrDescriptor,
    GetConfigStrDescriptor,
    GetInterfaceStrDescriptor,
};


// USB standard device descriptor
#if defined ( __ICCARM__ )
    #pragma data_alignment=4
#endif
static __ALIGN_BEGIN uint8_t deviceDesc[USB_LEN_DEV_DESC] __ALIGN_END =
{
    0x12,                       // bLength
    USB_DESC_TYPE_DEVICE,       // bDescriptorType
    0x00,                       // bcdUSB
    0x02,
    0x02,                       // bDeviceClass
    0x02,                       // bDeviceSubClass
    0x00,                       // bDeviceProtocol
    USB_MAX_EP0_SIZE,           // bMaxPacketSize
    LOBYTE(VID),                // idVendor
    HIBYTE(VID),                // idVendor
    LOBYTE(PID),                // idProduct
    HIBYTE(PID),                // idProduct
    0x00,                       // bcdDevice rel. 2.00
    0x02,
    USBD_IDX_MFC_STR,           // Index of manufacturer string
    USBD_IDX_PRODUCT_STR,       // Index of product string
    USBD_IDX_SERIAL_STR,        // Index of serial number string
    USBD_MAX_NUM_CONFIGURATION  // bNumConfigurations
};


// USB lang indentifier descriptor
#if defined ( __ICCARM__ )
    #pragma data_alignment=4
#endif
static __ALIGN_BEGIN uint8_t langIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(LANGID_STRING),
    HIBYTE(LANGID_STRING)
};


// String buffer
#if defined ( __ICCARM__ )
    #pragma data_alignment=4
#endif
static __ALIGN_BEGIN uint8_t stringDescBuffer[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;


#if defined ( __ICCARM__ )
    #pragma data_alignment=4
#endif
static __ALIGN_BEGIN uint8_t serialString[SERIAL_STRING_LEN] __ALIGN_END = {
    SERIAL_STRING_LEN,
    USB_DESC_TYPE_STRING,
};


uint8_t* GetDeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    *length = sizeof(deviceDesc);
    return deviceDesc;
}


uint8_t* GetLangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    *length = sizeof(langIDDesc);
    return langIDDesc;
}


uint8_t* GetProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t*)PRODUCT_STRING, stringDescBuffer, length);
    return stringDescBuffer;
}

uint8_t* GetManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)MANUFACTURER_STRING, stringDescBuffer, length);
    return stringDescBuffer;
}

uint8_t* GetSerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    *length = SERIAL_STRING_LEN;
    FormatSerial();
    return (uint8_t *) serialString;
}

uint8_t* GetConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)CONFIGURATION_STRING, stringDescBuffer, length);
    return stringDescBuffer;
}

uint8_t* GetInterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)INTERFACE_STRING, stringDescBuffer, length);
    return stringDescBuffer;
}


void FormatSerial()
{
    uint32_t part0 = *(uint32_t*) (UID_BASE + 0);
    uint32_t part1 = *(uint32_t*) (UID_BASE + 4);
    uint32_t part2 = *(uint32_t*) (UID_BASE + 8);

    part0 += part2;

    FormatHex(part0, serialString + 2, 8);
    FormatHex(part1, serialString + 18, 4);
}

static void FormatHex(uint32_t value, uint8_t* buf, uint8_t len)
{
    uint16_t* p = (uint16_t*)buf;

    for (int idx = 0; idx < len; idx++) {
        uint16_t digit = value >> 28;
        *p++ = digit < 10 ? digit + '0' : digit + 'A' - 10;
        value = value << 4;
    }
}
