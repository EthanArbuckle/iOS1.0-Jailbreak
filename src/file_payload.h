//
//  file_payload.h
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#ifndef FILE_PAYLOAD_H
#define FILE_PAYLOAD_H

#include <mach/mach.h>

typedef struct {
    void *data;
    size_t size;
} payload_t;


kern_return_t load_embedded_file(const char *name, payload_t *payload);
kern_return_t load_file_from_path(const char *path, payload_t *payload);
void payload_free(payload_t *payload);

#endif
