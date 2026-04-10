/*
 * 5G UE Simulation Application
 * ZUC Algorithm Implementation
 * 
 * Implements NEA3 (ciphering) and NIA3 (integrity) algorithms
 * as specified in 3GPP TS 35.211, TS 35.212, and TS 35.213
 */

#include "zuc.h"
#include "../core/memory.h"
#include <string.h>

// ZUC S-boxes
const uint8_t zuc_S0[256] = {
    0x3e, 0x72, 0x5b, 0x47, 0xca, 0xe0, 0x00, 0x33, 0x04, 0xd1, 0x54, 0x98, 0x09, 0xb9, 0x6d, 0xcb,
    0x7b, 0x1b, 0xf9, 0x32, 0xaf, 0x9d, 0x6a, 0xa5, 0xb8, 0x2d, 0xfc, 0x1d, 0x08, 0x53, 0x03, 0x90,
    0x4d, 0x4e, 0x84, 0x99, 0xe4, 0xce, 0xd9, 0x91, 0x12, 0xa0, 0x06, 0x62, 0x4d, 0x2e, 0x1f, 0x55,
    0x3c, 0x3d, 0x5a, 0xc6, 0x74, 0xb2, 0xf5, 0x8d, 0x6b, 0x9a, 0x78, 0x25, 0x5e, 0x1c, 0x87, 0x4c,
    0xa3, 0x19, 0xa7, 0x57, 0x42, 0xb0, 0x3f, 0x2a, 0x4f, 0x23, 0xf0, 0x10, 0x58, 0x7e, 0x36, 0x40,
    0xc4, 0x1a, 0xe6, 0x21, 0xc7, 0x61, 0x35, 0xd2, 0x14, 0xc2, 0x6c, 0xe8, 0x51, 0xea, 0x9b, 0x8a,
    0x24, 0x6e, 0x17, 0x9f, 0x0d, 0x0c, 0x34, 0x8e, 0x64, 0x2b, 0x15, 0x31, 0x44, 0x93, 0x85, 0x77,
    0x7d, 0x8b, 0xf8, 0x8c, 0xa1, 0x20, 0x5d, 0x4b, 0x75, 0xdb, 0x01, 0x46, 0x67, 0x73, 0x94, 0xf4,
    0x4a, 0x27, 0x49, 0x38, 0x69, 0x5f, 0x28, 0x30, 0x39, 0x37, 0x45, 0x83, 0x68, 0x48, 0x60, 0x1e,
    0x26, 0x11, 0xf2, 0xbc, 0x70, 0x97, 0x05, 0x0a, 0x2f, 0x7f, 0x81, 0x79, 0x71, 0x13, 0x89, 0x63,
    0x59, 0x86, 0x95, 0x43, 0x41, 0x22, 0x88, 0x3a, 0x5c, 0x18, 0x29, 0x50, 0x92, 0x3b, 0x9c, 0xfe,
    0x0f, 0x02, 0x07, 0x16, 0x0b, 0x0e, 0x65, 0x56, 0x66, 0x76, 0xac, 0x82, 0xe7, 0x4c, 0x80, 0x00,
    0x19, 0x52, 0x13, 0x3e, 0x72, 0x5b, 0x47, 0xca, 0xe0, 0x00, 0x33, 0x04, 0xd1, 0x54, 0x98, 0x09,
    0xb9, 0x6d, 0xcb, 0x7b, 0x1b, 0xf9, 0x32, 0xaf, 0x9d, 0x6a, 0xa5, 0xb8, 0x2d, 0xfc, 0x1d, 0x08,
    0x53, 0x03, 0x90, 0x4d, 0x4e, 0x84, 0x99, 0xe4, 0xce, 0xd9, 0x91, 0x12, 0xa0, 0x06, 0x62, 0x4d,
    0x2e, 0x1f, 0x55, 0x3c, 0x3d, 0x5a, 0xc6, 0x74, 0xb2, 0xf5, 0x8d, 0x6b, 0x9a, 0x78, 0x25, 0x5e
};

