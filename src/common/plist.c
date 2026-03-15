#include "plist.h"
#include <stdlib.h>
#include <string.h>

static CFStringRef cfstr_from_cstr(const char *s) {
    if (!s) {
        return NULL;
    }

    return CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);
}

static int copy_cfdata_bytes(CFDataRef data, uint8_t **out, size_t *out_len) {
    CFIndex len;
    uint8_t *buf;

    if (!data || !out || !out_len) {
        return 0;
    }

    len = CFDataGetLength(data);
    buf = malloc((size_t)len + 1);
    if (!buf) {
        return 0;
    }

    if (len > 0) {
        memcpy(buf, CFDataGetBytePtr(data), (size_t)len);
    }

    buf[len] = '\0';

    *out = buf;
    *out_len = (size_t)len;
    return 1;
}

CFDictionaryRef plist_parse_dict(const uint8_t *buf, size_t len) {
    CFDataRef data;
    CFPropertyListRef plist;
    CFDictionaryRef dict;
    CFPropertyListFormat format;

    if (!buf || len == 0) {
        return NULL;
    }

    data = CFDataCreate(kCFAllocatorDefault, buf, (CFIndex)len);
    if (!data) {
        return NULL;
    }

    format = 0;
    plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        NULL
    );
    CFRelease(data);

    if (!plist) {
        return NULL;
    }

    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return NULL;
    }

    dict = (CFDictionaryRef)plist;
    return dict;
}

CFDictionaryRef plist_parse_dict_cstr(const char *plist_str) {
    if (!plist_str) {
        return NULL;
    }

    return plist_parse_dict((const uint8_t *)plist_str, strlen(plist_str));
}

CFTypeRef plist_dict_get_value(CFDictionaryRef dict, const char *key) {
    CFStringRef cfkey;
    CFTypeRef value;

    if (!dict || !key) {
        return NULL;
    }

    cfkey = cfstr_from_cstr(key);
    if (!cfkey) {
        return NULL;
    }

    value = CFDictionaryGetValue(dict, cfkey);
    CFRelease(cfkey);

    return value;
}

int plist_dict_get_string(CFDictionaryRef dict, const char *key, char *out, size_t outsz) {
    CFTypeRef value;

    if (!dict || !key || !out || outsz == 0) {
        return 0;
    }

    out[0] = '\0';

    value = plist_dict_get_value(dict, key);
    if (!value) {
        return 0;
    }

    if (CFGetTypeID(value) != CFStringGetTypeID()) {
        return 0;
    }

    return CFStringGetCString((CFStringRef)value, out, (CFIndex)outsz, kCFStringEncodingUTF8);
}

char *plist_dict_copy_string(CFDictionaryRef dict, const char *key) {
    CFTypeRef value;
    CFIndex max_len;
    char *buf;

    if (!dict || !key) {
        return NULL;
    }

    value = plist_dict_get_value(dict, key);
    if (!value) {
        return NULL;
    }

    if (CFGetTypeID(value) != CFStringGetTypeID()) {
        return NULL;
    }

    max_len = CFStringGetMaximumSizeForEncoding(
        CFStringGetLength((CFStringRef)value),
        kCFStringEncodingUTF8
    ) + 1;

    buf = malloc((size_t)max_len);
    if (!buf) {
        return NULL;
    }

    if (!CFStringGetCString((CFStringRef)value, buf, max_len, kCFStringEncodingUTF8)) {
        free(buf);
        return NULL;
    }

    return buf;
}

int plist_dict_get_bool(CFDictionaryRef dict, const char *key, int *out_value) {
    CFTypeRef value;

    if (!dict || !key || !out_value) {
        return 0;
    }

    value = plist_dict_get_value(dict, key);
    if (!value) {
        return 0;
    }

    if (CFGetTypeID(value) != CFBooleanGetTypeID()) {
        return 0;
    }

    *out_value = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
    return 1;
}

