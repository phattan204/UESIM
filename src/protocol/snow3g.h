/*
 * 5G UE Simulation Application
 * SNOW 3G Algorithm Implementation Header
 */

#ifndef SNOW3G_H
#define SNOW3G_H

#include "pdcp.h"

// SNOW 3G Constants
#define SNOW3G_KEY_SIZE 16      // 128 bits
#define SNOW3G_IV_SIZE 16       // 128 bits
#define SNOW3G_LFSR_SIZE 16     // 16 registers of 32 bits each
#define SNOW3G_FSM_SIZE 3       // 3 registers of 32 bits each

// SNOW 3G S-boxes (256-byte S1 and S2)
extern const uint8_t snow3g_S1[256];
extern const uint8_t snow3g_S2[256];

// SNOW 3G Context Structure
typedef struct {
    uint32_t lfsr[SNOW3G_LFSR_SIZE];  // Linear Feedback Shift Register
    uint32_t fsm[SNOW3G_FSM_SIZE];    // Finite State Machine
    uint32_t keystream_buffer[4];     // Keystream buffer
    size_t buffer_index;              // Current position in buffer
} snow3g_context_t;

// Function prototypes
uesim_error_t snow3g_init_context(snow3g_context_t* ctx, const uint8_t* key,
                                 const pdcp_cipher_params_t* params);
void snow3g_destroy_context(snow3g_context_t* ctx);
void snow3g_generate_keystream(snow3g_context_t* ctx, uint32_t* keystream, size_t words);
void snow3g_lfsr_clock(snow3g_context_t* ctx);
void snow3g_fsm_next_state(snow3g_context_t* ctx);
uint32_t snow3g_fsm_function(snow3g_context_t* ctx);
uint8_t snow3g_s1_box(uint8_t input);
uint8_t snow3g_s2_box(uint8_t input);
uint32_t snow3g_mul_alpha(uint32_t x);
uint32_t snow3g_mul_alpha_inv(uint32_t x);

#endif // SNOW3G_H