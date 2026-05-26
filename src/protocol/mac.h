/*
 * 5G UE Simulation Application
 * MAC (Medium Access Control) Layer Header
 */

#ifndef MAC_H
#define MAC_H

#include "rlc.h"
#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>

// MAC Constants
#define MAC_MAX_HARQ_PROCESSES      16
#define MAC_MAX_LOGICAL_CHANNELS    32
#define MAC_MAX_TRANSPORT_BLOCKS    256
#define MAC_DEFAULT_TTI_MS          1
#define MAC_MAX_RACH_PREAMBLES      64
#define MAC_MAX_UL_GRANTS           32
#define MAC_MAX_DL_GRANTS           32

// MAC Logical Channels
typedef enum {
    MAC_LCH_BCCH = 0,           // Broadcast Control Channel
    MAC_LCH_PCCH = 1,           // Paging Control Channel
    MAC_LCH_CCCH = 2,           // Common Control Channel
    MAC_LCH_DCCH = 3,           // Dedicated Control Channel
    MAC_LCH_DTCH = 4,           // Dedicated Traffic Channel
    MAC_LCH_MAX
} mac_logical_channel_t;

// MAC Transport Channels
typedef enum {
    MAC_TCH_BCH = 0,            // Broadcast Channel
    MAC_TCH_PCH = 1,            // Paging Channel
    MAC_TCH_RACH = 2,           // Random Access Channel
    MAC_TCH_UL_SCH = 3,         // Uplink Shared Channel
    MAC_TCH_DL_SCH = 4,         // Downlink Shared Channel
    MAC_TCH_MAX
} mac_transport_channel_t;

// MAC HARQ Process States
typedef enum {
    MAC_HARQ_IDLE = 0,          // HARQ process idle
    MAC_HARQ_ACTIVE = 1,        // HARQ process active
    MAC_HARQ_PENDING = 2,       // HARQ process pending ACK/NACK
    MAC_HARQ_MAX
} mac_harq_state_t;

// MAC RNTI Types
typedef enum {
    MAC_RNTI_C_RNTI = 0,        // Cell RNTI
    MAC_RNTI_RA_RNTI = 1,       // Random Access RNTI
    MAC_RNTI_P_RNTI = 2,        // Paging RNTI
    MAC_RNTI_SI_RNTI = 3,       // System Information RNTI
    MAC_RNTI_SPS_RNTI = 4,      // Semi-Persistent Scheduling RNTI
    MAC_RNTI_MCS_C_RNTI = 5,    // Multi-Cell Scheduling C-RNTI
    MAC_RNTI_MAX
} mac_rnti_type_t;

// MAC Direction
typedef enum {
    MAC_DIRECTION_UPLINK = 0,   // UE to gNB
    MAC_DIRECTION_DOWNLINK = 1, // gNB to UE
    MAC_DIRECTION_MAX
} mac_direction_t;

// MAC Entity Status
typedef enum {
    MAC_STATUS_IDLE = 0,
    MAC_STATUS_CONFIGURED = 1,
    MAC_STATUS_ACTIVE = 2,
    MAC_STATUS_SUSPENDED = 3,
    MAC_STATUS_MAX
} mac_status_t;

// MAC RACH State
typedef enum {
    MAC_RACH_IDLE = 0,
    MAC_RACH_ACTIVE = 1,
    MAC_RACH_SUCCESS = 2,
    MAC_RACH_FAILED = 3,
    MAC_RACH_MAX
} mac_rach_state_t;

// MAC HARQ Process
typedef struct {
    uint8_t process_id;         // HARQ process ID
    mac_harq_state_t state;     // HARQ process state
    uint8_t* tb_data;           // Transport block data
    size_t tb_length;           // Transport block length
    uint16_t rvidx;             // Redundancy version index
    uint8_t ndi;                // New data indicator
    uint8_t harq_ack;           // HARQ ACK/NACK status
    uint32_t transmission_time; // Transmission time
    uint16_t tb_size;           // Transport block size
    uint8_t num_retransmissions; // Number of retransmissions
    uint8_t max_retransmissions; // Maximum retransmissions allowed
    pthread_mutex_t harq_mutex;  // HARQ process protection
} mac_harq_process_t;

