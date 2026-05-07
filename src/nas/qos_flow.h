/*
 * 5G UE Simulation Application
 * QoS Flow Management Header
 */

#ifndef QOS_FLOW_H
#define QOS_FLOW_H

#include "../uesim.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* QoS constants */
#define QOS_MAX_FLOWS_PER_SESSION   8
#define QOS_MAX_PACKET_FILTERS      16
#define QOS_DEFAULT_5QI             9

/* QoS Flow State */
typedef enum {
    QOS_FLOW_STATE_INACTIVE = 0,
    QOS_FLOW_STATE_ACTIVE,
    QOS_FLOW_STATE_SUSPENDED,
    QOS_FLOW_STATE_MAX
} qos_flow_state_t;

/* Allocation and Retention Priority */
typedef struct {
    uint8_t priority_level;
    bool pre_emption_capability;
    bool pre_emption_vulnerability;
} arp_t;

/* Bit Rate */
typedef struct {
    uint64_t uplink;
    uint64_t downlink;
} bit_rate_t;

/* Packet Filter */
typedef struct {
    uint8_t direction;
    uint8_t protocol;
    uint32_t remote_addr;
    uint32_t remote_mask;
    uint16_t local_port_low;
    uint16_t local_port_high;
    uint16_t remote_port_low;
    uint16_t remote_port_high;
    bool match_all;
    uint8_t precedence;
} packet_filter_t;

/* QoS Flow Rule */
typedef struct {
    uint8_t rule_id;
    uint8_t qfi;
    uint8_t precedence;
    packet_filter_t filters[QOS_MAX_PACKET_FILTERS];
    uint8_t num_filters;
    bool default_rule;
} qos_flow_rule_t;

/* QoS Flow */
typedef struct {
    uint8_t qfi;
    uint8_t five_qi;
    qos_flow_state_t state;
    arp_t arp;
    bit_rate_t gbr;
    bit_rate_t mbr;
    bit_rate_t current_rate;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_dropped;
    uint8_t drb_id;
    qos_flow_rule_t* rule;
    time_t create_time;
    time_t last_activity;
    pthread_mutex_t flow_mutex;
} qos_flow_t;

/* QoS Flow Manager */
typedef struct {
    qos_flow_t flows[QOS_MAX_FLOWS_PER_SESSION];
    uint8_t num_flows;
    uint8_t default_qfi;
    uint8_t next_rule_id;
    bit_rate_t session_ambr;
    uint64_t session_bytes_sent;
    uint64_t session_bytes_received;
    pthread_mutex_t manager_mutex;
} qos_flow_manager_t;

/* Token Bucket */
typedef struct {
    uint64_t tokens;
    uint64_t max_tokens;
    uint64_t refill_rate;
    time_t last_refill;
} token_bucket_t;

/* Function prototypes */
uesim_error_t qos_flow_init(void);
void qos_flow_cleanup(void);

uesim_error_t qos_flow_create_manager(qos_flow_manager_t** manager);
uesim_error_t qos_flow_destroy_manager(qos_flow_manager_t* manager);

uesim_error_t qos_flow_create(qos_flow_manager_t* manager, uint8_t five_qi,
                              const arp_t* arp, const bit_rate_t* gbr,
                              const bit_rate_t* mbr, qos_flow_t** flow);
uesim_error_t qos_flow_release(qos_flow_manager_t* manager, uint8_t qfi);
uesim_error_t qos_flow_activate(qos_flow_manager_t* manager, uint8_t qfi);

qos_flow_t* qos_flow_find_by_qfi(qos_flow_manager_t* manager, uint8_t qfi);
qos_flow_t* qos_flow_find_default(qos_flow_manager_t* manager);

uesim_error_t qos_flow_set_session_ambr(qos_flow_manager_t* manager,
                                        uint64_t uplink_kbps, uint64_t downlink_kbps);
uesim_error_t qos_flow_update_session_stats(qos_flow_manager_t* manager,
                                            uint64_t bytes_sent, uint64_t bytes_received);

uesim_error_t qos_flow_bind_to_drb(qos_flow_t* flow, uint8_t drb_id);
uint8_t qos_flow_get_drb_id(qos_flow_t* flow);

const char* qos_flow_state_str(qos_flow_state_t state);
bool qos_flow_is_gbr(uint8_t five_qi);

#endif /* QOS_FLOW_H */