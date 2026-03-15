#ifndef LOCKDOWND_H
#define LOCKDOWND_H

#include <CoreFoundation/CoreFoundation.h>
#include "mux.h"

#define LOCKDOWND_PORT 62078
#define LOCKDOWND_LABEL "lockdownd-client"
#define LOCKDOWND_RESP_MAX 65536

typedef struct {
    libusb_context *usb_ctx;
    libusb_device_handle *usb_handle;
    uint8_t ep_out;
    uint8_t ep_in;
    int intf_num;
    usb_pipe_t pipe;
    mux_session_t session;
} lockdownd_client_t;

int lockdownd_client_start_paired_session(lockdownd_client_t *client, char *session_id, size_t session_id_sz, char *error_out, size_t error_out_sz);
void lockdownd_client_cleanup(lockdownd_client_t *client);
int lockdownd_client_open(lockdownd_client_t *client);

int lockdownd_get_value_string(mux_session_t *session, const char *key, char *out, size_t outsz);
int lockdownd_get_value_data(mux_session_t *session, const char *key, uint8_t **out_data, size_t *out_len);

int lockdownd_send_enter_recovery(mux_session_t *session, const char *session_id, CFDictionaryRef *response);

#endif