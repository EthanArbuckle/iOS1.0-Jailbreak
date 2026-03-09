//
//  idevice.h
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#ifndef IDEVICE_H
#define IDEVICE_H

#include <libusb-1.0/libusb.h>
#include <mach/mach.h>

#define APPLE_VID           0x05AC
#define RECOVERY_PID        0x1280
#define NORMAL_PID          0x1290

#define USB_TIMEOUT         5000 * 2
#define EP_CTRL_OUT         0x04
#define EP_CTRL_IN          0x83
#define EP_CMD_OUT          0x02
#define EP_FILE_OUT         0x05

#define CMD_INIT            0x0000
#define CMD_SEND_COMMAND    0x0803
#define CMD_SEND_FILE       0x0805
#define CMD_ACK             0x0808

#pragma pack(push, 1)
typedef struct {
    int16_t cmdcode;
    int16_t constant;
    int32_t size;
    int32_t unknown;
} iboot_msg_t;
#pragma pack(pop)

typedef enum {
    DEVICE_NORMAL,
    DEVICE_RECOVERY,
    DEVICE_NOT_FOUND
} device_mode_t;

typedef struct {
    libusb_context *ctx;
    libusb_device_handle *handle;
    device_mode_t mode;
} idevice_t;

const char *device_mode_string(device_mode_t mode);

kern_return_t idevice_open(idevice_t *dev);
void idevice_close(idevice_t *dev);

kern_return_t idevice_init_handshake(idevice_t *dev);
kern_return_t idevice_send_command(idevice_t *dev, const char *cmd);
kern_return_t idevice_send_file(idevice_t *dev, void *data, size_t size, uint32_t load_addr);

#endif
