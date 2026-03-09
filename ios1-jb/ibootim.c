//
//  ibootim.c
//  ios1-jb
//
//  Created by Ethan Arbuckle on 3/6/26.
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach.h>
#include <png.h>
#include "ibootim.h"
#include "crt.h"
#include "file_payload.h"

#define APPLE8900_HEADER_SIZE 0x800

#define N 4096
#define F 18
#define THRESHOLD 2
#define NIL N

struct encode_state {
    int lchild[N + 1];
    int rchild[N + 257];
    int parent[N + 1];
    uint8_t text_buf[N + F - 1];
    int match_position;
    int match_length;
};

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} png_mem_read_state_t;

static void init_state(struct encode_state *sp) {
    memset(sp, 0, sizeof(*sp));
    for (int i = 0; i < N - F; i++) {
        sp->text_buf[i] = ' ';
    }
    
    for (int i = N + 1; i <= N + 256; i++) {
        sp->rchild[i] = NIL;
    }
    
    for (int i = 0; i < N; i++) {
        sp->parent[i] = NIL;
    }
}

static void insert_node(struct encode_state *sp, int r) {
    int i, p, cmp;
    uint8_t *key;

    cmp = 1;
    key = &sp->text_buf[r];
    p = N + 1 + key[0];
    sp->rchild[r] = sp->lchild[r] = NIL;
    sp->match_length = 0;
    for (;;) {
        if (cmp >= 0) {
            if (sp->rchild[p] != NIL) {
                p = sp->rchild[p];
            }
            else {
                sp->rchild[p] = r;
                sp->parent[r] = p;
                return;
            }
        }
        else {
            if (sp->lchild[p] != NIL) {
                p = sp->lchild[p];
            }
            else {
                sp->lchild[p] = r;
                sp->parent[r] = p;
                return;
            }
        }
        
        for (i = 1; i < F; i++) {
            if ((cmp = key[i] - sp->text_buf[p + i]) != 0) {
                break;
            }
        }
        
        if (i > sp->match_length) {
            sp->match_position = p;
            if ((sp->match_length = i) >= F) {
                break;
            }
        }
    }
    
    sp->parent[r] = sp->parent[p];
    sp->lchild[r] = sp->lchild[p];
    sp->rchild[r] = sp->rchild[p];
    sp->parent[sp->lchild[p]] = r;
    sp->parent[sp->rchild[p]] = r;
    
    if (sp->rchild[sp->parent[p]] == p) {
        sp->rchild[sp->parent[p]] = r;
    }
    else {
        sp->lchild[sp->parent[p]] = r;
    }

    sp->parent[p] = NIL;
}

static void delete_node(struct encode_state *sp, int p) {
    if (sp->parent[p] == NIL) {
        return;
    }
    
    int q = 0;
    if (sp->rchild[p] == NIL) {
        q = sp->lchild[p];
    }
    else if (sp->lchild[p] == NIL) {
        q = sp->rchild[p];
    }
    else {
        q = sp->lchild[p];
        if (sp->rchild[q] != NIL) {
            do {
                q = sp->rchild[q];
            } while (sp->rchild[q] != NIL);
            
            sp->rchild[sp->parent[q]] = sp->lchild[q];
            sp->parent[sp->lchild[q]] = sp->parent[q];
            sp->lchild[q] = sp->lchild[p];
            sp->parent[sp->lchild[p]] = q;
        }
        
        sp->rchild[q] = sp->rchild[p];
        sp->parent[sp->rchild[p]] = q;
    }
    
    sp->parent[q] = sp->parent[p];
    if (sp->rchild[sp->parent[p]] == p) {
        sp->rchild[sp->parent[p]] = q;
    }
    else {
        sp->lchild[sp->parent[p]] = q;
    }

    sp->parent[p] = NIL;
}