// MAC Logical Channel Buffer Entry
typedef struct mac_lch_buffer_entry_t {
    uint8_t* data;                        // Data buffer
    size_t data_length;                   // Data length
    uint32_t arrival_time;                // Arrival timestamp
    struct mac_lch_buffer_entry_t* next;  // Linked list next pointer
} mac_lch_buffer_entry_t;

// MAC Logical Channel Buffer
typedef struct {
    mac_lch_buffer_entry_t* head;         // Queue head
    mac_lch_buffer_entry_t* tail;         // Queue tail
    uint32_t entry_count;                 // Number of entries
    uint32_t total_bytes;                // Total queued bytes
    uint32_t max_bytes;                  // Maximum allowed bytes
    pthread_mutex_t buffer_mutex;        // Buffer protection
} mac_lch_buffer_t;

// MAC Logical Channel Info
typedef struct {
    mac_logical_channel_t lch_id; // Logical channel ID
    uint8_t priority;             // Logical channel priority (1-16)
    uint16_t prioritized_bit_rate; // Prioritized bit rate (kbit/s)
    uint16_t bucket_size_duration; // Bucket size duration (ms)
    bool logical_channel_group;   // Logical channel group
    uint32_t served_bytes;        // Bytes served on this channel
    uint32_t queued_bytes;        // Bytes queued on this channel
} mac_lch_info_t;

// MAC Transport Block
typedef struct mac_tb_t {
    uint8_t* data;              // TB data
    size_t data_length;         // TB data length
    uint16_t tb_id;             // Transport block ID
    uint8_t harq_process_id;    // Associated HARQ process
    uint16_t rnti;              // RNTI for this TB
    uint8_t modulation;         // Modulation scheme
    uint16_t mcs;               // Modulation and Coding Scheme
    uint16_t rv;                // Redundancy Version
    uint8_t ndi;                // New Data Indicator
    uint32_t creation_time;     // Creation time
    bool is_retransmission;     // Is this a retransmission?
    mac_direction_t direction;  // Direction (UL/DL)
    struct mac_tb_t* next;     // Linked list next pointer
} mac_tb_t;

// MAC Control Element
typedef struct {
    uint8_t ce_type;            // Control element type
    uint8_t* ce_data;           // Control element data
    size_t ce_length;           // Control element length
    uint16_t rnti;              // RNTI associated with CE
} mac_ce_t;

// MAC Random Access Configuration
typedef struct {
    uint8_t preamble_index;     // Selected preamble index
    uint8_t prach_mask_index;   // PRACH mask index
    uint16_t ra_rnti;           // Random Access RNTI
    uint8_t msg1_freq_hopping;  // Msg1 frequency hopping
    uint8_t msg1_scs;           // Msg1 subcarrier spacing
    uint8_t msg1_cyclic_prefix; // Msg1 cyclic prefix
    uint16_t ra_response_window; // RA response window (slots)
    uint8_t preamble_trans_max; // Max preamble transmissions
    uint16_t power_ramping_step; // Power ramping step (0.1 dB units)
    uint16_t ra_contention_resolution_timer; // Contention resolution timer
    int16_t preamble_initial_rx_target_power; // Initial preamble power (dBm)
} mac_rach_config_t;

// MAC Scheduling Request Configuration
typedef struct {
    uint8_t sr_id;              // Scheduling Request ID
    uint8_t sr_prohibit_timer;  // SR prohibit timer
    uint8_t sr_trans_max;       // Max SR transmissions
    uint8_t sr_counter;         // Current SR transmission counter
    uint8_t pucch_resource;     // PUCCH resource index
    uint16_t period;            // SR period (slots)
    bool sr_pending;            // SR pending flag
    bool sr_prohibited;         // SR prohibited flag
} mac_sr_config_t;

// Buffer Status Report (BSR) Configuration
typedef enum {
    MAC_BSR_SINGLE = 0,         // Single entry BSR
    MAC_BSR_SHORT = 1,          // Short BSR
    MAC_BSR_LONG = 2,           // Long BSR
    MAC_BSR_TRUNCATED = 3       // Truncated BSR
} mac_bsr_type_t;

