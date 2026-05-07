/*
 * 5G UE Simulation Application
 * RLC (Radio Link Control) Layer Header
 */

#ifndef RLC_H
#define RLC_H

#include "pdcp.h"
#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

// RLC Constants
#define RLC_MAX_SDU_SIZE        8192
#define RLC_MAX_PDU_SIZE        8192
#define RLC_DEFAULT_WINDOW_SIZE 512
#define RLC_MAX_POLL_PDU        1024
#define RLC_MAX_POLL_BYTE       1024000

// RLC Modes
typedef enum {
    RLC_MODE_TM = 0,    // Transparent Mode
    RLC_MODE_UM = 1,    // Unacknowledged Mode
    RLC_MODE_AM = 2,    // Acknowledged Mode
    RLC_MODE_MAX
} rlc_mode_t;

// RLC Bearer Types
typedef enum {
    RLC_BEARER_SRB0 = 0,         // Signaling Radio Bearer 0
    RLC_BEARER_SRB1 = 1,         // Signaling Radio Bearer 1
    RLC_BEARER_SRB2 = 2,         // Signaling Radio Bearer 2
    RLC_BEARER_SRB3 = 3,         // Signaling Radio Bearer 3
    RLC_BEARER_DRB1 = 4,         // Data Radio Bearer 1
    RLC_BEARER_DRB2 = 5,         // Data Radio Bearer 2
    RLC_BEARER_DRB_MAX = 32      // Maximum DRB value
} rlc_bearer_t;

// RLC Direction
typedef enum {
    RLC_DIRECTION_UPLINK = 0,    // UE to gNB
    RLC_DIRECTION_DOWNLINK = 1,  // gNB to UE
    RLC_DIRECTION_BIDIRECTIONAL = 2  // Both directions (for SRBs)
} rlc_direction_t;

// RLC Status
typedef enum {
    RLC_STATUS_IDLE = 0,
    RLC_STATUS_CONFIGURED = 1,
    RLC_STATUS_ACTIVE = 2,
    RLC_STATUS_SUSPENDED = 3,
    RLC_STATUS_MAX
} rlc_status_t;

// RLC TM Config
typedef struct {
    uint16_t reserved;           // Reserved for future use
} rlc_tm_config_t;

// RLC UM Config
typedef struct {
    uint8_t sn_length;           // Sequence Number length (6 or 12 bits)
    uint16_t t_reassembly;       // Timer for reassembly (ms)
    bool enable_ciphering;       // Enable ciphering for UM
} rlc_um_config_t;

// RLC AM Config
typedef struct {
    uint8_t sn_length;           // Sequence Number length (12 or 18 bits)
    uint16_t t_poll_retransmit;  // Poll retransmit timer (ms)
    uint16_t t_reassembly;       // Reassembly timer (ms)
    uint16_t t_status_prohibit;  // Status prohibit timer (ms)
    uint32_t poll_pdu;           // Poll PDU counter
    uint32_t poll_byte;          // Poll byte counter
    uint16_t max_retx_threshold; // Maximum retransmission threshold
} rlc_am_config_t;

// RLC Configuration
typedef struct {
    rlc_mode_t mode;             // RLC mode
    union {
        rlc_tm_config_t tm;      // TM configuration
        rlc_um_config_t um;      // UM configuration
        rlc_am_config_t am;      // AM configuration
    } config;
} rlc_config_t;

// RLC SDU (Service Data Unit)
typedef struct rlc_sdu {
    uint8_t* data;               // SDU data
    size_t data_length;          // Length of SDU data
    uint16_t sdu_id;             // SDU identifier
    uint32_t creation_time;      // Creation time (for discard)
    struct rlc_sdu* next;        // Next SDU in queue
} rlc_sdu_t;

// RLC PDU (Protocol Data Unit)
typedef struct rlc_pdu {
    uint8_t* data;               // PDU data
    size_t data_length;          // Length of PDU data
    uint16_t sn;                 // Sequence Number
    uint8_t fi;                  // Framing Info
    uint16_t so;                 // Segment Offset
    bool is_segment;             // Is this a segment?
    struct rlc_pdu* next;        // Next PDU in queue
} rlc_pdu_t;

