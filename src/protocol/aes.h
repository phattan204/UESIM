/*
 * 5G UE Simulation Application
 * AES Algorithm Implementation Header
 */

#ifndef AES_H
#define AES_H

#include "pdcp.h"

// AES Constants
#define AES_BLOCK_SIZE 16       // 128 bits
#define AES_KEY_SIZE 16         // 128 bits
#define AES_ROUNDS 10           // For 128-bit key

// AES Context Structure
typedef struct {
    uint32_t round_key[AES_ROUNDS + 1][4];  // Round keys
    uint8_t key[AES_KEY_SIZE];              // Original key
} aes_context_t;

// Function prototypes
uesim_error_t aes_init_context(aes_context_t* ctx, const uint8_t* key);
void aes_destroy_context(aes_context_t* ctx);
void aes_encrypt_block(aes_context_t* ctx, const uint8_t* plaintext, uint8_t* ciphertext);
void aes_decrypt_block(aes_context_t* ctx, const uint8_t* ciphertext, uint8_t* plaintext);
void aes_ctr_encrypt(aes_context_t* ctx, const uint8_t* iv, const uint8_t* plaintext,
                     uint8_t* ciphertext, size_t length);
void aes_cmac(aes_context_t* ctx, const uint8_t* message, size_t msg_len, uint8_t* mac);

// AES helper functions
void aes_key_expansion(aes_context_t* ctx);
void aes_add_round_key(uint8_t state[4][4], uint32_t round_key[4]);
void aes_sub_bytes(uint8_t state[4][4]);
void aes_shift_rows(uint8_t state[4][4]);
void aes_mix_columns(uint8_t state[4][4]);
void aes_inv_sub_bytes(uint8_t state[4][4]);
void aes_inv_shift_rows(uint8_t state[4][4]);
void aes_inv_mix_columns(uint8_t state[4][4]);

// AES S-box
extern const uint8_t aes_sbox[256];
extern const uint8_t aes_inv_sbox[256];

#endif // AES_H