// Logical Channel Group
typedef struct {
    uint8_t lcg_id;             // LCG ID (0-7)
    uint32_t buffer_size;       // Current buffer size in bytes
    bool has_data;              // Has data to send
    bool periodic_bsr_pending;  // Periodic BSR pending
    bool regular_bsr_pending;   // Regular BSR pending
} mac_lcg_status_t;

// BSR Control Element
typedef struct {
    mac_bsr_type_t bsr_type;    // BSR type
    uint8_t lcg_id;             // LCG ID (for single/short)
    uint8_t lcg_num;            // Number of LCGs (for long)
    uint8_t buffer_size[8];     // Buffer size per LCG (indexed by LCG ID)
} mac_bsr_ce_t;

// Power Headroom Report (PHR) Configuration
typedef enum {
    MAC_PHR_SINGLE = 0,         // Single entry PHR
    MAC_PHR_MULTI = 1,          // Multiple entry PHR
    MAC_PHR_EXTENDED = 2        // Extended PHR
} mac_phr_type_t;

// PHR Entry
typedef struct {
    int8_t ph;                  // Power headroom value (dB)
    int8_t p_cmax;              // Max TX power (dBm)
    uint8_t serving_cell_id;    // Serving cell ID
    bool pcmax_set;             // P_CMAX is set
} mac_phr_entry_t;

// PHR Control Element
typedef struct {
    mac_phr_type_t phr_type;    // PHR type
    uint8_t num_entries;        // Number of PHR entries
    mac_phr_entry_t entries[8]; // PHR entries
    bool phr_triggered;         // PHR triggered flag
    uint8_t phr_prohibit_timer; // PHR prohibit timer
    uint8_t phr_periodic_timer; // PHR periodic timer
} mac_phr_ce_t;

// DRX (Discontinuous Reception) Configuration
typedef struct {
    uint16_t on_duration_timer;     // On duration timer (ms)
    uint16_t drx_inactivity_timer;  // DRX inactivity timer (ms)
    uint16_t drx_retransmission_timer; // DRX retransmission timer (ms)
    uint16_t long_drx_cycle;        // Long DRX cycle length (ms)
    uint16_t short_drx_cycle;       // Short DRX cycle length (ms)
    uint16_t drx_start_offset;      // DRX start offset (subframes)
    uint8_t short_cycle_count;      // Number of short cycles
    uint16_t drx_slot_offset;       // Slot offset for DRX
    bool drx_active;                // DRX is currently active
    bool on_duration;               // Currently in on-duration
    bool use_short_cycle;           // Using short DRX cycle
    uint16_t current_cycle;         // Current DRX cycle count
    uint32_t next_on_duration;      // Next on-duration time (ms)
    uint32_t last_activity_time;    // Last activity time (ms)
    bool harq_active[MAC_MAX_HARQ_PROCESSES]; // HARQ processes with pending data
} mac_drx_config_t;

// MAC Entity Configuration
typedef struct {
    uint16_t c_rnti;            // Cell RNTI
    uint8_t harq_processes;     // Number of HARQ processes
    uint16_t tti_length_ms;     // TTI length in ms
    uint16_t max_harq_retx;     // Max HARQ retransmissions
    mac_rach_config_t rach_config; // RACH configuration
    mac_sr_config_t sr_config;   // Scheduling request configuration
    mac_lch_info_t lch_config[MAC_MAX_LOGICAL_CHANNELS]; // Logical channel config
    uint8_t num_lch_configured;  // Number of configured logical channels
    mac_drx_config_t drx_config; // DRX configuration
    mac_phr_ce_t phr_config;     // PHR configuration
    uint16_t bsr_periodic_timer; // BSR periodic timer (ms)
    uint16_t bsr_retx_timer;     // BSR retransmission timer (ms)
} mac_config_t;

// MAC Uplink Grant
typedef struct {
    uint16_t rnti;              // RNTI for this grant
    uint16_t tb_size;           // Transport block size
    uint16_t mcs;               // Modulation and Coding Scheme
    uint8_t ndi;                // New Data Indicator
    uint8_t rv;                 // Redundancy Version
    uint16_t frequency_hopping; // Frequency hopping flag
    uint16_t hopping_id;        // Hopping ID
    uint32_t grant_time;        // Grant time
    bool valid;                 // Grant validity
} mac_ul_grant_t;