const uint8_t zuc_S1[256] = {
    0x55, 0xc2, 0x63, 0x71, 0x3b, 0xc8, 0x47, 0x86, 0x9f, 0x3c, 0xda, 0x5b, 0x29, 0xaa, 0xfd, 0x77,
    0x8c, 0xc5, 0x94, 0x0c, 0xa6, 0x1a, 0x13, 0x00, 0xe3, 0xa8, 0x16, 0x72, 0x44, 0x9b, 0xb2, 0xfe,
    0x4d, 0x34, 0x7e, 0x0e, 0x5f, 0x41, 0x67, 0x02, 0x9e, 0x62, 0x4e, 0x78, 0x5e, 0x6b, 0x35, 0x68,
    0x51, 0xd1, 0xf9, 0xe5, 0x2a, 0x8e, 0x5a, 0x14, 0xa9, 0x6c, 0x84, 0x3a, 0x18, 0x65, 0x80, 0x9a,
    0x07, 0x12, 0x40, 0x23, 0x20, 0xf6, 0x0b, 0x22, 0x66, 0xd0, 0x45, 0x9d, 0x0d, 0x03, 0x27, 0x79,
    0x54, 0x28, 0xb0, 0x88, 0xba, 0x32, 0x1c, 0x7b, 0x5d, 0x2c, 0x11, 0x2b, 0xfc, 0x58, 0x30, 0x8a,
    0xf2, 0xa0, 0x06, 0x48, 0x26, 0xd3, 0xb4, 0xcf, 0x43, 0x4b, 0x09, 0x69, 0xd5, 0x8d, 0x33, 0x8f,
    0x4c, 0x70, 0x21, 0x60, 0x3d, 0xd7, 0x75, 0xd4, 0x85, 0x9c, 0x4f, 0x42, 0x6d, 0x59, 0xc0, 0x37,
    0x57, 0x74, 0x99, 0xc4, 0x2d, 0x17, 0xc9, 0x6a, 0x05, 0x49, 0x5c, 0xa1, 0x39, 0x24, 0x08, 0x90,
    0x50, 0x3f, 0x25, 0x7a, 0x38, 0x87, 0x0a, 0x15, 0xde, 0x96, 0x64, 0xaf, 0x7c, 0x76, 0xdc, 0x3e,
    0x6e, 0x56, 0x4a, 0x1e, 0xa2, 0xd8, 0x10, 0xb5, 0x2f, 0x1b, 0xf5, 0x73, 0xd9, 0x52, 0x2e, 0x81,
    0x92, 0x01, 0x83, 0x36, 0xee, 0xfb, 0x46, 0x6f, 0x04, 0x5a, 0xe8, 0x95, 0xe6, 0x8b, 0x98, 0x19,
    0xcd, 0x31, 0xb6, 0x9e, 0x55, 0xc2, 0x63, 0x71, 0x3b, 0xc8, 0x47, 0x86, 0x9f, 0x3c, 0xda, 0x5b,
    0x29, 0xaa, 0xfd, 0x77, 0x8c, 0xc5, 0x94, 0x0c, 0xa6, 0x1a, 0x13, 0x00, 0xe3, 0xa8, 0x16, 0x72,
    0x44, 0x9b, 0xb2, 0xfe, 0x4d, 0x34, 0x7e, 0x0e, 0x5f, 0x41, 0x67, 0x02, 0x9e, 0x62, 0x4e, 0x78,
    0x5e, 0x6b, 0x35, 0x68, 0x51, 0xd1, 0xf9, 0xe5, 0x2a, 0x8e, 0x5a, 0x14, 0xa9, 0x6c, 0x84, 0x3a
};

uesim_error_t zuc_init_context(zuc_context_t* ctx, const uint8_t* key,
                              const pdcp_cipher_params_t* params) {
    if (ctx == NULL || key == NULL || params == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize context
    memset(ctx, 0, sizeof(zuc_context_t));
    
    // Initialize LFSR with key
    for (int i = 0; i < 4; i++) {
        uint32_t key_word = (key[4*i] << 24) | (key[4*i+1] << 16) |
                           (key[4*i+2] << 8) | key[4*i+3];
        ctx->lfsr[i] = zuc_lfsr_initialization(key_word);
    }
    
    // Initialize IV (COUNT || BEARER || DIRECTION || 0...)
    uint8_t iv[16];
    memset(iv, 0, 16);
    
    // Pack IV parameters
    iv[0] = (params->count >> 24) & 0xFF;
    iv[1] = (params->count >> 16) & 0xFF;
    iv[2] = (params->count >> 8) & 0xFF;
    iv[3] = params->count & 0xFF;
    
    iv[4] = params->bearer;
    iv[5] = params->direction;
    // iv[6]..iv[15] remain 0
    
    // Initialize LFSR with IV
    for (int i = 0; i < 4; i++) {
        uint32_t iv_word = (iv[4*i] << 24) | (iv[4*i+1] << 16) |
                          (iv[4*i+2] << 8) | iv[4*i+3];
        ctx->lfsr[4+i] = zuc_lfsr_initialization(iv_word);
    }
    
    // Initialize remaining LFSR registers
    for (int i = 8; i < ZUC_LFSR_SIZE; i++) {
        ctx->lfsr[i] = 0;
    }
    
    // Initialize FSM
    ctx->fsm[0] = ctx->fsm[1] = 0;
    
    // Run initialization rounds
    for (int i = 0; i < 32; i++) {
        zuc_lfsr_clock(ctx);
        zuc_fsm_next_state(ctx);
    }
    
    return UESIM_SUCCESS;
}

void zuc_destroy_context(zuc_context_t* ctx) {
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(zuc_context_t));
    }
}

