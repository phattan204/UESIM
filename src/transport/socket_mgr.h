/*
 * 5G UE Simulation Application
 * Socket management header
 */

#ifndef SOCKET_MGR_H
#define SOCKET_MGR_H

#include "../uesim.h"

// Socket manager functions
uesim_error_t socket_manager_init(void);
void socket_manager_cleanup(void);

// Socket creation functions
uesim_error_t create_ngap_socket(ue_context_t* ue_ctx);
uesim_error_t create_gtpu_socket(ue_context_t* ue_ctx);

// Message sending functions
uesim_error_t send_ngap_message(ue_context_t* ue_ctx, const void* data, size_t length);
uesim_error_t send_gtpu_packet(ue_context_t* ue_ctx, const void* data, size_t length);

#endif // SOCKET_MGR_H