// RLC AM Transmit Window
typedef struct {
    rlc_pdu_t** tx_window;       // Transmit window array
    uint16_t window_size;        // Window size
    uint16_t vt_a;               // Acknowledgement pointer
    uint16_t vt_ms;              // Maximum send state
    uint16_t vt_s;               // Send state
    pthread_mutex_t window_mutex; // Window protection
} rlc_am_tx_window_t;

// RLC AM Receive Window
typedef struct {
    rlc_pdu_t** rx_window;       // Receive window array
    uint16_t window_size;        // Window size
    uint16_t vr_r;               // Receive state
    uint16_t vr_mr;              // Maximum receive state
    uint16_t vr_x;               // Expected retransmission
    uint16_t vr_ms;              // Maximum status
    uint16_t vr_h;               // Highest received
    pthread_mutex_t window_mutex; // Window protection
} rlc_am_rx_window_t;

// RLC AM Entity
typedef struct {
    rlc_am_tx_window_t tx_window; // Transmit window
    rlc_am_rx_window_t rx_window; // Receive window
    rlc_sdu_t* tx_buffer;        // Transmit SDU buffer
    rlc_pdu_t* rx_buffer;        // Receive PDU buffer (for reassembly)
    uint32_t poll_sn;            // Poll sequence number
    uint32_t poll_pdu_counter;   // Poll PDU counter
    uint32_t poll_byte_counter;  // Poll byte counter
    uint32_t total_bytes_sent;   // Total bytes sent
    pthread_mutex_t entity_mutex; // Entity protection
    pthread_cond_t entity_cond;   // Entity signaling
} rlc_am_entity_t;

// RLC UM Entity
typedef struct {
    rlc_pdu_t* rx_buffer;        // Receive buffer
    uint16_t vr_uh;              // Highest received in UM
    uint16_t vr_ur;              // Receive state in UM
    uint16_t vr_ux;              // Expected retransmission in UM
    pthread_mutex_t entity_mutex; // Entity protection
    pthread_cond_t entity_cond;   // Entity signaling
} rlc_um_entity_t;

// RLC TM Entity
typedef struct {
    rlc_sdu_t* tx_buffer;        // Transmit buffer
    rlc_sdu_t* rx_buffer;        // Receive buffer
    pthread_mutex_t entity_mutex; // Entity protection
    pthread_cond_t entity_cond;   // Entity signaling
} rlc_tm_entity_t;

// RLC Entity
typedef struct {
    uint32_t entity_id;          // Entity identifier
    rlc_bearer_t bearer_type;    // Bearer type
    rlc_direction_t direction;   // Direction
    rlc_mode_t mode;             // RLC mode
    rlc_status_t status;         // Entity status
    rlc_config_t config;         // Configuration
    union {
        rlc_tm_entity_t tm;      // TM entity
        rlc_um_entity_t um;      // UM entity
        rlc_am_entity_t am;      // AM entity
    } entity;
    atomic_uint sdu_counter;     // SDU counter
    atomic_uint pdu_counter;     // PDU counter
    bool active;                 // Entity active flag
    pthread_mutex_t entity_mutex; // Entity protection
    pthread_cond_t entity_cond;   // Entity signaling
} rlc_entity_t;

// RLC Statistics
typedef struct {
    uint64_t tx_pdus;            // Transmitted PDUs
    uint64_t tx_bytes;           // Transmitted bytes
    uint64_t rx_pdus;            // Received PDUs
    uint64_t rx_bytes;           // Received bytes
    uint64_t discarded_sdus;     // Discarded SDUs
    uint64_t retransmitted_pdus; // Retransmitted PDUs
    uint64_t errors;             // Errors
} rlc_stats_t;

// Function prototypes
uesim_error_t rlc_init(ue_context_t* ue_ctx);
void rlc_cleanup(ue_context_t* ue_ctx);

// RLC Entity Management
uesim_error_t rlc_create_entity(ue_context_t* ue_ctx, rlc_bearer_t bearer,
                               rlc_direction_t direction, rlc_mode_t mode,
                               const rlc_config_t* config, rlc_entity_t** entity);
uesim_error_t rlc_destroy_entity(ue_context_t* ue_ctx, rlc_entity_t* entity);
uesim_error_t rlc_configure_entity(rlc_entity_t* entity, const rlc_config_t* config);
uesim_error_t rlc_activate_entity(rlc_entity_t* entity);
uesim_error_t rlc_deactivate_entity(rlc_entity_t* entity);
uesim_error_t rlc_suspend_entity(rlc_entity_t* entity);
uesim_error_t rlc_resume_entity(rlc_entity_t* entity);

