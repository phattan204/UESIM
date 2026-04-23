/*
 * 5G UE Simulation Application
 * AES Algorithm Implementation
 * 
 * Implements NEA2 (ciphering) and NIA2 (integrity) algorithms
 * as specified in 3GPP TS 35.211, TS 35.212, and TS 35.213
 */

#include "aes.h"
#include "../core/memory.h"
#include <string.h>

// AES S-box
const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

const uint8_t aes_inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

uesim_error_t aes_init_context(aes_context_t* ctx, const uint8_t* key) {
    if (ctx == NULL || key == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize context
    memset(ctx, 0, sizeof(aes_context_t));
    
    // Copy key
    memcpy(ctx->key, key, AES_KEY_SIZE);
    
    // Generate round keys
    aes_key_expansion(ctx);
    
    return UESIM_SUCCESS;
}

void aes_destroy_context(aes_context_t* ctx) {
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(aes_context_t));
    }
}

void aes_key_expansion(aes_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Copy original key to first round key
    for (int i = 0; i < 4; i++) {
        ctx->round_key[0][i] = (ctx->key[4*i] << 24) | (ctx->key[4*i+1] << 16) |
                               (ctx->key[4*i+2] << 8) | ctx->key[4*i+3];
    }
    
    // Generate remaining round keys
    uint32_t* rc = (uint32_t[]){0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};
    
    for (int i = 1; i <= AES_ROUNDS; i++) {
        uint32_t temp = ctx->round_key[i-1][3];
        
        // Rotate and substitute
        temp = (aes_sbox[(temp >> 16) & 0xff] << 24) |
               (aes_sbox[(temp >> 8) & 0xff] << 16) |
               (aes_sbox[temp & 0xff] << 8) |
               aes_sbox[(temp >> 24) & 0xff];
        
        // XOR with round constant
        temp ^= rc[i-1] << 24;
        
        ctx->round_key[i][0] = ctx->round_key[i-1][0] ^ temp;
        ctx->round_key[i][1] = ctx->round_key[i-1][1] ^ ctx->round_key[i][0];
        ctx->round_key[i][2] = ctx->round_key[i-1][2] ^ ctx->round_key[i][1];
        ctx->round_key[i][3] = ctx->round_key[i-1][3] ^ ctx->round_key[i][2];
    }
}

void aes_encrypt_block(aes_context_t* ctx, const uint8_t* plaintext, uint8_t* ciphertext) {
    if (ctx == NULL || plaintext == NULL || ciphertext == NULL) {
        return;
    }
    
    // Initialize state
    uint8_t state[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[j][i] = plaintext[i*4 + j];
        }
    }
    
    // Initial round
    aes_add_round_key(state, ctx->round_key[0]);
    
    // Main rounds
    for (int round = 1; round < AES_ROUNDS; round++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, ctx->round_key[round]);
    }
    
    // Final round
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, ctx->round_key[AES_ROUNDS]);
    
    // Copy result
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            ciphertext[i*4 + j] = state[j][i];
        }
    }
}

void aes_decrypt_block(aes_context_t* ctx, const uint8_t* ciphertext, uint8_t* plaintext) {
    if (ctx == NULL || ciphertext == NULL || plaintext == NULL) {
        return;
    }
    
    // Initialize state
    uint8_t state[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[j][i] = ciphertext[i*4 + j];
        }
    }
    
    // Initial round
    aes_add_round_key(state, ctx->round_key[AES_ROUNDS]);
    
    // Main rounds
    for (int round = AES_ROUNDS - 1; round > 0; round--) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(state, ctx->round_key[round]);
        aes_inv_mix_columns(state);
    }
    
    // Final round
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(state, ctx->round_key[0]);
    
    // Copy result
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            plaintext[i*4 + j] = state[j][i];
        }
    }
}

void aes_add_round_key(uint8_t state[4][4], uint32_t round_key[4]) {
    for (int i = 0; i < 4; i++) {
        state[0][i] ^= (round_key[i] >> 24) & 0xff;
        state[1][i] ^= (round_key[i] >> 16) & 0xff;
        state[2][i] ^= (round_key[i] >> 8) & 0xff;
        state[3][i] ^= round_key[i] & 0xff;
    }
}

void aes_sub_bytes(uint8_t state[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[i][j] = aes_sbox[state[i][j]];
        }
    }
}

