#pragma once
#include <stddef.h>
#include <stdint.h>


#define IMG_HEADER_SIZE 0x200u
#define IMG_SIG_SIZE 64u


typedef enum {
    IMG_OK = 0, IMG_ERR_SIZE, IMG_ERR_MAGIC, IMG_ERR_LENGTH,
    IMG_ERR_ADDR, IMG_ERR_SIG, IMG_ERR_VERSION
} img_result;

typedef struct {
    uint32_t version;
    uint32_t payload_length;
    uint32_t load_address;
} img_info;

img_result img_verify(const uint8_t *image, size_t available, uint32_t expected_addr, uint32_t min_version, const uint8_t public_key[32], img_info *out);
