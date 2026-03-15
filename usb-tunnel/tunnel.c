#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../src/common/mux.h"

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [local_port] [device_port]\n", argv[0]);
        printf("  local_port   port to listen on locally     (default: 2222)\n");
        printf("  device_port  port to connect to on device  (default: 22)\n");
        
        return 0;
    }

    int local_port = argc > 1 ? atoi(argv[1]) : 2222;
    int device_port = argc > 2 ? atoi(argv[2]) : 22;

    srand((unsigned)time(NULL));

    libusb_context *usb_ctx = NULL;
    if (libusb_init(&usb_ctx) < 0) {
        fprintf(stderr, "libusb_init failed\n");
        return 1;
    }

    libusb_device_handle *usb_handle;
    uint8_t ep_out;
    uint8_t ep_in;
    int intf_num;
    if (find_and_claim(usb_ctx, &usb_handle, &ep_out, &ep_in, &intf_num) < 0) {
        fprintf(stderr, "No suitable device found.\n");
        return 1;
    }

    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_in srv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(local_port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    if (bind(srv_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        fprintf(stderr, "Failed to bind to port %d: %s\n", local_port, strerror(errno));
        return 1;
    }

    listen(srv_fd, 1);
    printf("Listening on localhost:%d\n", local_port);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(srv_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) {
            fprintf(stderr, "accept failed: %s\n", strerror(errno));
            break;
        }
        printf("Connection from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        usb_pipe_t pipe = {
            .handle = usb_handle,
            .ep_out = ep_out,
            .ep_in = ep_in,
            .rx_len = 0,
        };
        mux_session_t session = {
            .pipe = &pipe,
            .device_port = (uint16_t)device_port,
            .src_port = (uint16_t)(49152 + rand() % 16384),
            .tx_seq = 100,
            .rx_ack = 0,
            .prebuf_len = 0,
        };

        if (session_connect(&session) == 0) {
            forward_loop(cli_fd, &session);
        }
        else {
            fprintf(stderr, "session_connect failed\n");
        }

        session_close(&session);
        close(cli_fd);
        printf("Connection closed.\n\n");
    }

    libusb_release_interface(usb_handle, intf_num);
    libusb_close(usb_handle);
    libusb_exit(usb_ctx);
    close(srv_fd);

    return 0;
}
