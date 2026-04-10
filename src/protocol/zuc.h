/*
 * 5G UE Simulation Application
 * ZUC Algorithm Implementation Header
 */

#ifndef ZUC_H
#define ZUC_H

#include "pdcp.h"

// ZUC Constants
#define ZUC_KEY_SIZE 16         // 128 bits
#define ZUC_IV_SIZE 16          // 128 bits
#define ZUC_LFSR_SIZE 16        // 16 registers of 31 bits each
#define ZUC_FSM_SIZE 2          // 2 registers of 32 bits each
#define ZUC_WORD_SIZE 4         // 32 bits

// ZUC Context Structure
typedef struct {
    uint32_t lfsr[ZUC_LFSR_SIZE];   // Linear Feedback Shift Register (31-bit each)
    uint32_t fsm[ZUC_FSM_SIZE];     // Finite State Machine (32-bit each)
    uint32_t keystream_buffer[4];   // Keystream buffer
    size_t buffer_index;            // Current position in buffer
} zuc_context_t;

// ZUC S-boxes
extern const uint8_t zuc_S0[256];
extern const uint8_t zuc_S1[256];

// Function prototypes
uesim_error_t zuc_init_context(zuc_context_t* ctx, const uint8_t* key,
                              const pdcp_cipher_params_t* params);
void zuc_destroy_context(zuc_context_t* ctx);
void zuc_generate_keystream(zuc_context_t* ctx, uint32_t* keystream, size_t words);
void zuc_lfsr_clock(zuc_context_t* ctx);
void zuc_fsm_next_state(zuc_context_t* ctx);
uint32_t zuc_fsm_function(zuc_context_t* ctx, uint32_t u, uint32_t v);
uint8_t zuc_s0_box(uint8_t input);
uint8_t zuc_s1_box(uint8_t input);
uint32_t zuc_bit_reorganization(zuc_context_t* ctx);
uint32_t zuc_lfsr_initialization(uint32_t x);
uint32_t zuc_lfsr_feedback(uint32_t* lfsr);

#endif // ZUC_H