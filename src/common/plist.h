#ifndef PLIST_UTIL_H
#define PLIST_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>


CFDictionaryRef plist_parse_dict(const uint8_t *buf, size_t len);
CFDictionaryRef plist_parse_dict_cstr(const char *plist_str);

CFTypeRef plist_dict_get_value(CFDictionaryRef dict, const char *key);
int plist_dict_get_string(CFDictionaryRef dict, const char *key, char *out, size_t outsz);
char *plist_dict_copy_string(CFDictionaryRef dict, const char *key);
int plist_dict_get_bool(CFDictionaryRef dict, const char *key, int *out_value);
int plist_dict_copy_data(CFDictionaryRef dict, const char *key, uint8_t **out, size_t *out_len);
CFDictionaryRef plist_dict_get_dict(CFDictionaryRef dict, const char *key);

CFMutableDictionaryRef plist_dict_create(void);
int plist_dict_set_value(CFMutableDictionaryRef dict, const char *key, CFTypeRef value);
int plist_dict_set_string(CFMutableDictionaryRef dict, const char *key, const char *value);
int plist_dict_set_bool(CFMutableDictionaryRef dict, const char *key, int value);
int plist_dict_set_data(CFMutableDictionaryRef dict, const char *key, const void *data, size_t len);
int plist_dict_set_dict(CFMutableDictionaryRef dict, const char *key, CFDictionaryRef value);

int plist_serialize_xml(CFPropertyListRef plist, uint8_t **out_buf, size_t *out_len);
char *plist_serialize_xml_cstr(CFPropertyListRef plist, size_t *out_len);

#endif
