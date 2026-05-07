-- 5G UE Simulation Wireshark Dissector
-- Lua dissector for capturing and analyzing UESim protocol traffic

-- Protocol definitions
local uesim_proto = Proto("UESim", "5G UE Simulation Protocol")

-- RRC Message Types
local rrc_msg_types = {
    [0] = "RRC Setup Request",
    [1] = "RRC Setup",
    [2] = "RRC Setup Complete",
    [3] = "RRC Reestablishment Request",
    [4] = "RRC Reconfiguration",
    [5] = "Handover Command"
}

-- NAS Message Types
local nas_msg_types = {
    [0x41] = "Registration Request",
    [0x42] = "Registration Accept",
    [0x43] = "Registration Complete",
    [0x44] = "Registration Reject",
    [0x45] = "Deregistration Request",
    [0x46] = "Deregistration Accept",
    [0x51] = "Service Request",
    [0x52] = "Service Accept",
    [0x53] = "Service Reject",
    [0x61] = "PDU Session Establishment Request",
    [0x62] = "PDU Session Establishment Accept",
    [0x63] = "PDU Session Establishment Reject",
    [0x65] = "PDU Session Release Request",
    [0x66] = "PDU Session Release Command",
    [0x67] = "PDU Session Release Complete"
}

-- QoS 5QI Types
local five_qi_types = {
    [1] = "Conversational Voice (GBR)",
    [2] = "Conversational Video (GBR)",
    [3] = "Conversational Voice Background (GBR)",
    [4] = "Conversational Video Background (GBR)",
    [5] = "IMS Signaling (Non-GBR)",
    [6] = "Video Buffered Streaming (Non-GBR)",
    [7] = "Voice Buffered Streaming (Non-GBR)",
    [8] = "Video Live Streaming (Non-GBR)",
    [9] = "Internet Default (Non-GBR)",
    [65] = "Mission Critical Voice (GBR)",
    [66] = "Mission Critical Video (GBR)",
    [67] = "Mission Critical Data (GBR)",
    [69] = "Mission Critical Data (Non-GBR)",
    [70] = "Mission Critical Video (Non-GBR)"
}

-- gNB Types
local gnb_types = {
    [0] = "OAI",
    [1] = "srsRAN",
    [2] = "Commercial",
    [3] = "Mock"
}

-- gNB States
local gnb_states = {
    [0] = "Unknown",
    [1] = "Connected",
    [2] = "Disconnected",
    [3] = "Handover Candidate",
    [4] = "Connecting"
}

-- Protocol fields
local fields = uesim_proto.fields

-- Header fields
fields.msg_type = ProtoField.uint8("uesim.msg_type", "Message Type", base.DEC, rrc_msg_types)
fields.msg_len = ProtoField.uint16("uesim.msg_len", "Message Length", base.DEC)
fields.ue_id = ProtoField.uint32("uesim.ue_id", "UE ID", base.DEC)
fields.transaction_id = ProtoField.uint8("uesim.transaction_id", "Transaction ID", base.DEC)

-- RRC fields
fields.rrc_establishment_cause = ProtoField.uint8("uesim.rrc.cause", "Establishment Cause", base.DEC)
fields.rrc_ue_identity_type = ProtoField.uint8("uesim.rrc.identity_type", "UE Identity Type", base.DEC)
fields.rrc_ue_identity = ProtoField.uint64("uesim.rrc.identity", "UE Identity", base.HEX)
fields.rrc_transaction_id = ProtoField.uint8("uesim.rrc.transaction_id", "RRC Transaction ID", base.DEC)
fields.rrc_selected_plmn = ProtoField.uint8("uesim.rrc.selected_plmn", "Selected PLMN", base.DEC)
fields.rrc_nas_pdu = ProtoField.bytes("uesim.rrc.nas_pdu", "NAS PDU")

-- gNB fields
fields.gnb_id = ProtoField.uint32("uesim.gnb.id", "gNB ID", base.DEC)
fields.gnb_type = ProtoField.uint8("uesim.gnb.type", "gNB Type", base.DEC, gnb_types)
fields.gnb_state = ProtoField.uint8("uesim.gnb.state", "gNB State", base.DEC, gnb_states)
fields.gnb_rsrp = ProtoField.int32("uesim.gnb.rsrp", "RSRP (dBm)", base.DEC)
fields.gnb_rsrq = ProtoField.int32("uesim.gnb.rsrq", "RSRQ (dB)", base.DEC)
fields.gnb_cell_id = ProtoField.uint16("uesim.gnb.cell_id", "Cell ID", base.DEC)
fields.gnb_tac = ProtoField.uint16("uesim.gnb.tac", "TAC", base.DEC)

