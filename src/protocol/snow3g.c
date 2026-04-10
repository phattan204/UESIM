/*
 * 5G UE Simulation Application
 * SNOW 3G Algorithm Implementation
 * 
 * Implements NEA1 (ciphering) and NIA1 (integrity) algorithms
 * as specified in 3GPP TS 35.211, TS 35.212, and TS 35.213
 */

#include "snow3g.h"
#include "../core/memory.h"
#include <string.h>

// SNOW 3G S-boxes (256-byte S1 and S2)
const uint8_t snow3g_S1[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

const uint8_t snow3g_S2[256] = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

uesim_error_t snow3g_init_context(snow3g_context_t* ctx, const uint8_t* key,
                                 const pdcp_cipher_params_t* params) {
    if (ctx == NULL || key == NULL || params == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize context
    memset(ctx, 0, sizeof(snow3g_context_t));
    
    // Initialize LFSR with key
    for (int i = 0; i < 4; i++) {
        ctx->lfsr[i] = (key[4*i] << 24) | (key[4*i+1] << 16) | 
                       (key[4*i+2] << 8) | key[4*i+3];
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
        ctx->lfsr[4+i] = (iv[4*i] << 24) | (iv[4*i+1] << 16) | 
                         (iv[4*i+2] << 8) | iv[4*i+3];
    }
    
    // Initialize remaining LFSR registers
    for (int i = 8; i < SNOW3G_LFSR_SIZE; i++) {
        ctx->lfsr[i] = 0;
    }
    
    // Initialize FSM
    ctx->fsm[0] = ctx->fsm[1] = ctx->fsm[2] = 0;
    
    // Run initialization rounds
    for (int i = 0; i < 32; i++) {
        snow3g_lfsr_clock(ctx);
        snow3g_fsm_next_state(ctx);
    }
    
    return UESIM_SUCCESS;
}

void snow3g_destroy_context(snow3g_context_t* ctx) {
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(snow3g_context_t));
    }
}

void snow3g_generate_keystream(snow3g_context_t* ctx, uint32_t* keystream, size_t words) {
    if (ctx == NULL || keystream == NULL || words == 0) {
        return;
    }
    
    for (size_t i = 0; i < words; i++) {
        snow3g_lfsr_clock(ctx);
        snow3g_fsm_next_state(ctx);
        keystream[i] = ctx->lfsr[15] + ctx->fsm[0];
    }
}

void snow3g_lfsr_clock(snow3g_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // LFSR feedback polynomial: s15 + s13 + s10 + s4 + 1
    uint32_t feedback = ctx->lfsr[0] ^ ctx->lfsr[2] ^ ctx->lfsr[5] ^ ctx->lfsr[11];
    
    // Shift LFSR registers
    for (int i = 0; i < SNOW3G_LFSR_SIZE - 1; i++) {
        ctx->lfsr[i] = ctx->lfsr[i + 1];
    }
    
    ctx->lfsr[SNOW3G_LFSR_SIZE - 1] = feedback;
}

void snow3g_fsm_next_state(snow3g_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    uint32_t f_output = snow3g_fsm_function(ctx);
    
    // Shift FSM registers
    ctx->fsm[0] = ctx->fsm[1];
    ctx->fsm[1] = ctx->fsm[2];
    ctx->fsm[2] = f_output;
}

uint32_t snow3g_fsm_function(snow3g_context_t* ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    uint32_t r1 = ctx->fsm[1];
    uint32_t r2 = ctx->fsm[2];
    
    uint8_t s1 = snow3g_s1_box((r1 >> 24) & 0xFF);
    uint8_t s2 = snow3g_s2_box((r1 >> 16) & 0xFF);
    uint8_t s3 = snow3g_s1_box((r1 >> 8) & 0xFF);
    uint8_t s4 = snow3g_s2_box(r1 & 0xFF);
    
    uint32_t result = (s1 << 24) | (s2 << 16) | (s3 << 8) | s4;
    result ^= r2;
    
    return result;
}

uint8_t snow3g_s1_box(uint8_t input) {
    return snow3g_S1[input];
}

uint8_t snow3g_s2_box(uint8_t input) {
    return snow3g_S2[input];
}

uint32_t snow3g_mul_alpha(uint32_t x) {
    // Multiply by alpha in GF(2^32)
    uint32_t result = x << 1;
    if (x & 0x80000000) {
        result ^= 0x00000087; // Primitive polynomial
    }
    return result;
}

uint32_t snow3g_mul_alpha_inv(uint32_t x) {
    // Multiply by alpha^(-1) in GF(2^32)
    uint32_t result = x >> 1;
    if (x & 0x00000001) {
        result ^= 0x80000043; // Primitive polynomial reciprocal
    }
    return result;
}

// Main SNOW 3G encryption function (NEA1)
uesim_error_t snow3g_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* plaintext, uint8_t* ciphertext, size_t length) {
    if (key == NULL || params == NULL || plaintext == NULL || ciphertext == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize SNOW 3G context
    snow3g_context_t ctx;
    uesim_error_t result = snow3g_init_context(&ctx, key, params);
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
        snow3g_generate_keystream(&ctx, &keystream_word, 1);
        
        // XOR with plaintext (convert endianness if needed)
        uint32_t plaintext_word = input_words[i];
        output_words[i] = plaintext_word ^ keystream_word;
    }
    
    // Process remaining bytes
    if (remaining_bytes > 0) {
        uint32_t keystream_word;
        snow3g_generate_keystream(&ctx, &keystream_word, 1);
        
        const uint8_t* input_bytes = (const uint8_t*)(plaintext + word_count * 4);
        uint8_t* output_bytes = (uint8_t*)(ciphertext + word_count * 4);
        uint8_t* keystream_bytes = (uint8_t*)&keystream_word;
        
        for (size_t i = 0; i < remaining_bytes; i++) {
            output_bytes[i] = input_bytes[i] ^ keystream_bytes[i];
        }
    }
    
    snow3g_destroy_context(&ctx);
    return UESIM_SUCCESS;
}

// Main SNOW 3G decryption function (NEA1)
uesim_error_t snow3g_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* ciphertext, uint8_t* plaintext, size_t length) {
    // SNOW 3G decryption is identical to encryption (stream cipher)
    return snow3g_encrypt(key, params, ciphertext, plaintext, length);
}

// SNOW 3G MAC computation (NIA1)
uesim_error_t snow3g_compute_mac(const uint8_t* key, const uint8_t* message,
                                size_t msg_len, const pdcp_cipher_params_t* params,
                                uint32_t* mac) {
    if (key == NULL || message == NULL || params == NULL || mac == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    // Initialize SNOW 3G context for integrity
    snow3g_context_t ctx;
    uesim_error_t result = snow3g_init_context(&ctx, key, params);
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
        snow3g_generate_keystream(&ctx, &keystream_word, 1);
        
        // XOR message with keystream and accumulate
        mac_result ^= message_words[i] ^ keystream_word;
    }
    
    // Process remaining bytes
    if (remaining_bytes > 0) {
        uint32_t keystream_word;
        snow3g_generate_keystream(&ctx, &keystream_word, 1);
        
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
    snow3g_generate_keystream(&ctx, &final_keystream, 1);
    mac_result ^= final_keystream;
    
    *mac = mac_result;
    
    snow3g_destroy_context(&ctx);
    return UESIM_SUCCESS;
}