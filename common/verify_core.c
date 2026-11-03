#include "verify_core.h"
#include "monocypher-ed25519.h"


static uint32_t rd32(const uint8_t *b) 
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

img_result img_verify(const uint8_t *image, size_t available, uint32_t expected_addr, uint32_t min_version, const uint8_t public_key[32], img_info *out)
{
    if(available < IMG_HEADER_SIZE + IMG_SIG_SIZE) return IMG_ERR_MAGIC;

    if (image[0] != 'F' || image[1] != 'W' ||
        image[2] != 'U' || image[3] != 'P') return IMG_ERR_MAGIC;

    if (length == 0 || length > available - IMG_HEADER_SIZE - IMG_SIG_SIZE) return IMG_ERR_LENGTH;
    
    if(expected_addr != 0 && addr != expected_addr) return IMG_ERR_ADDR;

    const uint8_t *sig = image + IMG_HEADER_SIZE + length;
    if(crypto_ed25519_check(sig, public_key, image, IMG_HEADER_SIZE + length) != 0) return IMG_ERR_SIG;
    if (out) { out->version = version; out->payload_length = length;
               out->load_addr = addr; }
    return IMG_OK;
}