static uint8_t *lzss_compress(uint8_t *dst, uint32_t dstlen, uint8_t *src, uint32_t srcLen) {
    int i, c, last_match_length, code_buf_ptr;

    uint8_t *srcend = src + srcLen;
    uint8_t *dstend = dst + dstlen;

    struct encode_state *sp = (struct encode_state *)malloc(sizeof(*sp));
    if (sp == NULL) {
        return NULL;
    }
    init_state(sp);
    
    uint8_t code_buf[17] = {0};
    uint8_t mask;
    
    code_buf_ptr = mask = 1;
    int s = 0;
    int r = N - F;
    int len = 0;
    for (len = 0; len < F && src < srcend; len++) {
        sp->text_buf[r + len] = *src++;
    }
    
    if (!len) {
        free(sp);
        return NULL;
    }

    for (int i = 1; i <= F; i++) {
        insert_node(sp, r - i);
    }
    insert_node(sp, r);

    do {
        if (sp->match_length > len) {
            sp->match_length = len;
        }
        
        if (sp->match_length <= THRESHOLD) {
            sp->match_length = 1;
            code_buf[0] |= mask;
            code_buf[code_buf_ptr++] = sp->text_buf[r];
        }
        else {
            code_buf[code_buf_ptr++] = (uint8_t)sp->match_position;
            code_buf[code_buf_ptr++] = (uint8_t)(((sp->match_position >> 4) & 0xF0) | (sp->match_length - (THRESHOLD + 1)));
        }
        
        if ((mask <<= 1) == 0) {
            for (i = 0; i < code_buf_ptr; i++) {
                if (dst < dstend) {
                    *dst++ = code_buf[i];
                }
                else {
                    free(sp);
                    return NULL;
                }
            }
            
            code_buf[0] = 0;
            code_buf_ptr = mask = 1;
        }
        
        last_match_length = sp->match_length;
        for (i = 0; i < last_match_length && src < srcend; i++) {
            delete_node(sp, s);
            c = *src++;
            sp->text_buf[s] = c;
            
            if (s < F - 1) {
                sp->text_buf[s + N] = c;
            }
            
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            insert_node(sp, r);
        }
        
        while (i++ < last_match_length) {
            delete_node(sp, s);
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            if (--len) {
                insert_node(sp, r);
            }
        }
    } while (len > 0);

    if (code_buf_ptr > 1) {
        for (i = 0; i < code_buf_ptr; i++) {
            if (dst < dstend) {
                *dst++ = code_buf[i];
            } else {
                free(sp);
                return NULL;
            }
        }
    }

    free(sp);
    return dst;
}

static void png_read_from_file(png_structp png_ptr, png_bytep data, png_size_t length) {
    FILE *f = (FILE *)png_get_io_ptr(png_ptr);
    fread(data, 1, length, f);
}

static void png_error_fn(png_structp png_ptr, png_const_charp msg) {
    fprintf(stderr, "libpng error: %s\n", msg);
    longjmp(png_jmpbuf(png_ptr), 1);
}

static void png_warn_fn(png_structp png_ptr, png_const_charp msg) {
    fprintf(stderr, "libpng warning: %s\n", msg);
}

static void png_read_from_mem(png_structp png_ptr, png_bytep out, png_size_t count) {
    png_mem_read_state_t *state = (png_mem_read_state_t *)png_get_io_ptr(png_ptr);
    if (state->offset + count > state->size) {
        png_error(png_ptr, "Read past end of buffer");
        return;
    }

    memcpy(out, state->data + state->offset, count);
    state->offset += count;
}

