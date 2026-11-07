#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "verify_core.h"

#define MAX_PAYLOAD (1024u * 1024u)
#define DEFAULT_MIN_VERSION 2u

static int read_file(const char *path, uint8_t *buffer,
                     size_t capacity, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) { perror(path); return 0; }

    size_t count = fread(buffer, 1, capacity, file);
    int extra = fgetc(file);
    int failed = ferror(file);
    fclose(file);

    if (failed) { fprintf(stderr, "REJECT: could not read %s\n", path); return 0; }
    if (extra != EOF) { fprintf(stderr, "REJECT: file too large: %s\n", path); return 0; }

    *length = count;
    return 1;
}

static const char *result_name(img_result r)
{
    switch (r) {
    case IMG_OK:          return "ok";
    case IMG_ERR_SIZE:    return "image too small";
    case IMG_ERR_MAGIC:   return "incorrect magic marker";
    case IMG_ERR_LENGTH:  return "invalid payload length";
    case IMG_ERR_ADDR:    return "wrong load address";
    case IMG_ERR_SIG:     return "signature verification failed";
    case IMG_ERR_VERSION: return "version below minimum";
    }
    return "unknown error";
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s image.bin public.raw [min_version]\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint32_t min_version = DEFAULT_MIN_VERSION;
    if (argc == 4) min_version = (uint32_t)strtoul(argv[3], NULL, 0);

    uint8_t key[32];
    size_t key_len = 0;
    if (!read_file(argv[2], key, sizeof(key), &key_len) || key_len != 32) {
        fprintf(stderr, "REJECT: public key must be exactly 32 raw bytes\n");
        return EXIT_FAILURE;
    }

    const size_t capacity = IMG_HEADER_SIZE + MAX_PAYLOAD + IMG_SIG_SIZE;
    uint8_t *image = malloc(capacity);
    if (image == NULL) { fprintf(stderr, "ERROR: out of memory\n"); return EXIT_FAILURE; }

    int result = EXIT_FAILURE;
    size_t image_len = 0;
    img_info info;

    if (!read_file(argv[1], image, capacity, &image_len)) goto cleanup;

    img_result r = img_verify(image, image_len, 0, min_version, key, &info);
    if (r != IMG_OK) { fprintf(stderr, "REJECT: %s\n", result_name(r)); goto cleanup; }

    if (image_len != IMG_HEADER_SIZE + info.payload_length + IMG_SIG_SIZE) {
        fprintf(stderr, "REJECT: unexpected trailing data\n");
        goto cleanup;
    }

    printf("ACCEPT: version %" PRIu32 ", payload %" PRIu32 " bytes, load 0x%08" PRIX32 "\n",
           info.version, info.payload_length, info.load_addr);
    result = EXIT_SUCCESS;

cleanup:
    free(image);
    return result;
}
