//
//  file_payload.c
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#include "file_payload.h"
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <stdio.h>
#include <stdlib.h>


kern_return_t load_embedded_file(const char *name, payload_t *payload) {
    unsigned long size = 0;
    const struct mach_header_64 *header = (const struct mach_header_64 *)_dyld_get_image_header(0);
    uint8_t *data = getsectiondata(header, "__DATA", name, &size);
    if (data == NULL) {
        return KERN_FAILURE;
    }

    payload->data = malloc(size);
    if (payload->data == NULL) {
        return KERN_NO_SPACE;
    }

    memcpy(payload->data, data, size);
    payload->size = size;
    return KERN_SUCCESS;
}

void payload_free(payload_t *payload) {
    if (payload->data != NULL) {
        free(payload->data);
        payload->data = NULL;
    }

    payload->size = 0;
}