kern_return_t ibootim_png_to_raw(const payload_t *png_payload, const payload_t *template_payload, uint8_t **out_buf, size_t *out_size) {
    if (template_payload->size < (APPLE8900_HEADER_SIZE + sizeof(img2_inner_header_t) + sizeof(ibootim_header_t))) {
        fprintf(stderr, "Template file too small\n");
        return KERN_FAILURE;
    }

    img2_inner_header_t *img2_hdr = (img2_inner_header_t *)(template_payload->data + APPLE8900_HEADER_SIZE);
    if (img2_hdr->signature != IMG2_SIGNATURE) {
        fprintf(stderr, "No Img2 signature at offset 0x800\n");
        return KERN_FAILURE;
    }

    ibootim_header_t *ibootim_hdr = (ibootim_header_t *)(template_payload->data + APPLE8900_HEADER_SIZE + sizeof(img2_inner_header_t));
    if (memcmp(ibootim_hdr->signature, IBOOTIM_SIGNATURE, 7) != 0) {
        fprintf(stderr, "No iBootIm signature at offset 0xC00\n");
        return KERN_FAILURE;
    }

    img2_inner_header_t saved_img2_hdr;
    memcpy(&saved_img2_hdr, img2_hdr, sizeof(img2_inner_header_t));

    ibootim_header_t saved_ibootim_hdr;
    memcpy(&saved_ibootim_hdr, ibootim_hdr, sizeof(ibootim_header_t));

    if (png_payload->size < 8 || png_sig_cmp(png_payload->data, 0, 8) != 0) {
        fprintf(stderr, "Not a valid PNG file\n");
        return KERN_FAILURE;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_fn, png_warn_fn);
    if (png_ptr == NULL) {
        return KERN_NO_SPACE;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return KERN_NO_SPACE;
    }

    png_infop end_info = png_create_info_struct(png_ptr);
    if (end_info == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return KERN_NO_SPACE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_FAILURE;
    }

    png_mem_read_state_t read_state = { .data = png_payload->data, .size = png_payload->size, .offset = 0 };
    png_set_read_fn(png_ptr, &read_state, png_read_from_mem);
    png_read_info(png_ptr, info_ptr);
    png_set_expand(png_ptr);
    png_set_strip_16(png_ptr);
    png_set_bgr(png_ptr);
    png_set_add_alpha(png_ptr, 0x0, PNG_FILLER_AFTER);
    png_set_invert_alpha(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    if (width > 320 || height > 480) {
        fprintf(stderr, "PNG dimensions %ux%u exceed 320x480\n", width, height);
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_FAILURE;
    }

    if (png_get_bit_depth(png_ptr, info_ptr) != 8) {
        fprintf(stderr, "Bit depth must be 8, got %d\n", png_get_bit_depth(png_ptr, info_ptr));
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_FAILURE;
    }

    int color_type = png_get_color_type(png_ptr, info_ptr);
    uint32_t ibootim_format;
    if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        ibootim_format = IBOOTIM_GREY;
    }
    else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        ibootim_format = IBOOTIM_ARGB;
    }
    else {
        fprintf(stderr, "Unexpected color type after transforms: %d\n", color_type);
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_FAILURE;
    }

    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    size_t pixel_data_size = height * rowbytes;
    uint8_t *pixel_buf = malloc(pixel_data_size);
    if (pixel_buf == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_NO_SPACE;
    }

    png_bytepp row_pointers = malloc(sizeof(png_bytep) * height);
    if (row_pointers == NULL) {
        free(pixel_buf);
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        return KERN_NO_SPACE;
    }

    for (png_uint_32 i = 0; i < height; i++) {
        row_pointers[i] = pixel_buf + (rowbytes * i);
    }

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, end_info);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(row_pointers);

    size_t compress_buf_size = pixel_data_size * 2;
    uint8_t *compressed = malloc(compress_buf_size);
    if (compressed == NULL) {
        free(pixel_buf);
        return KERN_NO_SPACE;
    }

    uint8_t *compress_end = lzss_compress(compressed, (uint32_t)compress_buf_size, pixel_buf, (uint32_t)pixel_data_size);
    if (compress_end == NULL) {
        free(pixel_buf);
        free(compressed);
        return KERN_FAILURE;
    }
    size_t compressed_size = (size_t)(compress_end - compressed);
    free(pixel_buf);

    ibootim_header_t new_ibootim_hdr;
    memcpy(&new_ibootim_hdr, &saved_ibootim_hdr, sizeof(ibootim_header_t));
    new_ibootim_hdr.format = ibootim_format;
    new_ibootim_hdr.width = (uint16_t)width;
    new_ibootim_hdr.height = (uint16_t)height;

    uint32_t payload_len = (uint32_t)(sizeof(ibootim_header_t) + compressed_size);

    size_t total_size = sizeof(img2_inner_header_t) + sizeof(ibootim_header_t) + compressed_size;
    uint8_t *result = calloc(1, total_size);
    if (result == NULL) {
        free(compressed);
        return KERN_NO_SPACE;
    }

    img2_inner_header_t *out_img2 = (img2_inner_header_t *)result;
    memcpy(out_img2, &saved_img2_hdr, sizeof(img2_inner_header_t));
    out_img2->dataLen = payload_len;
    out_img2->dataLenPadded = payload_len;
    out_img2->header_checksum = 0;

    uint32_t cksum = compute_crc(result, 0x64) ^ 0xFFFFFFFF;
    out_img2->header_checksum = cksum;

    memcpy(result + sizeof(img2_inner_header_t), &new_ibootim_hdr, sizeof(ibootim_header_t));
    memcpy(result + sizeof(img2_inner_header_t) + sizeof(ibootim_header_t), compressed, compressed_size);
    free(compressed);

    *out_buf = result;
    *out_size = total_size;

    return KERN_SUCCESS;
}
