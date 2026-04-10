/*
 * 5G UE Simulation Application
 * PDCP (Packet Data Convergence Protocol) Layer Header
 */

#ifndef PDCP_H
#define PDCP_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

// PDCP Constants
#define PDCP_MAX_SN_LENGTH      18
#define PDCP_MAX_HEADER_SIZE    3
#define PDCP_MAX_CIPHER_KEY_LEN 16
#define PDCP_MAX_INTEGRITY_KEY_LEN 16

// PDCP Ciphering Algorithms
typedef enum {
    PDCP_CIPHERING_ALG_NEA0 = 0,  // NULL algorithm
    PDCP_CIPHERING_ALG_NEA1 = 1,  // SNOW 3G
    PDCP_CIPHERING_ALG_NEA2 = 2,  // AES-128
    PDCP_CIPHERING_ALG_NEA3 = 3,  // ZUC
    PDCP_CIPHERING_ALG_MAX
} pdcp_ciphering_alg_t;

// PDCP Integrity Algorithms
typedef enum {
    PDCP_INTEGRITY_ALG_NIA0 = 0,  // NULL algorithm
    PDCP_INTEGRITY_ALG_NIA1 = 1,  // SNOW 3G
    PDCP_INTEGRITY_ALG_NIA2 = 2,  // AES-128
    PDCP_INTEGRITY_ALG_NIA3 = 3,  // ZUC
    PDCP_INTEGRITY_ALG_MAX
} pdcp_integrity_alg_t;

// PDCP Bearer Types
typedef enum {
    PDCP_BEARER_SRB0 = 0,         // Signaling Radio Bearer 0
    PDCP_BEARER_SRB1 = 1,         // Signaling Radio Bearer 1
    PDCP_BEARER_SRB2 = 2,         // Signaling Radio Bearer 2
    PDCP_BEARER_SRB3 = 3,         // Signaling Radio Bearer 3
    PDCP_BEARER_DRB1 = 4,         // Data Radio Bearer 1
    PDCP_BEARER_DRB2 = 5,         // Data Radio Bearer 2
    PDCP_BEARER_DRB_MAX = 32      // Maximum DRB value
} pdcp_bearer_t;

// PDCP Direction
typedef enum {
    PDCP_DIRECTION_UPLINK = 0,    // UE to gNB
    PDCP_DIRECTION_DOWNLINK = 1   // gNB to UE
} pdcp_direction_t;

// PDCP Ciphering Parameters
typedef struct {
    uint32_t count;               // 32-bit COUNT value
    uint8_t bearer;               // 5-bit bearer identity (0-31)
    uint8_t direction;            // 1-bit direction (0=UL, 1=DL)
} pdcp_cipher_params_t;

// PDCP Security Context
typedef struct {
    uint8_t ciphering_key[PDCP_MAX_CIPHER_KEY_LEN];
    uint8_t integrity_key[PDCP_MAX_INTEGRITY_KEY_LEN];
    pdcp_ciphering_alg_t ciphering_alg;
    pdcp_integrity_alg_t integrity_alg;
    atomic_uint key_refresh_count;
    pthread_mutex_t security_mutex;
} pdcp_security_context_t;

// PDCP Entity
typedef struct {
    uint32_t entity_id;
    pdcp_bearer_t bearer_type;
    pdcp_direction_t direction;
    uint16_t sn_length;           // Sequence Number length (7, 12, or 18 bits)
    atomic_uint next_pdcp_sn;     // Next PDCP SN to be assigned
    atomic_uint next_expected_sn; // Next expected SN for reception
    pdcp_security_context_t* security_ctx;
    pthread_mutex_t entity_mutex;
    pthread_cond_t entity_cond;
    bool active;
} pdcp_entity_t;

// PDCP PDU Structure
typedef struct {
    uint16_t sn;                  // Sequence Number
    uint8_t* data;                // PDCP SDU data
    size_t data_length;           // Length of SDU data
    uint32_t mac_i;               // Message Authentication Code
    bool ciphered;                // Ciphering applied
    bool integrity_protected;     // Integrity protection applied
} pdcp_pdu_t;

