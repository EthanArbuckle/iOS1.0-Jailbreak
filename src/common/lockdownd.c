#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lockdownd.h"
#include "plist.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

typedef struct {
    char host_id[64];
} pairing_record_info_t;

static EVP_PKEY *gen_rsa_key(void) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        return NULL;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

static X509 *make_cert_skeleton(EVP_PKEY *pubkey) {
    X509 *x = X509_new();
    if (!x) {
        return NULL;
    }

    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 0);
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), 10 * 365 * 24 * 3600L);
    X509_set_pubkey(x, pubkey);

    return x;
}

static void add_ski(X509 *x) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x, x, NULL, NULL, 0);

    X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_key_identifier, "hash");
    if (ext) {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }
}

static X509 *make_root_cert(EVP_PKEY *root_key) {
    X509 *x = make_cert_skeleton(root_key);
    if (!x) {
        return NULL;
    }

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x, x, NULL, NULL, 0);

    X509_EXTENSION *bc = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "critical,CA:TRUE");
    if (bc) {
        X509_add_ext(x, bc, -1);
        X509_EXTENSION_free(bc);
    }

    add_ski(x);
    X509_sign(x, root_key, EVP_sha1());

    return x;
}

static X509 *make_leaf_cert(EVP_PKEY *leaf_pubkey, EVP_PKEY *ca_key, X509 *ca_cert) {
    X509 *x = make_cert_skeleton(leaf_pubkey);
    if (!x) {
        return NULL;
    }

    X509_set_issuer_name(x, X509_get_subject_name(ca_cert));

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, ca_cert, x, NULL, NULL, 0);

    X509_EXTENSION *bc = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "critical,CA:FALSE");
    if (bc) {
        X509_add_ext(x, bc, -1);
        X509_EXTENSION_free(bc);
    }

    X509V3_set_ctx(&ctx, x, x, NULL, NULL, 0);
    add_ski(x);

    X509_EXTENSION *ku = X509V3_EXT_conf_nid(NULL, &ctx, NID_key_usage, "critical,digitalSignature,keyEncipherment");
    if (ku) {
        X509_add_ext(x, ku, -1);
        X509_EXTENSION_free(ku);
    }

    X509_sign(x, ca_key, EVP_sha1());

    return x;
}

static int cert_to_der(X509 *cert, uint8_t **out, size_t *out_len) {
    if (!cert || !out || !out_len) {
        return 0;
    }

    *out = NULL;
    *out_len = 0;

    int len = i2d_X509(cert, NULL);
    if (len <= 0) {
        return 0;
    }

    uint8_t *buf = malloc((size_t)len);
    if (!buf) {
        return 0;
    }

    uint8_t *p = buf;
    i2d_X509(cert, &p);

    *out = buf;
    *out_len = (size_t)len;
    return 1;
}

static int privkey_to_der(EVP_PKEY *pkey, uint8_t **out, size_t *out_len) {
    if (!pkey || !out || !out_len) {
        return 0;
    }

    *out = NULL;
    *out_len = 0;

    int len = i2d_PrivateKey(pkey, NULL);
    if (len <= 0) {
        return 0;
    }

    uint8_t *buf = malloc((size_t)len);
    if (!buf) {
        return 0;
    }

    uint8_t *p = buf;
    i2d_PrivateKey(pkey, &p);

    *out = buf;
    *out_len = (size_t)len;
    return 1;
}

static void gen_uuid(char *out, size_t outsz) {
    uint8_t rnd[16];

    RAND_bytes(rnd, sizeof(rnd));
    rnd[6] = (rnd[6] & 0x0f) | 0x40;
    rnd[8] = (rnd[8] & 0x3f) | 0x80;

    snprintf(out, outsz, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        rnd[0], rnd[1], rnd[2], rnd[3],
        rnd[4], rnd[5],
        rnd[6], rnd[7],
        rnd[8], rnd[9],
        rnd[10], rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]
    );
}

