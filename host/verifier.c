#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/pem.h>

#define HEADER_SIZE 12u
#define MAX_PAYLOAD (1024u * 1024u)
#define SIGNATURE_SIZE 64u

/* Practice policy: accept version 2 or newer. */
#define MIN_VERSION 2u

/*
 * Read at most capacity bytes.
 * Reject files that are larger than the destination buffer.
 */
static int read_file(const char *path,
                     unsigned char *buffer,
                     size_t capacity,
                     size_t *length)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        perror(path);
        return 0;
    }

    size_t count = fread(buffer, 1, capacity, file);
    int extra = fgetc(file);
    int failed = ferror(file);

    fclose(file);

    if (failed) {
        fprintf(stderr, "REJECT: could not read %s\n", path);
        return 0;
    }

    if (extra != EOF) {
        fprintf(stderr, "REJECT: file too large: %s\n", path);
        return 0;
    }

    *length = count;
    return 1;
}

/* Decode four bytes without relying on struct layout or alignment. */
static uint32_t read_u32_le(const unsigned char *bytes)
{
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

static int verify_signature(const char *public_key_path,
                            const unsigned char *data,
                            size_t data_length,
                            const unsigned char *signature,
                            size_t signature_length)
{
    FILE *file = fopen(public_key_path, "rb");

    if (file == NULL) {
        perror(public_key_path);
        return 0;
    }

    EVP_PKEY *key = PEM_read_PUBKEY(file, NULL, NULL, NULL);
    fclose(file);

    if (key == NULL) {
        fprintf(stderr, "REJECT: cannot load public key\n");
        return 0;
    }

    if (EVP_PKEY_is_a(key, "ED25519") != 1) {
        fprintf(stderr, "REJECT: expected an Ed25519 public key\n");
        EVP_PKEY_free(key);
        return 0;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    int valid = 0;

    if (context != NULL &&
        EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1) {
        valid = EVP_DigestVerify(
            context,
            signature,
            signature_length,
            data,
            data_length
        ) == 1;
    }

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);

    return valid;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s firmware.bin firmware.sig public.pem\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const size_t capacity = HEADER_SIZE + MAX_PAYLOAD;
    unsigned char *firmware = malloc(capacity);

    if (firmware == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        return EXIT_FAILURE;
    }

    unsigned char signature[SIGNATURE_SIZE];
    size_t firmware_length = 0;
    size_t signature_length = 0;
    int result = EXIT_FAILURE;

    if (!read_file(argv[1], firmware, capacity, &firmware_length) ||
        !read_file(argv[2], signature, sizeof(signature),
                   &signature_length)) {
        goto cleanup;
    }

    if (firmware_length < HEADER_SIZE) {
        fprintf(stderr, "REJECT: truncated header\n");
        goto cleanup;
    }

    if (memcmp(firmware, "FWUP", 4) != 0) {
        fprintf(stderr, "REJECT: incorrect magic marker\n");
        goto cleanup;
    }

    uint32_t version = read_u32_le(firmware + 4);
    uint32_t payload_length = read_u32_le(firmware + 8);

    if (payload_length == 0 || payload_length > MAX_PAYLOAD) {
        fprintf(stderr, "REJECT: invalid payload length\n");
        goto cleanup;
    }

    if ((size_t)payload_length != firmware_length - HEADER_SIZE) {
        fprintf(stderr, "REJECT: payload length mismatch\n");
        goto cleanup;
    }

    if (signature_length != SIGNATURE_SIZE) {
        fprintf(stderr, "REJECT: incorrect signature length\n");
        goto cleanup;
    }

    if (!verify_signature(argv[3], firmware, firmware_length,
                          signature, signature_length)) {
        fprintf(stderr, "REJECT: signature verification failed\n");
        goto cleanup;
    }

    /* Apply version policy after authenticating the package. */
    if (version < MIN_VERSION) {
        fprintf(stderr,
                "REJECT: version %" PRIu32 " is below minimum %u\n",
                version, MIN_VERSION);
        goto cleanup;
    }

    printf("ACCEPT: firmware version %" PRIu32
           ", payload %" PRIu32 " bytes\n",
           version, payload_length);

    result = EXIT_SUCCESS;

cleanup:
    free(firmware);
    return result;
}