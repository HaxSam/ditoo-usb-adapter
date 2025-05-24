#include "usb_descriptors.h"

#include <bsp/board_api.h>
#include <tusb.h>

#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                 _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))

#define USB_VID 0xCafe
#define USB_BCD 0x0201

// defines a descriptor that will be communicated to the host
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,

    .bDeviceClass = TUSB_CLASS_MISC,          // CDC is a subclass of misc
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,  // CDC uses common subclass
    .bDeviceProtocol = MISC_PROTOCOL_IAD,     // CDC uses IAD

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,  // 64 bytes

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,  // Device release number

    .iManufacturer = 0x01,  // Index of manufacturer string
    .iProduct = 0x02,       // Index of product string
    .iSerialNumber = 0x03,  // Index of serial number string

    .bNumConfigurations = 0x01  // 1 configuration
};

// called when host requests to get device descriptor
uint8_t const *tud_descriptor_device_cb(void);

enum {
    ITF_NUM_CDC_0 = 0,
    ITF_NUM_CDC_0_DATA,
    ITF_NUM_VENDOR,
    ITF_NUM_TOTAL
};

// total length of configuration descriptor
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN + TUD_VENDOR_DESC_LEN)

// define endpoint numbers
#define EPNUM_CDC_0_NOTIF 0x81  // notification endpoint for CDC 0
#define EPNUM_CDC_0_OUT 0x02    // out endpoint for CDC 0
#define EPNUM_CDC_0_IN 0x82     // in endpoint for CDC 0

#define EPNUM_VENDOR_OUT 0x03
#define EPNUM_VENDOR_IN 0x83

// configure descriptor (for 2 CDC interfaces)
uint8_t const desc_configuration[] = {
    // config descriptor | how much power in mA, count of interfaces, ...
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // CDC 0: Communication Interface
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, 0x80 | EPNUM_CDC_0_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),

    // Interface number, string index, EP Out & IN address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 5, EPNUM_VENDOR_OUT, 0x80 | EPNUM_VENDOR_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

#define MS_OS_20_DESC_LEN 0xB2
#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] = {
    // total length, number of device caps
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

    // Vendor Code, iLandingPage
    TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

    // Microsoft OS 2.0 descriptor
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT),
};

uint8_t const *tud_descriptor_bos_cb(void) {
    return desc_bos;
}

uint8_t const desc_ms_os_20[] =
    {
        // Set header: length, type, windows version, total length
        U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

        // Configuration subset header: length, type, configuration index, reserved, configuration total length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

        // Function Subset header: length, type, first interface, reserved, subset length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_VENDOR, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

        // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
        U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // sub-compatible

        // MS OS 2.0 Registry property descriptor: length, type
        U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08 - 0x08 - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
        U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A),  // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
        U16_TO_U8S_LE(0x0050),  // wPropertyDataLength
                                // bPropertyData: {70394F16-EDAF-47D5-92C8-7CB51107A235}.
        '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
        '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
        '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
        '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

// called when host requests to get configuration descriptor
uint8_t const *tud_descriptor_configuration_cb(uint8_t index);

// more device descriptor this time the qualifier
tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,

    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00,
};

// called when host requests to get device qualifier descriptor
uint8_t const *tud_descriptor_device_qualifier_cb(void);

// String descriptors referenced with .i... in the descriptor tables

enum {
    STRID_LANGID = 0,    // 0: supported language ID
    STRID_MANUFACTURER,  // 1: Manufacturer
    STRID_PRODUCT,       // 2: Product
    STRID_SERIAL,        // 3: Serials
    STRID_CDC_0,         // 4: CDC Interface 0
    STRID_VENDOR,
};

// array of pointer to string descriptors
char const *string_desc_arr[] = {
    // switched because board is little endian
    (const char[]){0x09, 0x04},  // 0: supported language is English (0x0409)
    "HaxSam",                    // 1: Manufacturer
    "Ditoo USB Adapter",         // 2: Product
    NULL,                        // 3: Serials (null so it uses unique ID if available)
    "Ditoo USB CDC Adapter",     // 4: CDC Interface 0
    "Ditoo WebUSB"               // 5: Vendor Interface
};

// buffer to hold the string descriptor during the request | plus 1 for the null terminator
static uint16_t _desc_str[32 + 1];

// called when host request to get string descriptor
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);

// --------------------------------------------------------------------+
// IMPLEMENTATION
// --------------------------------------------------------------------+

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_device_qualifier_cb(void) {
    return (uint8_t const *)&desc_device_qualifier;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    // avoid unused parameter warning and keep function signature consistent
    (void)index;

    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t char_count;

    // Determine which string descriptor to return
    switch (index) {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[STRID_LANGID], 2);
            char_count = 1;
            break;

        case STRID_SERIAL:
            // try to read the serial from the board
            char_count = board_usb_get_serial(_desc_str + 1, 32);
            break;

        default:
            // COPYRIGHT NOTE: Based on TinyUSB example
            // Windows wants utf16le

            // Determine which string descriptor to return
            if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
                return NULL;
            }

            // Copy string descriptor into _desc_str
            const char *str = string_desc_arr[index];

            char_count = strlen(str);
            size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;  // -1 for string type
            // Cap at max char
            if (char_count > max_count) {
                char_count = max_count;
            }

            // Convert ASCII string into UTF-16
            for (size_t i = 0; i < char_count; i++) {
                _desc_str[1 + i] = str[i];
            }
            break;
    }

    // First byte is the length (including header), second byte is string type
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (char_count * 2 + 2));

    return _desc_str;
}