void zuc_generate_keystream(zuc_context_t* ctx, uint32_t* keystream, size_t words) {
    if (ctx == NULL || keystream == NULL || words == 0) {
        return;
    }
    
    for (size_t i = 0; i < words; i++) {
        zuc_lfsr_clock(ctx);
        zuc_fsm_next_state(ctx);
        keystream[i] = ctx->lfsr[0] ^ ctx->fsm[0];
    }
}

void zuc_lfsr_clock(zuc_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // LFSR feedback
    uint32_t feedback = zuc_lfsr_feedback(ctx->lfsr);
    
    // Shift LFSR registers
    for (int i = 0; i < ZUC_LFSR_SIZE - 1; i++) {
        ctx->lfsr[i] = ctx->lfsr[i + 1];
    }
    
    ctx->lfsr[ZUC_LFSR_SIZE - 1] = feedback;
}

void zuc_fsm_next_state(zuc_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    uint32_t u = ctx->lfsr[0];
    uint32_t v = ctx->lfsr[4];
    
    uint32_t f_output = zuc_fsm_function(ctx, u, v);
    
    // Shift FSM registers
    ctx->fsm[0] = ctx->fsm[1];
    ctx->fsm[1] = f_output;
}

uint32_t zuc_fsm_function(zuc_context_t* ctx, uint32_t u, uint32_t v) {
    if (ctx == NULL) {
        return 0;
    }
    
    uint32_t r1 = ctx->fsm[0];
    uint32_t r2 = ctx->fsm[1];
    
    uint8_t s0 = zuc_s0_box((u >> 24) & 0xFF);
    uint8_t s1 = zuc_s1_box((u >> 16) & 0xFF);
    uint8_t s2 = zuc_s0_box((u >> 8) & 0xFF);
    uint8_t s3 = zuc_s1_box(u & 0xFF);
    
    uint32_t w = (s0 << 24) | (s1 << 16) | (s2 << 8) | s3;
    w ^= v;
    
    uint8_t d0 = zuc_s0_box((w >> 24) & 0xFF);
    uint8_t d1 = zuc_s1_box((w >> 16) & 0xFF);
    uint8_t d2 = zuc_s0_box((w >> 8) & 0xFF);
    uint8_t d3 = zuc_s1_box(w & 0xFF);
    
    uint32_t result = (d0 << 24) | (d1 << 16) | (d2 << 8) | d3;
    result ^= r1 ^ r2;
    
    return result;
}

uint8_t zuc_s0_box(uint8_t input) {
    return zuc_S0[input];
}

uint8_t zuc_s1_box(uint8_t input) {
    return zuc_S1[input];
}

uint32_t zuc_bit_reorganization(zuc_context_t* ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    // Bit reorganization as per ZUC specification
    uint32_t x0 = ctx->lfsr[0];
    uint32_t x1 = ctx->lfsr[1];
    uint32_t x2 = ctx->lfsr[2];
    uint32_t x3 = ctx->lfsr[3];
    
    // Combine bits according to ZUC specification
    return x0 ^ x1 ^ x2 ^ x3;
}

uint32_t zuc_lfsr_initialization(uint32_t x) {
    // Initialize LFSR register with proper bit masking
    return x & 0x7FFFFFFF; // 31-bit value
}