// Crypto Operations Interface
typedef struct {
    uesim_error_t (*encrypt)(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* plaintext, uint8_t* ciphertext, size_t length);
    uesim_error_t (*decrypt)(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* ciphertext, uint8_t* plaintext, size_t length);
    uesim_error_t (*compute_mac)(const uint8_t* key, const uint8_t* message,
                               size_t msg_len, const pdcp_cipher_params_t* params,
                               uint32_t* mac);
} pdcp_crypto_ops_t;

// Function prototypes
uesim_error_t pdcp_init(ue_context_t* ue_ctx);
void pdcp_cleanup(ue_context_t* ue_ctx);

// PDCP Entity Management
uesim_error_t pdcp_create_entity(ue_context_t* ue_ctx, pdcp_bearer_t bearer,
                                pdcp_direction_t direction, pdcp_entity_t** entity);
uesim_error_t pdcp_destroy_entity(ue_context_t* ue_ctx, pdcp_entity_t* entity);
uesim_error_t pdcp_activate_entity(pdcp_entity_t* entity);
uesim_error_t pdcp_deactivate_entity(pdcp_entity_t* entity);

// PDCP Data Processing
uesim_error_t pdcp_process_tx_data(pdcp_entity_t* entity, const void* sdu_data,
                                  size_t sdu_length, pdcp_pdu_t** pdu);
uesim_error_t pdcp_process_rx_data(pdcp_entity_t* entity, const pdcp_pdu_t* pdu,
                                  void** sdu_data, size_t* sdu_length);

// Security Context Management
uesim_error_t pdcp_create_security_context(pdcp_ciphering_alg_t cipher_alg,
                                          pdcp_integrity_alg_t integrity_alg,
                                          const uint8_t* cipher_key,
                                          const uint8_t* integrity_key,
                                          pdcp_security_context_t** security_ctx);
uesim_error_t pdcp_destroy_security_context(pdcp_security_context_t* security_ctx);
uesim_error_t pdcp_update_security_context(pdcp_security_context_t* security_ctx,
                                          pdcp_ciphering_alg_t new_cipher_alg,
                                          pdcp_integrity_alg_t new_integrity_alg);

// Ciphering Operations
uesim_error_t pdcp_encrypt_pdu(pdcp_entity_t* entity, pdcp_pdu_t* pdu);
uesim_error_t pdcp_decrypt_pdu(pdcp_entity_t* entity, pdcp_pdu_t* pdu);

// Integrity Protection
uesim_error_t pdcp_compute_integrity_protect(pdcp_entity_t* entity, pdcp_pdu_t* pdu);
uesim_error_t pdcp_verify_integrity_protect(pdcp_entity_t* entity, const pdcp_pdu_t* pdu,
                                           bool* valid);

// Algorithm-specific implementations
uesim_error_t snow3g_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* plaintext, uint8_t* ciphertext, size_t length);
uesim_error_t snow3g_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                            const uint8_t* ciphertext, uint8_t* plaintext, size_t length);
uesim_error_t snow3g_compute_mac(const uint8_t* key, const uint8_t* message,
                                size_t msg_len, const pdcp_cipher_params_t* params,
                                uint32_t* mac);

uesim_error_t aes_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* plaintext, uint8_t* ciphertext, size_t length);
uesim_error_t aes_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* ciphertext, uint8_t* plaintext, size_t length);
uesim_error_t aes_compute_mac(const uint8_t* key, const uint8_t* message,
                             size_t msg_len, const pdcp_cipher_params_t* params,
                             uint32_t* mac);

uesim_error_t zuc_encrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* plaintext, uint8_t* ciphertext, size_t length);
uesim_error_t zuc_decrypt(const uint8_t* key, const pdcp_cipher_params_t* params,
                         const uint8_t* ciphertext, uint8_t* plaintext, size_t length);
uesim_error_t zuc_compute_mac(const uint8_t* key, const uint8_t* message,
                             size_t msg_len, const pdcp_cipher_params_t* params,
                             uint32_t* mac);

// PDCP Header Operations
uesim_error_t pdcp_encode_header(const pdcp_pdu_t* pdu, uint8_t* header, size_t* header_len);
uesim_error_t pdcp_decode_header(const uint8_t* header, size_t header_len, pdcp_pdu_t* pdu);

// Utility Functions
uint32_t pdcp_get_count(pdcp_entity_t* entity);
uesim_error_t pdcp_increment_count(pdcp_entity_t* entity);

#endif // PDCP_H