void aes_shift_rows(uint8_t state[4][4]) {
    uint8_t temp;
    
    // Row 1: shift left by 1
    temp = state[1][0];
    state[1][0] = state[1][1];
    state[1][1] = state[1][2];
    state[1][2] = state[1][3];
    state[1][3] = temp;
    
    // Row 2: shift left by 2
    temp = state[2][0];
    state[2][0] = state[2][2];
    state[2][2] = temp;
    temp = state[2][1];
    state[2][1] = state[2][3];
    state[2][3] = temp;
    
    // Row 3: shift left by 3
    temp = state[3][0];
    state[3][0] = state[3][3];
    state[3][3] = state[3][2];
    state[3][2] = state[3][1];
    state[3][1] = temp;
}

void aes_mix_columns(uint8_t state[4][4]) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0][i];
        uint8_t s1 = state[1][i];
        uint8_t s2 = state[2][i];
        uint8_t s3 = state[3][i];
        
        state[0][i] = (s0 << 1) ^ (s0 & 0x80 ? 0x1b : 0) ^ s1 ^ (s1 << 1) ^ (s1 & 0x80 ? 0x1b : 0) ^ s2 ^ s3;
        state[1][i] = s0 ^ (s1 << 1) ^ (s1 & 0x80 ? 0x1b : 0) ^ s2 ^ (s2 << 1) ^ (s2 & 0x80 ? 0x1b : 0) ^ s3;
        state[2][i] = s0 ^ s1 ^ (s2 << 1) ^ (s2 & 0x80 ? 0x1b : 0) ^ s3 ^ (s3 << 1) ^ (s3 & 0x80 ? 0x1b : 0);
        state[3][i] = s0 ^ (s0 << 1) ^ (s0 & 0x80 ? 0x1b : 0) ^ s1 ^ s2 ^ s3 ^ (s3 << 1) ^ (s3 & 0x80 ? 0x1b : 0);
    }
}

void aes_inv_sub_bytes(uint8_t state[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state[i][j] = aes_inv_sbox[state[i][j]];
        }
    }
}

void aes_inv_shift_rows(uint8_t state[4][4]) {
    uint8_t temp;
    
    // Row 1: shift right by 1
    temp = state[1][3];
    state[1][3] = state[1][2];
    state[1][2] = state[1][1];
    state[1][1] = state[1][0];
    state[1][0] = temp;
    
    // Row 2: shift right by 2
    temp = state[2][0];
    state[2][0] = state[2][2];
    state[2][2] = temp;
    temp = state[2][1];
    state[2][1] = state[2][3];
    state[2][3] = temp;
    
    // Row 3: shift right by 3
    temp = state[3][0];
    state[3][0] = state[3][1];
    state[3][1] = state[3][2];
    state[3][2] = state[3][3];
    state[3][3] = temp;
}

static uint8_t multiply(uint8_t x, uint8_t y);

void aes_inv_mix_columns(uint8_t state[4][4]) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0][i];
        uint8_t s1 = state[1][i];
        uint8_t s2 = state[2][i];
        uint8_t s3 = state[3][i];
        
        state[0][i] = multiply(s0, 0x0e) ^ multiply(s1, 0x0b) ^ multiply(s2, 0x0d) ^ multiply(s3, 0x09);
        state[1][i] = multiply(s0, 0x09) ^ multiply(s1, 0x0e) ^ multiply(s2, 0x0b) ^ multiply(s3, 0x0d);
        state[2][i] = multiply(s0, 0x0d) ^ multiply(s1, 0x09) ^ multiply(s2, 0x0e) ^ multiply(s3, 0x0b);
        state[3][i] = multiply(s0, 0x0b) ^ multiply(s1, 0x0d) ^ multiply(s2, 0x09) ^ multiply(s3, 0x0e);
    }
}

// Helper function for GF(2^8) multiplication
static uint8_t multiply(uint8_t x, uint8_t y) {
    uint8_t result = 0;
    uint8_t temp = x;
    
    for (int i = 0; i < 8; i++) {
        if (y & 1) {
            result ^= temp;
        }
        if (temp & 0x80) {
            temp = (temp << 1) ^ 0x1b;
        } else {
            temp <<= 1;
        }
        y >>= 1;
    }
    
    return result;
}

