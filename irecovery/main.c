//
//  irecovery.c
//  Minimal irecovery-like tool for S5L8900 devices
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "s5l8900_exploit.h"
#include "idevice.h"

#define DEFAULT_LOAD_ADDR 0x09000000

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Minimal recovery-mode tool for S5L8900 devices\n");
    printf("\n");
    printf("Options:\n");
    printf("  -f <file>        Send file to device\n");
    printf("  -c <command>     Send command to device\n");
    printf("  -a <address>     Load address for file (hex, default: 0x%08X)\n", DEFAULT_LOAD_ADDR);
    printf("  -e               Wrap file in exploit 8900 container before sending\n");
    printf("  -n               Exit recovery mode (normal boot)\n");
    printf("  -h               Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f ramdisk.dmg -a 0x09CC2000          Send file at address\n", prog);
    printf("  %s -f image.img -e                        Exploit-wrap then send\n", prog);
    printf("  %s -c \"setenv boot-args rd=md0\"            Send a single command\n", prog);
    printf("  %s -f ramdisk.dmg -a 0x09CC2000 -c bootx  Send file then command\n", prog);
    printf("  %s -n                                      Exit recovery mode\n", prog);
}

static int load_file(const char *path, uint8_t **out_buf, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[-] Failed to open: %s\n", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *buf = malloc(sz);
    if (!buf) {
        fprintf(stderr, "[-] Failed to allocate %zu bytes\n", sz);
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, sz, fp) != sz) {
        fprintf(stderr, "[-] Failed to read: %s\n", path);
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *out_buf = buf;
    *out_size = sz;
    return 0;
}

static int apply_exploit_wrap(uint8_t *in_buf, size_t in_size, uint8_t **out_buf, size_t *out_size) {
    uint8_t *payload = in_buf;
    size_t payload_size = in_size;

    // Strip existing 8900 header if present
    if (in_size >= 4 && memcmp(in_buf, "8900", 4) == 0) {
        if (in_size <= 0x800) {
            fprintf(stderr, "[-] Input has 8900 header but is too small to strip\n");
            return -1;
        }

        payload = in_buf + 0x800;
        payload_size = in_size - 0x800;
    }

    exploit_image_t container = {0};
    if (exploit_image_create(payload, payload_size, &container) != KERN_SUCCESS) {
        fprintf(stderr, "[-] exploit_image_create failed\n");
        return -1;
    }

    // Copy out so caller owns the buffer
    *out_buf = malloc(container.size);
    if (!*out_buf) {
        exploit_image_free(&container);
        return -1;
    }

    memcpy(*out_buf, container.buf, container.size);
    *out_size = container.size;
    exploit_image_free(&container);

    return 0;
}

static int connect_device(idevice_t *dev) {
    printf("[*] Waiting for recovery-mode device...\n");
    for (int attempt = 0; attempt < 30; attempt++) {
        if (idevice_open(dev) == KERN_SUCCESS) {
            printf("[+] Device connected in %s mode\n", device_mode_string(dev->mode));
            if (idevice_init_handshake(dev) != KERN_SUCCESS) {
                fprintf(stderr, "[-] Handshake failed\n");
                idevice_close(dev);
                return -1;
            }
            return 0;
        }
    
        idevice_close(dev);
        sleep(1);
    }

    fprintf(stderr, "[-] Timed out waiting for device\n");
    return -1;
}

static int send_file(idevice_t *dev, const char *path, uint32_t addr, bool exploit) {
    uint8_t *buf = NULL;
    size_t size = 0;

    if (load_file(path, &buf, &size) != 0) {
        return -1;
    }

    printf("[*] Loaded %s (%zu bytes)\n", path, size);

    uint8_t *send_buf = buf;
    size_t send_size = size;
    uint8_t *exploit_buf = NULL;

    if (exploit) {
        printf("[*] Wrapping in exploit 8900 container...\n");
        if (apply_exploit_wrap(buf, size, &exploit_buf, &send_size) != 0) {
            free(buf);
            return -1;
        }

        send_buf = exploit_buf;
    }

    printf("[*] Sending to 0x%08X (%zu bytes)...\n", addr, send_size);
    kern_return_t kr = idevice_send_file(dev, send_buf, send_size, addr);

    free(buf);
    if (exploit_buf) {
        free(exploit_buf);
    }

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[-] Failed to send file\n");
        return -1;
    }

    printf("[+] File sent successfully\n");
    return 0;
}

static int send_command(idevice_t *dev, const char *cmd) {
    // Append newline if not present
    size_t len = strlen(cmd);
    char *cmd_nl = malloc(len + 2);
    if (!cmd_nl) return -1;

    memcpy(cmd_nl, cmd, len);
    if (len == 0 || cmd[len - 1] != '\n') {
        cmd_nl[len] = '\n';
        cmd_nl[len + 1] = '\0';
    } else {
        cmd_nl[len] = '\0';
    }

    printf("[*] Sending: %s", cmd_nl);
    kern_return_t kr = idevice_send_command(dev, cmd_nl);
    free(cmd_nl);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[-] Command failed\n");
        return -1;
    }

    return 0;
}

static int exit_recovery(idevice_t *dev) {
    printf("[*] Exiting recovery mode...\n");

    if (send_command(dev, "setenv auto-boot true") != 0) {
        return -1;
    }

    if (send_command(dev, "setenv boot-args \"\"") != 0) {
        return -1;
    }

    if (send_command(dev, "saveenv") != 0) {
        return -1;
    }

    send_command(dev, "fsboot");

    printf("[+] Device should be booting normally\n");
    return 0;
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_path = NULL;
    const char *command = NULL;
    uint32_t load_addr = DEFAULT_LOAD_ADDR;
    bool exploit = false;
    bool do_exit_recovery = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            file_path = argv[++i];
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            command = argv[++i];
        }
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            load_addr = (uint32_t)strtoul(argv[++i], NULL, 0);
        }
        else if (strcmp(argv[i], "-e") == 0) {
            exploit = true;
        }
        else if (strcmp(argv[i], "-n") == 0) {
            do_exit_recovery = true;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!file_path && !command && !do_exit_recovery) {
        fprintf(stderr, "[-] Nothing to do. Specify -f, -c, or -n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    idevice_t dev = {0};
    int ret = EXIT_FAILURE;

    if (connect_device(&dev) != 0) {
        return EXIT_FAILURE;
    }

    // Exit recovery takes priority — do it and bail
    if (do_exit_recovery) {
        ret = (exit_recovery(&dev) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
        goto cleanup;
    }

    // Send file if requested
    if (file_path) {
        if (send_file(&dev, file_path, load_addr, exploit) != 0) {
            goto cleanup;
        }
    }

    // Send command if requested
    if (command) {
        if (send_command(&dev, command) != 0) {
            goto cleanup;
        }
    }

    ret = EXIT_SUCCESS;

cleanup:
    idevice_close(&dev);
    return ret;
}