-- QoS fields
fields.qos_qfi = ProtoField.uint8("uesim.qos.qfi", "QFI", base.DEC)
fields.qos_5qi = ProtoField.uint8("uesim.qos.5qi", "5QI", base.DEC, five_qi_types)
fields.qos_arp_priority = ProtoField.uint8("uesim.qos.arp_priority", "ARP Priority", base.DEC)
fields.qos_gbr_ul = ProtoField.uint64("uesim.qos.gbr_ul", "GBR Uplink (kbps)", base.DEC)
fields.qos_gbr_dl = ProtoField.uint64("uesim.qos.gbr_dl", "GBR Downlink (kbps)", base.DEC)
fields.qos_mbr_ul = ProtoField.uint64("uesim.qos.mbr_ul", "MBR Uplink (kbps)", base.DEC)
fields.qos_mbr_dl = ProtoField.uint64("uesim.qos.mbr_dl", "MBR Downlink (kbps)", base.DEC)
fields.qos_ambr_ul = ProtoField.uint64("uesim.qos.ambr_ul", "Session AMBR Uplink (kbps)", base.DEC)
fields.qos_ambr_dl = ProtoField.uint64("uesim.qos.ambr_dl", "Session AMBR Downlink (kbps)", base.DEC)

-- Load Test fields
fields.load_test_scenario = ProtoField.string("uesim.load_test.scenario", "Scenario")
fields.load_test_num_ues = ProtoField.uint32("uesim.load_test.num_ues", "Number of UEs", base.DEC)
fields.load_test_duration = ProtoField.uint64("uesim.load_test.duration", "Duration (s)", base.DEC)
fields.load_test_latency_min = ProtoField.uint64("uesim.load_test.latency_min", "Min Latency (us)", base.DEC)
fields.load_test_latency_max = ProtoField.uint64("uesim.load_test.latency_max", "Max Latency (us)", base.DEC)
fields.load_test_latency_mean = ProtoField.uint64("uesim.load_test.latency_mean", "Mean Latency (us)", base.DEC)
fields.load_test_latency_p50 = ProtoField.uint64("uesim.load_test.latency_p50", "P50 Latency (us)", base.DEC)
fields.load_test_latency_p95 = ProtoField.uint64("uesim.load_test.latency_p95", "P95 Latency (us)", base.DEC)
fields.load_test_latency_p99 = ProtoField.uint64("uesim.load_test.latency_p99", "P99 Latency (us)", base.DEC)
fields.load_test_throughput = ProtoField.double("uesim.load_test.throughput", "Throughput (proc/s)", base.DEC)
fields.load_test_failure_rate = ProtoField.double("uesim.load_test.failure_rate", "Failure Rate (%)", base.DEC)

-- NAS fields
fields.nas_msg_type = ProtoField.uint8("uesim.nas.msg_type", "NAS Message Type", base.HEX, nas_msg_types)
fields.nas_security_header = ProtoField.uint8("uesim.nas.security_header", "Security Header", base.HEX)
fields.nas_sequence = ProtoField.uint8("uesim.nas.sequence", "Sequence Number", base.DEC)

-- PDU fields
fields.pdu_session_id = ProtoField.uint8("uesim.pdu.session_id", "PDU Session ID", base.DEC)
fields.pdu_ssc_mode = ProtoField.uint8("uesim.pdu.ssc_mode", "SSC Mode", base.DEC)
fields.pdu_session_type = ProtoField.uint8("uesim.pdu.session_type", "Session Type", base.DEC)