// AES CTR mode encryption (NEA2)
void aes_ctr_encrypt(aes_context_t* ctx, const uint8_t* iv, const uint8_t* plaintext,
                     uint8_t* ciphertext, size_t length) {
    if (ctx == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL || length == 0) {
        return;
    }
    
    uint8_t counter[AES_BLOCK_SIZE];
    uint8_t keystream[AES_BLOCK_SIZE];
    size_t offset = 0;
    
    // Copy IV to counter
    memcpy(counter, iv, AES_BLOCK_SIZE);
    
    while (offset < length) {
        // Encrypt counter to generate keystream
        aes_encrypt_block(ctx, counter, keystream);
        
        // XOR with plaintext
        size_t block_size = (length - offset < AES_BLOCK_SIZE) ? (length - offset) : AES_BLOCK_SIZE;
        for (size_t i = 0; i < block_size; i++) {
            ciphertext[offset + i] = plaintext[offset + i] ^ keystream[i];
        }
        
        // Increment counter
        for (int i = AES_BLOCK_SIZE - 1; i >= 0; i--) {
            counter[i]++;
            if (counter[i] != 0) {
                break;
            }
        }
        
        offset += block_size;
    }
}

// AES CMAC (NIA2)
void aes_cmac(aes_context_t* ctx, const uint8_t* message, size_t msg_len, uint8_t* mac) {
    if (ctx == NULL || message == NULL || mac == NULL) {
        return;
    }
    
    // Zero initialization vector
    uint8_t iv[AES_BLOCK_SIZE] = {0};
    uint8_t temp[AES_BLOCK_SIZE];
    
    // Process message in blocks
    size_t offset = 0;
    while (offset + AES_BLOCK_SIZE <= msg_len) {
        // XOR with current block
        for (int i = 0; i < AES_BLOCK_SIZE; i++) {
            iv[i] ^= message[offset + i];
        }
        
        // Encrypt
        aes_encrypt_block(ctx, iv, temp);
        memcpy(iv, temp, AES_BLOCK_SIZE);
        
        offset += AES_BLOCK_SIZE;
    }
    
    // Handle last partial block
    if (offset < msg_len) {
        size_t remaining = msg_len - offset;
        for (size_t i = 0; i < remaining; i++) {
            iv[i] ^= message[offset + i];
        }
        // Pad with 0x80 followed by zeros
        iv[remaining] ^= 0x80;
        
        aes_encrypt_block(ctx, iv, temp);
        memcpy(iv, temp, AES_BLOCK_SIZE);
    }
    
    // Final encryption for MAC
    aes_encrypt_block(ctx, iv, mac);
}

// Main AES encryption function (NEA2)
uesim_error_t aes_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* plaintext, uint8_t* ciphertext, size_t length) {
    if (key == NULL || params == NULL || plaintext == NULL || ciphertext == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize AES context
    aes_context_t ctx;
    uesim_error_t result = aes_init_context(&ctx, key);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Create IV from parameters (COUNT || BEARER || DIRECTION || 0...)
    uint8_t iv[AES_BLOCK_SIZE] = {0};
    iv[0] = (params->count >> 24) & 0xFF;
    iv[1] = (params->count >> 16) & 0xFF;
    iv[2] = (params->count >> 8) & 0xFF;
    iv[3] = params->count & 0xFF;
    iv[4] = params->bearer;
    iv[5] = params->direction;
    
    // Perform CTR encryption
    aes_ctr_encrypt(&ctx, iv, plaintext, ciphertext, length);
    
    aes_destroy_context(&ctx);
    return UESIM_SUCCESS;
}

// Main AES decryption function (NEA2)
uesim_error_t aes_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* ciphertext, uint8_t* plaintext, size_t length) {
    // AES CTR decryption is identical to encryption
    return aes_encrypt(key, params, ciphertext, plaintext, length);
}

// AES MAC computation (NIA2)
uesim_error_t aes_compute_mac(const uint8_t* key, const uint8_t* message,
                             size_t msg_len, const pdcp_cipher_params_t* params,
                             uint32_t* mac) {
    if (key == NULL || message == NULL || params == NULL || mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize AES context
    aes_context_t ctx;
    uesim_error_t result = aes_init_context(&ctx, key);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Compute CMAC
    uint8_t full_mac[AES_BLOCK_SIZE];
    aes_cmac(&ctx, message, msg_len, full_mac);
    
    // Truncate to 32 bits
    *mac = (full_mac[0] << 24) | (full_mac[1] << 16) | (full_mac[2] << 8) | full_mac[3];
    
    aes_destroy_context(&ctx);
    return UESIM_SUCCESS;
}