// MAC Downlink Grant
typedef struct {
    uint16_t rnti;              // RNTI for this grant
    uint16_t tb_size;           // Transport block size
    uint16_t mcs;               // Modulation and Coding Scheme
    uint8_t ndi;                // New Data Indicator
    uint8_t rv;                 // Redundancy Version
    uint32_t grant_time;        // Grant time
    bool valid;                 // Grant validity
} mac_dl_grant_t;

// MAC Statistics
typedef struct {
    uint64_t tx_tb_count;       // Transmitted TB count
    uint64_t rx_tb_count;       // Received TB count
    uint64_t tx_bytes;          // Transmitted bytes
    uint64_t rx_bytes;          // Received bytes
    uint64_t harq_retransmissions; // HARQ retransmissions
    uint64_t harq_failures;     // HARQ failures
    uint64_t rach_attempts;     // RACH attempts
    uint64_t rach_success;      // Successful RACH
    uint64_t scheduling_requests; // Scheduling requests
} mac_stats_t;

// MAC Entity
typedef struct {
    uint32_t entity_id;         // MAC entity ID
    mac_status_t status;        // MAC entity status
    mac_config_t config;        // MAC configuration
    mac_harq_process_t ul_harq[MAC_MAX_HARQ_PROCESSES]; // UL HARQ processes
    mac_harq_process_t dl_harq[MAC_MAX_HARQ_PROCESSES]; // DL HARQ processes
    mac_lch_buffer_t lch_buffers[MAC_LCH_MAX]; // Per-channel data buffers
    mac_tb_t* tb_queue;         // Transport block queue
    mac_ce_t* ce_queue;         // Control element queue
    mac_ul_grant_t ul_grants[MAC_MAX_UL_GRANTS]; // UL grants
    mac_dl_grant_t dl_grants[MAC_MAX_DL_GRANTS]; // DL grants
    mac_stats_t stats;          // MAC statistics
#ifdef _WIN32
    volatile LONG tb_counter;     // TB counter
    volatile LONG ce_counter;     // CE counter
#else
    atomic_uint tb_counter;     // TB counter
    atomic_uint ce_counter;     // CE counter
#endif
    bool active;                // MAC entity active
    pthread_mutex_t mac_mutex;  // MAC entity protection
    pthread_cond_t mac_cond;    // MAC entity signaling
    /* RACH state machine */
    mac_rach_state_t rach_state;       // RACH procedure state
    uint8_t rach_preamble_index;       // Current preamble index
    uint8_t rach_attempt;              // Preamble transmission attempt
    int16_t rach_power;                // Current preamble power (dBm)
    uint16_t rach_backoff;             // Backoff time (ms)
    uint16_t rach_contention_timer;    // Contention resolution timer
    uint32_t timing_advance;          // Timing advance (N_TA)
    uint32_t rach_ul_grant;           // UL grant from RAR
} mac_entity_t;

// Function prototypes
uesim_error_t mac_init(ue_context_t* ue_ctx);
void mac_cleanup(ue_context_t* ue_ctx);

// MAC Entity Management
uesim_error_t mac_create_entity(ue_context_t* ue_ctx, const mac_config_t* config, 
                               mac_entity_t** entity);
uesim_error_t mac_destroy_entity(ue_context_t* ue_ctx, mac_entity_t* entity);
uesim_error_t mac_configure_entity(mac_entity_t* entity, const mac_config_t* config);
uesim_error_t mac_activate_entity(mac_entity_t* entity);
uesim_error_t mac_deactivate_entity(mac_entity_t* entity);
uesim_error_t mac_suspend_entity(mac_entity_t* entity);
uesim_error_t mac_resume_entity(mac_entity_t* entity);

// MAC Data Processing
uesim_error_t mac_process_tx_data(mac_entity_t* entity, const void* data, 
                                 size_t data_length, mac_tb_t** tb);
uesim_error_t mac_process_rx_data(mac_entity_t* entity, const mac_tb_t* tb, 
                                 void** data, size_t* data_length);
uesim_error_t mac_transmit_tb(mac_entity_t* entity, mac_tb_t* tb);
uesim_error_t mac_receive_tb(mac_entity_t* entity, const mac_tb_t* tb);

