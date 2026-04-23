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
    uint16_t power_ramping_step; // Power ramping step (dB)
    uint16_t ra_contention_resolution_timer; // Contention resolution timer
} mac_rach_config_t;

// MAC Scheduling Request Configuration
typedef struct {
    uint8_t sr_id;              // Scheduling Request ID
    uint8_t sr_prohibit_timer;  // SR prohibit timer
    uint8_t sr_trans_max;       // Max SR transmissions
    bool sr_pending;            // SR pending flag
} mac_sr_config_t;

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
    mac_tb_t* tb_queue;         // Transport block queue
    mac_ce_t* ce_queue;         // Control element queue
    mac_ul_grant_t ul_grants[MAC_MAX_UL_GRANTS]; // UL grants
    mac_dl_grant_t dl_grants[MAC_MAX_DL_GRANTS]; // DL grants
    mac_stats_t stats;          // MAC statistics
    atomic_uint tb_counter;     // TB counter
    atomic_uint ce_counter;     // CE counter
    bool active;                // MAC entity active
    pthread_mutex_t mac_mutex;  // MAC entity protection
    pthread_cond_t mac_cond;    // MAC entity signaling
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

// MAC Logical Channel Functions
uesim_error_t mac_configure_logical_channel(mac_entity_t* entity, 
                                           const mac_lch_info_t* lch_info);
uesim_error_t mac_prioritize_logical_channels(mac_entity_t* entity);
uesim_error_t mac_queue_logical_channel_data(mac_entity_t* entity, 
                                            mac_logical_channel_t lch_id, 
                                            const void* data, size_t length);

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

#endif // MAC_H