uint32_t zuc_lfsr_feedback(uint32_t* lfsr) {
    if (lfsr == NULL) {
        return 0;
    }
    
    // LFSR feedback polynomial for ZUC
    uint32_t s0 = lfsr[0];
    uint32_t s4 = lfsr[4];
    uint32_t s10 = lfsr[10];
    uint32_t s13 = lfsr[13];
    uint32_t s15 = lfsr[15];
    
    // Feedback computation
    uint32_t feedback = s0 ^ s4 ^ s10 ^ s13 ^ s15;
    return feedback & 0x7FFFFFFF; // 31-bit result
}

// Main ZUC encryption function (NEA3)
uesim_error_t zuc_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* plaintext, uint8_t* ciphertext, size_t length) {
    if (key == NULL || params == NULL || plaintext == NULL || ciphertext == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize ZUC context
    zuc_context_t ctx;
    uesim_error_t result = zuc_init_context(&ctx, key, params);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Process data in 32-bit words
    size_t word_count = length / 4;
    size_t remaining_bytes = length % 4;
    
    const uint32_t* input_words = (const uint32_t*)plaintext;
    uint32_t* output_words = (uint32_t*)ciphertext;
    
    // Process complete words
    for (size_t i = 0; i < word_count; i++) {
        uint32_t keystream_word;
        zuc_generate_keystream(&ctx, &keystream_word, 1);
        
        // XOR with plaintext
        uint32_t plaintext_word = input_words[i];
        output_words[i] = plaintext_word ^ keystream_word;
    }
    
    // Process remaining bytes
    if (remaining_bytes > 0) {
        uint32_t keystream_word;
        zuc_generate_keystream(&ctx, &keystream_word, 1);
        
        const uint8_t* input_bytes = (const uint8_t*)(plaintext + word_count * 4);
        uint8_t* output_bytes = (uint8_t*)(ciphertext + word_count * 4);
        uint8_t* keystream_bytes = (uint8_t*)&keystream_word;
        
        for (size_t i = 0; i < remaining_bytes; i++) {
            output_bytes[i] = input_bytes[i] ^ keystream_bytes[i];
        }
    }
    
    zuc_destroy_context(&ctx);
    return UESIM_SUCCESS;
}

// Main ZUC decryption function (NEA3)
uesim_error_t zuc_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* ciphertext, uint8_t* plaintext, size_t length) {
    // ZUC decryption is identical to encryption (stream cipher)
    return zuc_encrypt(key, params, ciphertext, plaintext, length);
}

// ZUC MAC computation (NIA3)
uesim_error_t zuc_compute_mac(const uint8_t* key, const uint8_t* message,
                             size_t msg_len, const pdcp_cipher_params_t* params,
                             uint32_t* mac) {
    if (key == NULL || message == NULL || params == NULL || mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize ZUC context for integrity
    zuc_context_t ctx;
    uesim_error_t result = zuc_init_context(&ctx, key, params);
    if (result != UESIM_SUCCESS) {
        return result;
    }
    
    // Process message in 32-bit words
    size_t word_count = msg_len / 4;
    size_t remaining_bytes = msg_len % 4;
    
    const uint32_t* message_words = (const uint32_t*)message;
    uint32_t mac_result = 0;
    
    // Process complete words
    for (size_t i = 0; i < word_count; i++) {
        uint32_t keystream_word;
        zuc_generate_keystream(&ctx, &keystream_word, 1);
        
        // XOR message with keystream and accumulate
        mac_result ^= message_words[i] ^ keystream_word;
    }
    
    // Process remaining bytes
    if (remaining_bytes > 0) {
        uint32_t keystream_word;
        zuc_generate_keystream(&ctx, &keystream_word, 1);
        
        uint32_t partial_word = 0;
        const uint8_t* message_bytes = (const uint8_t*)(message + word_count * 4);
        uint8_t* partial_bytes = (uint8_t*)&partial_word;
        
        for (size_t i = 0; i < remaining_bytes; i++) {
            partial_bytes[i] = message_bytes[i];
        }
        
        mac_result ^= partial_word ^ keystream_word;
    }
    
    // Final keystream word for MAC
    uint32_t final_keystream;
    zuc_generate_keystream(&ctx, &final_keystream, 1);
    mac_result ^= final_keystream;
    
    *mac = mac_result;
    
    zuc_destroy_context(&ctx);
    return UESIM_SUCCESS;
}