// MAC HARQ Functions
uesim_error_t mac_harq_init_process(mac_harq_process_t* harq_process, uint8_t process_id);
uesim_error_t mac_harq_start_transmission(mac_harq_process_t* harq_process, 
                                         const uint8_t* data, size_t length);
uesim_error_t mac_harq_process_ack(mac_harq_process_t* harq_process, uint8_t ack);
uesim_error_t mac_harq_retransmit(mac_harq_process_t* harq_process);
uesim_error_t mac_harq_reset_process(mac_harq_process_t* harq_process);

// MAC Scheduling Functions
uesim_error_t mac_schedule_uplink(mac_entity_t* entity, mac_ul_grant_t* grant);
uesim_error_t mac_schedule_downlink(mac_entity_t* entity, mac_dl_grant_t* grant);
uesim_error_t mac_process_grant(mac_entity_t* entity, const mac_ul_grant_t* grant);
uesim_error_t mac_generate_ul_grant(mac_entity_t* entity, uint16_t tb_size, 
                                   mac_ul_grant_t* grant);

// MAC Logical Channel Buffer Functions
uesim_error_t mac_lch_buffer_init(mac_lch_buffer_t* buffer, uint32_t max_bytes);
void mac_lch_buffer_cleanup(mac_lch_buffer_t* buffer);
uesim_error_t mac_lch_buffer_enqueue(mac_lch_buffer_t* buffer, const void* data, size_t length);
uesim_error_t mac_lch_buffer_dequeue(mac_lch_buffer_t* buffer, void** data, size_t* length);
uesim_error_t mac_lch_buffer_peek(mac_lch_buffer_t* buffer, void** data, size_t* length);
uint32_t mac_lch_buffer_get_bytes(mac_lch_buffer_t* buffer);
uint32_t mac_lch_buffer_get_entries(mac_lch_buffer_t* buffer);
bool mac_lch_buffer_is_empty(mac_lch_buffer_t* buffer);

// MAC Logical Channel Functions
uesim_error_t mac_configure_logical_channel(mac_entity_t* entity, 
                                           const mac_lch_info_t* lch_info);
uesim_error_t mac_prioritize_logical_channels(mac_entity_t* entity);
uesim_error_t mac_queue_logical_channel_data(mac_entity_t* entity, 
                                            mac_logical_channel_t lch_id, 
                                            const void* data, size_t length);
uesim_error_t mac_dequeue_logical_channel_data(mac_entity_t* entity, 
                                              mac_logical_channel_t lch_id,
                                              void** data, size_t* length);
uesim_error_t mac_get_lch_buffer_status(mac_entity_t* entity, 
                                       mac_logical_channel_t lch_id,
                                       uint32_t* queued_bytes);
uesim_error_t mac_get_total_buffer_status(mac_entity_t* entity, uint32_t* total_bytes);

// MAC Control Element Functions
uesim_error_t mac_create_control_element(mac_entity_t* entity, uint8_t ce_type, 
                                        const void* ce_data, size_t ce_length, 
                                        mac_ce_t** ce);
uesim_error_t mac_process_control_element(mac_entity_t* entity, const mac_ce_t* ce);
uesim_error_t mac_destroy_control_element(mac_entity_t* entity, mac_ce_t* ce);

// MAC Random Access Functions
uesim_error_t mac_initiate_random_access(mac_entity_t* entity);
uesim_error_t mac_process_rach_request(mac_entity_t* entity, uint8_t preamble_index);
uesim_error_t mac_send_rach_preamble(mac_entity_t* entity, uint8_t preamble_index);
uesim_error_t mac_receive_rach_response(mac_entity_t* entity, const uint8_t* response_data, 
                                       size_t response_length);

// MAC Scheduling Request Functions
uesim_error_t mac_trigger_scheduling_request(mac_entity_t* entity);
uesim_error_t mac_process_scheduling_response(mac_entity_t* entity, 
                                             const uint8_t* response_data, 
                                             size_t response_length);

// MAC Transport Block Functions
uesim_error_t mac_create_transport_block(mac_entity_t* entity, size_t data_length, 
                                        mac_tb_t** tb);
uesim_error_t mac_destroy_transport_block(mac_entity_t* entity, mac_tb_t* tb);
uesim_error_t mac_queue_transport_block(mac_entity_t* entity, mac_tb_t* tb);
uesim_error_t mac_dequeue_transport_block(mac_entity_t* entity, mac_tb_t** tb);