-- Dissector function
function uesim_proto.dissector(buffer, pinfo, tree)
    local buffer_len = buffer:len()
    if buffer_len < 4 then return end
    
    pinfo.cols.protocol = "UESim"
    
    local subtree = tree:add(uesim_proto, buffer(), "5G UE Simulation Protocol Data")
    
    -- Parse header
    local msg_type = buffer(0, 1):uint()
    local msg_len = buffer(1, 2):uint()
    local ue_id = buffer(3, 4):uint()
    
    subtree:add(fields.msg_type, buffer(0, 1))
    subtree:add(fields.msg_len, buffer(1, 2))
    subtree:add(fields.ue_id, buffer(3, 4))
    
    -- Parse message-specific content
    if buffer_len > 7 then
        local payload = buffer(7)
        local payload_tree = subtree:add(buffer(7), "Payload")
        
        -- RRC messages
        if msg_type == 0 then -- RRC Setup Request
            payload_tree:add(fields.rrc_establishment_cause, payload(0, 1))
            payload_tree:add(fields.rrc_ue_identity_type, payload(1, 1))
            payload_tree:add(fields.rrc_ue_identity, payload(2, 8))
            
        elseif msg_type == 1 then -- RRC Setup
            payload_tree:add(fields.rrc_transaction_id, payload(0, 1))
            
        elseif msg_type == 2 then -- RRC Setup Complete
            payload_tree:add(fields.rrc_transaction_id, payload(0, 1))
            payload_tree:add(fields.rrc_selected_plmn, payload(1, 1))
            if payload:len() > 2 then
                payload_tree:add(fields.rrc_nas_pdu, payload(2))
            end
            
        elseif msg_type == 4 then -- RRC Reconfiguration
            payload_tree:add(fields.rrc_transaction_id, payload(0, 1))
            if payload:len() > 4 then
                -- QoS information
                local qos_tree = payload_tree:add(payload(4), "QoS Configuration")
                qos_tree:add(fields.qos_qfi, payload(4, 1))
                qos_tree:add(fields.qos_5qi, payload(5, 1))
                qos_tree:add(fields.qos_arp_priority, payload(6, 1))
                qos_tree:add(fields.qos_gbr_ul, payload(7, 4))
                qos_tree:add(fields.qos_gbr_dl, payload(11, 4))
            end
            
        elseif msg_type == 5 then -- Handover Command
            payload_tree:add(fields.gnb_id, payload(0, 4))
            payload_tree:add(fields.gnb_type, payload(4, 1))
            payload_tree:add(fields.gnb_cell_id, payload(5, 2))
            payload_tree:add(fields.gnb_tac, payload(7, 2))
            payload_tree:add(fields.gnb_rsrp, payload(9, 4))
            payload_tree:add(fields.gnb_rsrq, payload(13, 4))
        end
        
        -- NAS parsing if present
        if msg_type == 2 and payload:len() > 4 then
            local nas_tree = payload_tree:add(payload(2), "NAS Message")
            local nas_type = payload(4):uint()
            nas_tree:add(fields.nas_msg_type, payload(4, 1))
            nas_tree:add(fields.nas_security_header, payload(5, 1))
            nas_tree:add(fields.nas_sequence, payload(6, 1))
            
            -- PDU Session info
            if nas_type == 0x61 or nas_type == 0x62 then
                nas_tree:add(fields.pdu_session_id, payload(7, 1))
                nas_tree:add(fields.pdu_ssc_mode, payload(8, 1))
                nas_tree:add(fields.pdu_session_type, payload(9, 1))
            end
        end
    end
    
    -- Update info column
    pinfo.cols.info = string.format("UESim %s (UE=%d, Len=%d)",
        rrc_msg_types[msg_type] or "Unknown", ue_id, msg_len)
end

-- Register dissector
local udp_port = DissectorTable.get("udp.port")
local tcp_port = DissectorTable.get("tcp.port")

-- Register for common ports
udp_port:add(38412, uesim_proto)  -- NGAP
udp_port:add(2152, uesim_proto)   -- GTP-U
tcp_port:add(38412, uesim_proto)
tcp_port:add(5000, uesim_proto)   -- UESim default

-- Print loading message
print("UESim Wireshark Dissector loaded successfully")
print("  - UDP ports: 38412 (NGAP), 2152 (GTP-U), 5000 (UESim)")
print("  - TCP ports: 38412, 5000")

-- Utility functions for filtering

-- Filter for RRC messages only
function uesim_filter_rrc()
    return "uesim.msg_type >= 0 and uesim.msg_type <= 5"
end

-- Filter for specific UE
function uesim_filter_ue(ue_id)
    return string.format("uesim.ue_id == %d", ue_id)
end

-- Filter for QoS flows
function uesim_filter_qos()
    return "uesim.qos.qfi > 0"
end

-- Filter for handover
function uesim_filter_handover()
    return "uesim.msg_type == 5 or uesim.gnb.state == 3"
end

-- Filter for failed procedures
function uesim_filter_failures()
    return "uesim.load_test.failure_rate > 0"
end

-- Filter for high latency
function uesim_filter_high_latency(threshold_us)
    return string.format("uesim.load_test.latency_p99 > %d", threshold_us)
end