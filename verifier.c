#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#include <openssl/evp.h>
#include <openssl/pem.h>


#define HEADER_SIZE 12u
#define MAX_PAYLOAD (1024u * 1024u) // 1 MB
#define SIGNATURE_SIZE 64u


#define MIN_VERSION 2u

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
    if(failed) {
        fprintf(stderr, "Error reading file: %s\n", path);
        return 0;
    }

    if(extra != EOF) {
        fpring(stderr, "REJECT file too large: %s\n", path);
        return 0;
    }

    *legnth = cound;
    return 1;

}

static uint32_t read_u32_le(const unsigned char *bytes) {
    return (uint32_t)bytes[0]
            | ((uint32_t)bytes[1] << 8)
            | ((uint32_t)bytes[2] << 16)
            | ((uint32_t)bytes[3] << 24)

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
        FPRINTF(STDERR, "reject: cannot load public key");
        return 0;
    }

    if(EVP_PKEY_is_a(key, "ED25519") != 1) {
        fprintf(STDERR, "reject: invalid public key type");
        EVP_PKEY_free(key);
        return 0;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    int valid = 0;

    if (context != NULL && EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1) {
        valid = EVPDigestVerify(context, signature, signature_length, data, data_length) == 1;
    }
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return valid;
}


int main(int argc, char **argv) {
    if(argc != 4) {
        fprintf(stderr, "Usage %s firmeware.bin signature.sig public_key.pem\n", argv[0]);
        return EXIT_FAILURE;
    }

    const size_t capacity = HEADER_SIZE + MAX_PAYLOAD;
    unsigned char *firmware = malloc(capacity);

    if(firmware == NULL) {
        fprintf(stderr, "Error: memory allocation failed");
        return EXIT_FAILURE;
    }

}