static void pairing_record_path(const char *udid, char *out, size_t outsz) {
    const char *home = getenv("HOME");
    if (!home) {
        home = "/tmp";
    }

    snprintf(out, outsz, "%s/.ios_pairing_records", home);
    mkdir(out, 0700);

    snprintf(out, outsz, "%s/.ios_pairing_records/%s.plist", home, udid);
}

static uint8_t *read_file_bytes(const char *path, size_t *out_len) {
    if (!path || !out_len) {
        return NULL;
    }

    *out_len = 0;

    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (sz > 0) {
        if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
            free(buf);
            fclose(f);
            return NULL;
        }
    }

    buf[sz] = '\0';
    fclose(f);

    *out_len = (size_t)sz;
    return buf;
}

static int write_file_bytes(const char *path, const uint8_t *buf, size_t len) {
    if (!path || !buf) {
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return 0;
    }

    if (len > 0 && fwrite(buf, 1, len, f) != len) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

static int load_pairing_record_info(const char *path, pairing_record_info_t *info) {
    if (!path || !info) {
        return 0;
    }

    memset(info, 0, sizeof(*info));

    size_t len;
    uint8_t *buf = read_file_bytes(path, &len);
    if (!buf) {
        return 0;
    }

    CFDictionaryRef dict = plist_parse_dict(buf, len);
    free(buf);

    if (!dict) {
        return 0;
    }

    if (!plist_dict_get_string(dict, "HostID", info->host_id, sizeof(info->host_id))) {
        CFRelease(dict);
        return 0;
    }

    CFRelease(dict);
    return 1;
}

static int save_pairing_record(const char *path, const char *host_id, const uint8_t *device_cert_der, size_t device_cert_der_len, const uint8_t *host_cert_der, size_t host_cert_der_len, const uint8_t *root_cert_der, size_t root_cert_der_len, const uint8_t *host_key_der, size_t host_key_der_len) {
    if (!path || !host_id || !device_cert_der || !host_cert_der || !root_cert_der || !host_key_der) {
        return 0;
    }

    CFMutableDictionaryRef dict = plist_dict_create();
    if (!dict) {
        return 0;
    }

    plist_dict_set_data(dict, "DeviceCertificate", device_cert_der, device_cert_der_len);
    plist_dict_set_data(dict, "HostCertificate", host_cert_der, host_cert_der_len);
    plist_dict_set_string(dict, "HostID", host_id);
    plist_dict_set_data(dict, "HostPrivateKey", host_key_der, host_key_der_len);
    plist_dict_set_data(dict, "RootCertificate", root_cert_der, root_cert_der_len);

    uint8_t *xml;
    size_t xml_len;
    int ok = plist_serialize_xml(dict, &xml, &xml_len);
    CFRelease(dict);

    if (!ok) {
        return 0;
    }

    ok = write_file_bytes(path, xml, xml_len);
    free(xml);

    return ok;
}

void lockdownd_client_cleanup(lockdownd_client_t *client) {
    if (!client) {
        return;
    }

    if (client->usb_handle) {
        session_close(&client->session);

        libusb_release_interface(client->usb_handle, client->intf_num);
        libusb_close(client->usb_handle);
        client->usb_handle = NULL;
    }

    if (client->usb_ctx) {
        libusb_exit(client->usb_ctx);
        client->usb_ctx = NULL;
    }
}

int lockdownd_client_open(lockdownd_client_t *client) {
    if (!client) {
        return 0;
    }

    memset(client, 0, sizeof(*client));

    if (libusb_init(&client->usb_ctx) < 0) {
        fprintf(stderr, "libusb_init failed\n");
        return 0;
    }

    if (find_and_claim(client->usb_ctx, &client->usb_handle, &client->ep_out, &client->ep_in, &client->intf_num) < 0) {
        lockdownd_client_cleanup(client);
        return 0;
    }

    client->pipe.handle = client->usb_handle;
    client->pipe.ep_out = client->ep_out;
    client->pipe.ep_in = client->ep_in;
    client->pipe.rx_len = 0;

    client->session.pipe = &client->pipe;
    client->session.device_port = LOCKDOWND_PORT;
    client->session.src_port = (uint16_t)(49152 + rand() % 16384);
    client->session.tx_seq = 100;
    client->session.rx_ack = 0;
    client->session.prebuf_len = 0;

    if (session_connect(&client->session) < 0) {
        fprintf(stderr, "session_connect failed\n");
        lockdownd_client_cleanup(client);
        return 0;
    }

    return 1;
}

static int lockdownd_exchange(mux_session_t *session, const uint8_t *req, size_t req_len, uint8_t *resp_out, size_t resp_outsz) {
    if (!session || !req || !resp_out) {
        return -1;
    }

    uint8_t hdr[4];
    w32be(hdr, (uint32_t)req_len);

    if (session_send_frame(session, TCP_ACK, hdr, 4) < 0) {
        return -1;
    }

    if (session_send_frame(session, TCP_ACK, req, (int)req_len) < 0) {
        return -1;
    }

    if (session_recv(session, hdr, 4) < 0) {
        return -1;
    }

    uint32_t rlen = r32be(hdr);
    if (rlen == 0 || rlen >= resp_outsz) {
        fprintf(stderr, "bad response length %u\n", rlen);
        return -1;
    }

    int got = 0;
    while (got < (int)rlen) {
        int n = session_recv(session, resp_out + got, rlen - got);
        if (n < 0) {
            return -1;
        }

        got += n;
    }

    return got;
}

static CFMutableDictionaryRef lockdownd_create_request(const char *request_name) {
    CFMutableDictionaryRef req = plist_dict_create();
    if (!req) {
        return NULL;
    }

    plist_dict_set_string(req, "Label", LOCKDOWND_LABEL);

    if (request_name) {
        plist_dict_set_string(req, "Request", request_name);
    }

    return req;
}

static int lockdownd_exchange_plist(mux_session_t *session, CFDictionaryRef request, CFDictionaryRef *response) {
    if (!session || !request || !response) {
        return 0;
    }

    *response = NULL;

    uint8_t *xml;
    size_t xml_len;
    if (!plist_serialize_xml(request, &xml, &xml_len)) {
        return 0;
    }

    uint8_t resp[LOCKDOWND_RESP_MAX];
    int resp_len = lockdownd_exchange(session, xml, xml_len, resp, sizeof(resp));
    free(xml);

    if (resp_len <= 0) {
        return 0;
    }

    CFDictionaryRef dict = plist_parse_dict(resp, (size_t)resp_len);
    if (!dict) {
        return 0;
    }

    *response = dict;
    return 1;
}

static int lockdownd_get_value_response(mux_session_t *session, const char *key, CFDictionaryRef *response) {
    if (!session || !key || !response) {
        return 0;
    }

    CFMutableDictionaryRef req = lockdownd_create_request("GetValue");
    if (!req) {
        return 0;
    }

    plist_dict_set_string(req, "Key", key);

    int ok = lockdownd_exchange_plist(session, req, response);
    CFRelease(req);

    return ok;
}

int lockdownd_get_value_string(mux_session_t *session, const char *key, char *out, size_t outsz) {
    if (!out || outsz == 0) {
        return 0;
    }

    out[0] = '\0';

    CFDictionaryRef resp;
    if (!lockdownd_get_value_response(session, key, &resp)) {
        return 0;
    }

    int ok = plist_dict_get_string(resp, "Value", out, outsz);
    CFRelease(resp);

    return ok;
}

int lockdownd_get_value_data(mux_session_t *session, const char *key, uint8_t **out_data, size_t *out_len) {
    if (!out_data || !out_len) {
        return 0;
    }

    *out_data = NULL;
    *out_len = 0;

    CFDictionaryRef resp;
    if (!lockdownd_get_value_response(session, key, &resp)) {
        return 0;
    }

    int ok = plist_dict_copy_data(resp, "Value", out_data, out_len);
    CFRelease(resp);

    return ok;
}

static int parse_device_public_key(const uint8_t *pem, size_t pem_len, EVP_PKEY **out_key) {
    if (!pem || !out_key) {
        return 0;
    }

    *out_key = NULL;

    BIO *keybio = BIO_new_mem_buf((void *)pem, (int)pem_len);
    if (!keybio) {
        return 0;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RSA *rsa = PEM_read_bio_RSAPublicKey(keybio, NULL, NULL, NULL);
#pragma GCC diagnostic pop
    BIO_free(keybio);

    if (!rsa) {
        return 0;
    }

    EVP_PKEY *pkey = EVP_PKEY_new();
    if (!pkey) {
        RSA_free(rsa);
        return 0;
    }

    if (EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
        EVP_PKEY_free(pkey);
        RSA_free(rsa);
        return 0;
    }

    *out_key = pkey;
    return 1;
}

static int lockdownd_pair(mux_session_t *session, const char *record_path, char *host_id, size_t host_id_sz, EVP_PKEY **out_root_key, EVP_PKEY **out_host_key,
    EVP_PKEY **out_dev_pubkey, X509 **out_root_cert, X509 **out_host_cert, X509 **out_dev_cert) {

    uint8_t *dev_pubkey_pem = NULL;
    size_t dev_pubkey_pem_len = 0;
    EVP_PKEY *dev_pubkey = NULL;
    EVP_PKEY *root_key = NULL;
    EVP_PKEY *host_key = NULL;
    X509 *root_cert = NULL;
    X509 *host_cert = NULL;
    X509 *dev_cert = NULL;

    uint8_t *root_der = NULL;
    uint8_t *host_der = NULL;
    uint8_t *dev_der = NULL;
    uint8_t *host_key_der = NULL;
    size_t root_der_len = 0;
    size_t host_der_len = 0;
    size_t dev_der_len = 0;
    size_t host_key_der_len = 0;

    CFMutableDictionaryRef req = NULL;
    CFMutableDictionaryRef pair_record = NULL;
    CFDictionaryRef resp = NULL;
    char result[64] = {0};
    int ok = 0;

    if (!session || !record_path || !host_id) {
        return 0;
    }

    if (!lockdownd_get_value_data(session, "DevicePublicKey", &dev_pubkey_pem, &dev_pubkey_pem_len)) {
        fprintf(stderr, "GetValue(DevicePublicKey) failed\n");
        goto out;
    }

    if (!parse_device_public_key(dev_pubkey_pem, dev_pubkey_pem_len, &dev_pubkey)) {
        fprintf(stderr, "Failed to parse device public key\n");
        goto out;
    }

    root_key = gen_rsa_key();
    host_key = gen_rsa_key();
    if (!root_key || !host_key) {
        fprintf(stderr, "Key generation failed\n");
        goto out;
    }

    root_cert = make_root_cert(root_key);
    host_cert = make_leaf_cert(host_key, root_key, root_cert);
    dev_cert = make_leaf_cert(dev_pubkey, root_key, root_cert);
    if (!root_cert || !host_cert || !dev_cert) {
        fprintf(stderr, "Certificate generation failed\n");
        goto out;
    }

    if (!cert_to_der(root_cert, &root_der, &root_der_len)) {
        goto out;
    }

    if (!cert_to_der(host_cert, &host_der, &host_der_len)) {
        goto out;
    }

    if (!cert_to_der(dev_cert, &dev_der, &dev_der_len)) {
        goto out;
    }

    if (!privkey_to_der(host_key, &host_key_der, &host_key_der_len)) {
        goto out;
    }

    gen_uuid(host_id, host_id_sz);

    req = lockdownd_create_request("Pair");
    pair_record = plist_dict_create();
    if (!req || !pair_record) {
        goto out;
    }

    plist_dict_set_data(pair_record, "DeviceCertificate", dev_der, dev_der_len);
    plist_dict_set_data(pair_record, "HostCertificate", host_der, host_der_len);
    plist_dict_set_string(pair_record, "HostID", host_id);
    plist_dict_set_data(pair_record, "RootCertificate", root_der, root_der_len);
    plist_dict_set_dict(req, "PairRecord", pair_record);

    if (!lockdownd_exchange_plist(session, req, &resp)) {
        fprintf(stderr, "Pair request failed\n");
        goto out;
    }

    if (!plist_dict_get_string(resp, "Result", result, sizeof(result))) {
        fprintf(stderr, "Pair response missing Result\n");
        goto out;
    }

    if (strcmp(result, "Success") != 0) {
        char err[128] = {0};

        plist_dict_get_string(resp, "Error", err, sizeof(err));

        if (err[0]) {
            fprintf(stderr, "Pair failed: %s (%s)\n", result, err);
        } else {
            fprintf(stderr, "Pair failed: %s\n", result);
        }

        goto out;
    }

    if (!save_pairing_record(
            record_path,
            host_id,
            dev_der,
            dev_der_len,
            host_der,
            host_der_len,
            root_der,
            root_der_len,
            host_key_der,
            host_key_der_len)) {
        fprintf(stderr, "Failed to save pairing record: %s\n", record_path);
        goto out;
    }

    if (out_root_key) {
        *out_root_key = root_key;
        root_key = NULL;
    }

    if (out_host_key) {
        *out_host_key = host_key;
        host_key = NULL;
    }

    if (out_dev_pubkey) {
        *out_dev_pubkey = dev_pubkey;
        dev_pubkey = NULL;
    }

    if (out_root_cert) {
        *out_root_cert = root_cert;
        root_cert = NULL;
    }

    if (out_host_cert) {
        *out_host_cert = host_cert;
        host_cert = NULL;
    }

    if (out_dev_cert) {
        *out_dev_cert = dev_cert;
        dev_cert = NULL;
    }

    ok = 1;

out:
    if (resp) {
        CFRelease(resp);
    }

    if (pair_record) {
        CFRelease(pair_record);
    }

    if (req) {
        CFRelease(req);
    }

    if (dev_pubkey_pem) {
        free(dev_pubkey_pem);
    }

    if (root_der) {
        free(root_der);
    }

    if (host_der) {
        free(host_der);
    }

    if (dev_der) {
        free(dev_der);
    }

    if (host_key_der) {
        free(host_key_der);
    }

    if (root_cert) {
        X509_free(root_cert);
    }

    if (host_cert) {
        X509_free(host_cert);
    }

    if (dev_cert) {
        X509_free(dev_cert);
    }

    if (root_key) {
        EVP_PKEY_free(root_key);
    }

    if (host_key) {
        EVP_PKEY_free(host_key);
    }

    if (dev_pubkey) {
        EVP_PKEY_free(dev_pubkey);
    }

    return ok;
}

static int lockdownd_start_session(mux_session_t *session, const char *host_id, char *session_id, size_t session_id_sz, char *error_out, size_t error_out_sz) {
    if (!session || !host_id || !session_id || session_id_sz == 0) {
        return 0;
    }

    session_id[0] = '\0';
    if (error_out && error_out_sz > 0) {
        error_out[0] = '\0';
    }

    CFMutableDictionaryRef req = lockdownd_create_request("StartSession");
    if (!req) {
        return 0;
    }

    plist_dict_set_string(req, "HostID", host_id);

    CFDictionaryRef resp = NULL;
    if (!lockdownd_exchange_plist(session, req, &resp)) {
        CFRelease(req);
        return 0;
    }

    CFRelease(req);

    char result[64] = {0};
    plist_dict_get_string(resp, "Result", result, sizeof(result));
    plist_dict_get_string(resp, "SessionID", session_id, session_id_sz);

    int enable_ssl_tmp = 0;
    if (!plist_dict_get_bool(resp, "EnableSessionSSL", &enable_ssl_tmp)) {
        char ssl_flag[16] = {0};

        if (plist_dict_get_string(resp, "EnableSessionSSL", ssl_flag, sizeof(ssl_flag))) {
            if (strcmp(ssl_flag, "true") == 0 || strcmp(ssl_flag, "1") == 0) {
                enable_ssl_tmp = 1;
            }
        }
    }

    if (strcmp(result, "Success") != 0) {
        if (error_out && error_out_sz > 0) {
            plist_dict_get_string(resp, "Error", error_out, error_out_sz);
        }

        CFRelease(resp);
        return 0;
    }

    CFRelease(resp);
    return 1;
}

static int lockdownd_send_request(mux_session_t *session, CFDictionaryRef request, CFDictionaryRef *response) {
    return lockdownd_exchange_plist(session, request, response);
}

int lockdownd_send_enter_recovery(mux_session_t *session, const char *session_id, CFDictionaryRef *response) {
    if (!session || !session_id) {
        return 0;
    }

    CFMutableDictionaryRef req = lockdownd_create_request("EnterRecovery");
    if (!req) {
        return 0;
    }

    plist_dict_set_string(req, "SessionID", session_id);

    int ok = lockdownd_send_request(session, req, response);
    CFRelease(req);

    return ok;
}

int lockdownd_client_start_paired_session(lockdownd_client_t *client, char *session_id, size_t session_id_sz, char *error_out, size_t error_out_sz) {
    int needs_pair = 1;
    EVP_PKEY *root_key = NULL;
    EVP_PKEY *host_key = NULL;
    EVP_PKEY *dev_pubkey = NULL;
    X509 *root_cert = NULL;
    X509 *host_cert = NULL;
    X509 *dev_cert = NULL;

    srand((unsigned)time(NULL));

    char udid[128] = {0};
    if (!lockdownd_get_value_string(&client->session, "UniqueDeviceID", udid, sizeof(udid))) {
        fprintf(stderr, "GetValue(UniqueDeviceID) failed\n");
        return 1;
    }

    char record_path[512];
    pairing_record_path(udid, record_path, sizeof(record_path));

    char host_id[64] = {0};
    pairing_record_info_t record_info;
    if (load_pairing_record_info(record_path, &record_info)) {
        snprintf(host_id, sizeof(host_id), "%s", record_info.host_id);

        needs_pair = 0;
    }

    if (needs_pair) {
        if (!lockdownd_pair(
                &client->session,
                record_path,
                host_id,
                sizeof(host_id),
                &root_key,
                &host_key,
                &dev_pubkey,
                &root_cert,
                &host_cert,
                &dev_cert)) {
            return 1;
        }
    }

    if (!lockdownd_start_session(&client->session, host_id, session_id, sizeof(session_id), error_out, error_out_sz)) {
        if (!needs_pair && strcmp(error_out, "InvalidHostID") == 0) {
            fprintf(stderr, "Saved pairing record rejected (InvalidHostID).\n");
            fprintf(stderr, "Deleting %s — re-run to re-pair.\n", record_path);
            remove(record_path);
        }
        else if (error_out[0]) {
            fprintf(stderr, "StartSession failed: %s\n", error_out);
        }
        else {
            fprintf(stderr, "StartSession failed\n");
        }

        if (root_cert) {
            X509_free(root_cert);
        }

        if (host_cert) {
            X509_free(host_cert);
        }

        if (dev_cert) {
            X509_free(dev_cert);
        }

        if (root_key) {
            EVP_PKEY_free(root_key);
        }

        if (host_key) {
            EVP_PKEY_free(host_key);
        }

        if (dev_pubkey) {
            EVP_PKEY_free(dev_pubkey);
        }

        return 1;
    }

    if (root_cert) {
        X509_free(root_cert);
    }

    if (host_cert) {
        X509_free(host_cert);
    }

    if (dev_cert) {
        X509_free(dev_cert);
    }

    if (root_key) {
        EVP_PKEY_free(root_key);
    }

    if (host_key) {
        EVP_PKEY_free(host_key);
    }

    if (dev_pubkey) {
        EVP_PKEY_free(dev_pubkey);
    }

    return 0;
}