// MAC Utility Functions
uint16_t mac_calculate_tb_size(mac_entity_t* entity, uint16_t mcs, uint16_t num_prbs);
uesim_error_t mac_update_statistics(mac_entity_t* entity);
bool mac_is_entity_active(mac_entity_t* entity);
uesim_error_t mac_get_entity_stats(mac_entity_t* entity, mac_stats_t* stats);

// MAC Configuration Functions
uesim_error_t mac_set_default_config(mac_config_t* config);
uesim_error_t mac_set_rach_config(mac_config_t* config, const mac_rach_config_t* rach_config);
uesim_error_t mac_set_sr_config(mac_config_t* config, const mac_sr_config_t* sr_config);
uesim_error_t mac_add_logical_channel(mac_config_t* config, const mac_lch_info_t* lch_info);

// MAC BSR (Buffer Status Report) Functions
uesim_error_t mac_bsr_config(mac_entity_t* entity, uint16_t periodic_timer, uint16_t retx_timer);
uesim_error_t mac_bsr_update_lcg(mac_entity_t* entity, uint8_t lcg_id, uint32_t buffer_size);
uesim_error_t mac_bsr_construct(mac_entity_t* entity, mac_bsr_type_t type, mac_bsr_ce_t* bsr);
uesim_error_t mac_bsr_trigger_regular(mac_entity_t* entity, uint8_t lcg_id);
uesim_error_t mac_bsr_trigger_periodic(mac_entity_t* entity);
uesim_error_t mac_bsr_cancel(mac_entity_t* entity);
uesim_error_t mac_bsr_get_highest_priority_lcg(mac_entity_t* entity, uint8_t* lcg_id);
uint8_t mac_bsr_buffer_size_to_index(uint32_t buffer_size);
uint32_t mac_bsr_index_to_buffer_size(uint8_t index);

// MAC PHR (Power Headroom Report) Functions
uesim_error_t mac_phr_config(mac_entity_t* entity, uint8_t periodic_timer, uint8_t prohibit_timer);
uesim_error_t mac_phr_trigger(mac_entity_t* entity);
uesim_error_t mac_phr_construct(mac_entity_t* entity, mac_phr_ce_t* phr);
uesim_error_t mac_phr_update_entry(mac_entity_t* entity, uint8_t cell_id, int8_t ph, int8_t p_cmax);
bool mac_phr_is_triggered(mac_entity_t* entity);

// MAC DRX (Discontinuous Reception) Functions
uesim_error_t mac_drx_config(mac_entity_t* entity, const mac_drx_config_t* config);
uesim_error_t mac_drx_start(mac_entity_t* entity);
uesim_error_t mac_drx_stop(mac_entity_t* entity);
uesim_error_t mac_drx_on_duration_start(mac_entity_t* entity);
uesim_error_t mac_drx_on_duration_end(mac_entity_t* entity);
uesim_error_t mac_drx_inactivity_timer_start(mac_entity_t* entity);
uesim_error_t mac_drx_inactivity_timer_stop(mac_entity_t* entity);
uesim_error_t mac_drx_retransmission_timer_start(mac_entity_t* entity, uint8_t harq_id);
uesim_error_t mac_drx_retransmission_timer_stop(mac_entity_t* entity, uint8_t harq_id);
bool mac_drx_is_active_time(mac_entity_t* entity);
bool mac_drx_is_on_duration(mac_entity_t* entity);
uesim_error_t mac_drx_update_timers(mac_entity_t* entity, uint32_t current_time_ms);

// MAC SR (Scheduling Request) Enhanced Functions
uesim_error_t mac_sr_config(mac_entity_t* entity, uint8_t sr_id, uint8_t pucch_resource,
                           uint16_t period, uint8_t trans_max);
uesim_error_t mac_sr_trigger(mac_entity_t* entity);
uesim_error_t mac_sr_cancel(mac_entity_t* entity);
uesim_error_t mac_sr_transmit(mac_entity_t* entity);
bool mac_sr_is_pending(mac_entity_t* entity);
bool mac_sr_is_prohibited(mac_entity_t* entity);

#endif // MAC_H