// RLC Data Processing
uesim_error_t rlc_process_tx_data(rlc_entity_t* entity, const void* sdu_data,
                                 size_t sdu_length, rlc_pdu_t** pdu_list);
uesim_error_t rlc_process_rx_data(rlc_entity_t* entity, const rlc_pdu_t* pdu_list,
                                 void** sdu_data, size_t* sdu_length);
uesim_error_t rlc_receive_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu);

// RLC PDU Management
uesim_error_t rlc_create_pdu(rlc_entity_t* entity, size_t data_length, rlc_pdu_t** pdu);
uesim_error_t rlc_destroy_pdu(rlc_entity_t* entity, rlc_pdu_t* pdu);
uesim_error_t rlc_segment_pdu(rlc_entity_t* entity, const rlc_pdu_t* pdu,
                             size_t segment_size, rlc_pdu_t** segment_list);
uesim_error_t rlc_reassemble_pdu(rlc_entity_t* entity, const rlc_pdu_t* segment_list,
                                rlc_pdu_t** reassembled_pdu);

// RLC SDU Management
uesim_error_t rlc_create_sdu(rlc_entity_t* entity, size_t data_length, rlc_sdu_t** sdu);
uesim_error_t rlc_destroy_sdu(rlc_entity_t* entity, rlc_sdu_t* sdu);
uesim_error_t rlc_queue_sdu(rlc_entity_t* entity, rlc_sdu_t* sdu);
uesim_error_t rlc_dequeue_sdu(rlc_entity_t* entity, rlc_sdu_t** sdu);

// RLC AM Specific Functions
uesim_error_t rlc_am_poll_entity(rlc_entity_t* entity);
uesim_error_t rlc_am_process_status_pdu(rlc_entity_t* entity, const uint8_t* status_data,
                                       size_t status_length);
uesim_error_t rlc_am_generate_status_pdu(rlc_entity_t* entity, uint8_t** status_data,
                                        size_t* status_length);

// RLC AM Window Management Functions
uesim_error_t rlc_am_init_tx_window(rlc_am_tx_window_t* win, uint16_t window_size);
uesim_error_t rlc_am_init_rx_window(rlc_am_rx_window_t* win, uint16_t window_size);
uesim_error_t rlc_am_destroy_tx_window(rlc_am_tx_window_t* win);
uesim_error_t rlc_am_destroy_rx_window(rlc_am_rx_window_t* win);
uesim_error_t rlc_am_tx_window_insert(rlc_am_tx_window_t* win, rlc_pdu_t* pdu, uint8_t sn_length);
uesim_error_t rlc_am_rx_window_insert(rlc_am_rx_window_t* win, rlc_pdu_t* pdu, uint8_t sn_length);
rlc_pdu_t* rlc_am_tx_window_get(rlc_am_tx_window_t* win, uint16_t sn);
rlc_pdu_t* rlc_am_rx_window_get(rlc_am_rx_window_t* win, uint16_t sn);

// RLC Timer Functions
uesim_error_t rlc_start_timer(rlc_entity_t* entity, uint16_t timer_id, uint32_t timeout_ms);
uesim_error_t rlc_stop_timer(rlc_entity_t* entity, uint16_t timer_id);
uesim_error_t rlc_handle_timer_expiry(rlc_entity_t* entity, uint16_t timer_id);

// RLC Utility Functions
uint16_t rlc_get_sequence_number(rlc_entity_t* entity);
uesim_error_t rlc_increment_sequence_number(rlc_entity_t* entity);
bool rlc_is_entity_active(rlc_entity_t* entity);
uesim_error_t rlc_get_entity_stats(rlc_entity_t* entity, rlc_stats_t* stats);

// RLC Configuration Functions
uesim_error_t rlc_set_tm_config(rlc_config_t* config);
uesim_error_t rlc_set_um_config(rlc_config_t* config, uint8_t sn_length, uint16_t t_reassembly);
uesim_error_t rlc_set_am_config(rlc_config_t* config, uint8_t sn_length, uint16_t t_poll_retransmit,
                               uint16_t t_reassembly, uint16_t t_status_prohibit,
                               uint32_t poll_pdu, uint32_t poll_byte, uint16_t max_retx_threshold);

#endif // RLC_H