int plist_dict_copy_data(CFDictionaryRef dict, const char *key, uint8_t **out, size_t *out_len) {
    CFTypeRef value;

    if (!dict || !key || !out || !out_len) {
        return 0;
    }

    *out = NULL;
    *out_len = 0;

    value = plist_dict_get_value(dict, key);
    if (!value) {
        return 0;
    }

    if (CFGetTypeID(value) != CFDataGetTypeID()) {
        return 0;
    }

    return copy_cfdata_bytes((CFDataRef)value, out, out_len);
}

CFDictionaryRef plist_dict_get_dict(CFDictionaryRef dict, const char *key) {
    CFTypeRef value;

    if (!dict || !key) {
        return NULL;
    }

    value = plist_dict_get_value(dict, key);
    if (!value) {
        return NULL;
    }

    if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
        return NULL;
    }

    return (CFDictionaryRef)value;
}

CFMutableDictionaryRef plist_dict_create(void) {
    return CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

int plist_dict_set_value(CFMutableDictionaryRef dict, const char *key, CFTypeRef value) {
    CFStringRef cfkey;

    if (!dict || !key || !value) {
        return 0;
    }

    cfkey = cfstr_from_cstr(key);
    if (!cfkey) {
        return 0;
    }

    CFDictionarySetValue(dict, cfkey, value);
    CFRelease(cfkey);
    return 1;
}

int plist_dict_set_string(CFMutableDictionaryRef dict, const char *key, const char *value) {
    CFStringRef cfvalue;
    int ok;

    if (!dict || !key || !value) {
        return 0;
    }

    cfvalue = cfstr_from_cstr(value);
    if (!cfvalue) {
        return 0;
    }

    ok = plist_dict_set_value(dict, key, cfvalue);
    CFRelease(cfvalue);
    return ok;
}

int plist_dict_set_bool(CFMutableDictionaryRef dict, const char *key, int value) {
    return plist_dict_set_value(dict, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

int plist_dict_set_data(CFMutableDictionaryRef dict, const char *key, const void *data, size_t len) {
    CFDataRef cfdata;
    int ok;

    if (!dict || !key || (!data && len != 0)) {
        return 0;
    }

    cfdata = CFDataCreate(kCFAllocatorDefault, data, (CFIndex)len);
    if (!cfdata) {
        return 0;
    }

    ok = plist_dict_set_value(dict, key, cfdata);
    CFRelease(cfdata);
    return ok;
}

int plist_dict_set_dict(CFMutableDictionaryRef dict, const char *key, CFDictionaryRef value) {
    if (!dict || !key || !value) {
        return 0;
    }

    return plist_dict_set_value(dict, key, value);
}

int plist_serialize_xml(CFPropertyListRef plist, uint8_t **out_buf, size_t *out_len) {
    CFDataRef data;
    uint8_t *buf;
    CFIndex len;

    if (!plist || !out_buf || !out_len) {
        return 0;
    }

    *out_buf = NULL;
    *out_len = 0;

    data = CFPropertyListCreateData(kCFAllocatorDefault, plist, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    if (!data) {
        return 0;
    }

    len = CFDataGetLength(data);
    buf = malloc((size_t)len + 1);
    if (!buf) {
        CFRelease(data);
        return 0;
    }

    if (len > 0) {
        memcpy(buf, CFDataGetBytePtr(data), (size_t)len);
    }

    buf[len] = '\0';

    *out_buf = buf;
    *out_len = (size_t)len;

    CFRelease(data);
    return 1;
}

char *plist_serialize_xml_cstr(CFPropertyListRef plist, size_t *out_len) {
    uint8_t *buf;
    size_t len;

    buf = NULL;
    len = 0;

    if (!plist_serialize_xml(plist, &buf, &len)) {
        return NULL;
    }

    if (out_len) {
        *out_len = len;
    }

    return (char *)buf;
}
