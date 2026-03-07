//
//  file_payload.c
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#include "file_payload.h"
#include <stdlib.h>
#include <stdio.h>


kern_return_t load_file(const char *path, payload_t *payload) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return KERN_FAILURE;
    }

    fseek(f, 0, SEEK_END);
    payload->size = ftell(f);
    fseek(f, 0, SEEK_SET);

    payload->data = malloc(payload->size);
    if (payload->data == NULL) {
        fclose(f);
        return KERN_NO_SPACE;
    }

    size_t read_size = fread(payload->data, 1, payload->size, f);
    fclose(f);

    if (read_size != payload->size) {
        free(payload->data);
        payload->data = NULL;
        return KERN_FAILURE;
    }

    return KERN_SUCCESS;
}

void payload_free(payload_t *payload) {
    if (payload->data != NULL) {
        free(payload->data);
        payload->data = NULL;
    }

    payload->size = 0;
}

