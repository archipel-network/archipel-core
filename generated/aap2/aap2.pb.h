/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.8-dev */

#ifndef PB_AAP2_AAP2_AAP2_PB_H_INCLUDED
#define PB_AAP2_AAP2_AAP2_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
/* The type of actions requested to be enabled for an AAP 2.0 connection. */
typedef enum _aap2_AuthType {
    /* Only allow to register free endpoints or agent IDs for which the same
 secret can be provided. */
    aap2_AuthType_AUTH_TYPE_DEFAULT = 0,
    /* Allow for changing the FIB (non-sub) or receiving FIB updates (sub). */
    aap2_AuthType_AUTH_TYPE_FIB_CONTROL = 1,
    /* Allow for dispatching bundles (sub) or sending bundles from arbitrary
 source endpoints (non-sub). */
    aap2_AuthType_AUTH_TYPE_BUNDLE_DISPATCH = 2,
    /* Allow for receiving FIB updates and dispatching bundles. */
    aap2_AuthType_AUTH_TYPE_FIB_AND_DISPATCH = 3
} aap2_AuthType;

/* Flags defining specific behavior for the bundle ADU, e.g., for BIBE. */
typedef enum _aap2_BundleADUFlags {
    /* No flags set - normal transmission request. */
    aap2_BundleADUFlags_BUNDLE_ADU_NORMAL = 0,
    /* The bundle ADU is a BIBE protocol data unit - request BIBE transmission. */
    aap2_BundleADUFlags_BUNDLE_ADU_BPDU = 1
} aap2_BundleADUFlags;

/* The reason why a DispatchRequest was sent. */
typedef enum _aap2_DispatchReason {
    /* Invalid. */
    aap2_DispatchReason_DISPATCH_REASON_UNSPECIFIED = 0,
    /* Destination EID was not found in FIB. */
    aap2_DispatchReason_DISPATCH_REASON_NO_FIB_ENTRY = 1,
    /* The link that should be used is currently not active or unusable. */
    aap2_DispatchReason_DISPATCH_REASON_LINK_INACTIVE = 2,
    /* The CLA subsystem responded negatively to the next-hop request. */
    aap2_DispatchReason_DISPATCH_REASON_CLA_LOOKUP_FAILED = 3,
    /* The transmission was attempted but failed. */
    aap2_DispatchReason_DISPATCH_REASON_TX_FAILED = 4
} aap2_DispatchReason;

/* The intended or currently-recorded status of a Link in the FIB. */
typedef enum _aap2_LinkStatus {
    /* Do not change the status. Invalid in a response/information. */
    aap2_LinkStatus_LINK_STATUS_UNSPECIFIED = 0,
    /* The link has been requested (e.g., via AAP) but the CLA has not yet
 confirmed a connection. */
    aap2_LinkStatus_LINK_STATUS_PENDING = 1,
    /* The link is present and available for forwarding bundles. */
    aap2_LinkStatus_LINK_STATUS_ACTIVE = 2,
    /* The link has been detected by a CLA but insufficient information exists
 to use the associated FIB entry. */
    aap2_LinkStatus_LINK_STATUS_OPPORTUNISTIC = 3,
    /* Deletion of the link is requested. */
    aap2_LinkStatus_LINK_STATUS_TEARDOWN = 4
} aap2_LinkStatus;

/* Definition of the status codes for an AAPResponse to be associated with. */
typedef enum _aap2_ResponseStatus {
    /* Invalid. */
    aap2_ResponseStatus_RESPONSE_STATUS_UNSPECIFIED = 0,
    /* Success. */
    aap2_ResponseStatus_RESPONSE_STATUS_SUCCESS = 1,
    /* Data received (neither success nor failure can be indicated). */
    aap2_ResponseStatus_RESPONSE_STATUS_ACK = 2,
    /* Failure status: all values >= 8.
 An unspecified failure. */
    aap2_ResponseStatus_RESPONSE_STATUS_ERROR = 8,
    /* A timeout occurred when performing the requested action. */
    aap2_ResponseStatus_RESPONSE_STATUS_TIMEOUT = 9,
    /* The received request is considered invalid and, thus, is not processed. */
    aap2_ResponseStatus_RESPONSE_STATUS_INVALID_REQUEST = 10,
    /* A resource required for processing the request was not found. */
    aap2_ResponseStatus_RESPONSE_STATUS_NOT_FOUND = 11,
    /* The client is not authorized to perform the requested action or a provided
 credential value is not valid for the requested authorization. */
    aap2_ResponseStatus_RESPONSE_STATUS_UNAUTHORIZED = 12
} aap2_ResponseStatus;

/* Struct definitions */
/* Message sent by the Server at the start of a connection to
 communicate basic parameters. */
typedef struct _aap2_Welcome {
    /* The local node ID, i.e., the (primary) EID identifying the local node.
 This may become a repeated element in the future.
 This value shall be used for deriving the (requested) application EIDs. */
    char *node_id;
} aap2_Welcome;

/* Message sent by the Client to authenticate itself and configure the
 connection. */
typedef struct _aap2_ConnectionConfig {
    /* If `true`, the direction of control is switched after positive
 confirmation, i.e., the server becomes the initiator of all following
 communication and also takes over sending the Keepalive messages. */
    bool is_subscriber;
    /* Controls the actions allowed and data provided over the connection. */
    aap2_AuthType auth_type;
    /* A secret to authorize the configuration. Required for already-registered
 endpoints and if `auth_type != 0`. When creating a new registration, this
 field is optional, but only if it is set, the same registration can be
 claimed by other connections (that need to specify the same secret).
 If `auth_type != 0`, the secret must be equal to the configured
 pre-shared key for allowing the requested access. */
    char *secret;
    /* The endpoint to be registered for the app. This is required to be a full
 EID as the local node may have multiple node IDs that can be used for
 registering applications. Optional when `auth_type != 0` and the app.
 only intends to control the FIB or dispatch bundles. */
    char *endpoint_id;
    /* The maximum time interval between messages, should be zero, or between
 30 sec and 600 sec. The Initiator SHALL send `KEEPALIVE` messages after
 this amount of time has elapsed. 0 disables the feature (see RFC 9174),
 which is useful e.g. for local sockets. Default = 60 seconds. */
    uint32_t keepalive_seconds;
} aap2_ConnectionConfig;

/* Message transmitting a bundle ADU into either direction. */
typedef struct _aap2_BundleADU {
    /* The bundle source EID. Optional when sending the bundle from the only
 registered endpoint associated with the connection.
 NOTE: FUTURE EXTENSION: Specifies the used local endpoint if multiple
 endpoints can be registered (and have been registered). */
    char *src_eid;
    /* The bundle destination EID. */
    char *dst_eid;
    /* The bundle creation time in milliseconds since the DTN epoch as defined
 in RFC 9171. Optional when sending bundles (will be assigned by µD3TN). */
    uint64_t creation_timestamp_ms;
    /* The bundle sequence number as defined in RFC 9171.
 Optional when sending bundles (will be assigned by µD3TN). */
    uint64_t sequence_number;
    /* The number of bytes contained in the bundle payload, which MUST be
 enclosed immediately _after_ the Protobuf message. */
    uint64_t payload_length;
    /* Flags defining specific behavior for the bundle ADU, e.g., for BIBE. */
    aap2_BundleADUFlags adu_flags;
} aap2_BundleADU;

/* Message informing a BDM Client about a received or newly-created
 bundle. */
typedef struct _aap2_Bundle {
    /* The bundle source EID. */
    char *src_eid;
    /* The bundle destination EID. */
    char *dst_eid;
    /* The bundle creation time in milliseconds since the DTN epoch as defined
 in RFC 9171. */
    uint64_t creation_timestamp_ms;
    /* The bundle sequence number as defined in RFC 9171. */
    uint64_t sequence_number;
    /* The payload length as defined in RFC 9171. */
    uint64_t payload_length;
    /* The fragment offset as defined in RFC 9171.
 Only set if the bundle is a fragment. */
    uint64_t fragment_offset;
    /* The total ADU length as defined in RFC 9171.
 Only set if the bundle is a fragment. */
    uint64_t total_adu_length;
    /* The bundle lifetime as defined in RFC 9171. */
    uint64_t lifetime_ms;
} aap2_Bundle;

/* Message informing a BDM Client about a bundle that needs to be dispatched. */
typedef struct _aap2_DispatchRequest {
    /* The bundle to dispatch. */
    bool has_bundle;
    aap2_Bundle bundle;
    /* Specifies why the BDM was triggered this time. */
    aap2_DispatchReason reason;
} aap2_DispatchRequest;

/* Message for initiating or informing about CLA link status changes. */
typedef struct _aap2_Link {
    /* The intended or detected link status. */
    aap2_LinkStatus status;
    /* An EID representing the next-hop node ID. */
    char *peer_node_id;
    /* The identification of a CLA plus CLA-specific address to reach the
 next-hop bundle node. */
    char *peer_cla_addr;
} aap2_Link;

/* A message that should be regularly sent by the current initiator and must be
 acknowledged with an `AAPResponse` specifying the result `RPC_RESULT_ACK`. */
typedef struct _aap2_Keepalive {
    char dummy_field;
} aap2_Keepalive;

/* The outer AAP 2.0 message type that is always sent by the initiator (the
 client if configured with `is_subscriber == false`, otherwise the server). */
typedef struct _aap2_AAPMessage {
    pb_size_t which_msg;
    union {
        /* Message sent by the Server at the start of a connection to
     communicate basic parameters. */
        aap2_Welcome welcome;
        /* Message sent by the Client to authenticate itself and configure the
     connection. */
        aap2_ConnectionConfig config;
        /* Message transmitting a bundle ADU into either direction. */
        aap2_BundleADU adu;
        /* Message informing a BDM Client about a bundle that needs to be
     dispatched. */
        aap2_DispatchRequest dispatch_request;
        /* Message for initiating or informing about CLA link status changes.
     This message is also issued multiple times by µD3TN at the start of a
     connection to a FIB-authorized subscriber to transmit the current FIB. */
        aap2_Link link;
        /* Call issued regularly to ensure the connection is still alive. */
        aap2_Keepalive keepalive;
    } msg;
} aap2_AAPMessage;

/* Representation of a result of a Bundle Dispatcher Module (BDM). */
typedef struct _aap2_DispatchResult {
    /* The next hop node for the bundle, which must be directly connected.
 If more than one next hop is specified, the bundle will be replicated
 among all of those nodes.
 If empty, the bundle will be dropped as the BPA is not expected to have
 any storage by itself (see the storage concepts discussion below).
 This way, we also do not need an "action list" as described for the
 "Generic Bundle Forwarding Interface" paper previously, because all
 actions can either be represented as forwarding to a specific set of
 nodes or _not_ forwarding, i.e., the deletion of the bundle.
 Note that proactive fragmentation is expected to be a feature of the
 storage for now. Later, maximum link capacities may be added (see above). */
    pb_size_t next_hops_count;
    struct _aap2_DispatchResult_NextHopEntry *next_hops;
} aap2_DispatchResult;

/* The response to every AAPMessage, sent by the peer that received the message. */
typedef struct _aap2_AAPResponse {
    /* The result of the received call/request represented as a single value. */
    aap2_ResponseStatus response_status;
    /* Set the next hops for a bundle. Only valid in response to a Bundle
 message sent to a BDM by the server (µD3TN). */
    bool has_dispatch_result;
    aap2_DispatchResult dispatch_result;
    /* Headers of the created bundle (present when sending a bundle). */
    bool has_bundle_headers;
    aap2_Bundle bundle_headers;
} aap2_AAPResponse;

/* Representation of an entry in the list of next hops of a DispatchResult. */
typedef struct _aap2_DispatchResult_NextHopEntry {
    /* The next-hop node ID for the bundle. May be an ID such as "ud3tn:storage"
 that resolve to a special CLA. */
    char *node_id;
} aap2_DispatchResult_NextHopEntry;


#ifdef __cplusplus
extern "C" {
#endif

/* Helper constants for enums */
#define _aap2_AuthType_MIN aap2_AuthType_AUTH_TYPE_DEFAULT
#define _aap2_AuthType_MAX aap2_AuthType_AUTH_TYPE_FIB_AND_DISPATCH
#define _aap2_AuthType_ARRAYSIZE ((aap2_AuthType)(aap2_AuthType_AUTH_TYPE_FIB_AND_DISPATCH+1))

#define _aap2_BundleADUFlags_MIN aap2_BundleADUFlags_BUNDLE_ADU_NORMAL
#define _aap2_BundleADUFlags_MAX aap2_BundleADUFlags_BUNDLE_ADU_BPDU
#define _aap2_BundleADUFlags_ARRAYSIZE ((aap2_BundleADUFlags)(aap2_BundleADUFlags_BUNDLE_ADU_BPDU+1))

#define _aap2_DispatchReason_MIN aap2_DispatchReason_DISPATCH_REASON_UNSPECIFIED
#define _aap2_DispatchReason_MAX aap2_DispatchReason_DISPATCH_REASON_TX_FAILED
#define _aap2_DispatchReason_ARRAYSIZE ((aap2_DispatchReason)(aap2_DispatchReason_DISPATCH_REASON_TX_FAILED+1))

#define _aap2_LinkStatus_MIN aap2_LinkStatus_LINK_STATUS_UNSPECIFIED
#define _aap2_LinkStatus_MAX aap2_LinkStatus_LINK_STATUS_TEARDOWN
#define _aap2_LinkStatus_ARRAYSIZE ((aap2_LinkStatus)(aap2_LinkStatus_LINK_STATUS_TEARDOWN+1))

#define _aap2_ResponseStatus_MIN aap2_ResponseStatus_RESPONSE_STATUS_UNSPECIFIED
#define _aap2_ResponseStatus_MAX aap2_ResponseStatus_RESPONSE_STATUS_UNAUTHORIZED
#define _aap2_ResponseStatus_ARRAYSIZE ((aap2_ResponseStatus)(aap2_ResponseStatus_RESPONSE_STATUS_UNAUTHORIZED+1))



#define aap2_ConnectionConfig_auth_type_ENUMTYPE aap2_AuthType

#define aap2_BundleADU_adu_flags_ENUMTYPE aap2_BundleADUFlags


#define aap2_DispatchRequest_reason_ENUMTYPE aap2_DispatchReason

#define aap2_Link_status_ENUMTYPE aap2_LinkStatus


#define aap2_AAPResponse_response_status_ENUMTYPE aap2_ResponseStatus




/* Initializer values for message structs */
#define aap2_AAPMessage_init_default             {0, {aap2_Welcome_init_default}}
#define aap2_Welcome_init_default                {NULL}
#define aap2_ConnectionConfig_init_default       {0, _aap2_AuthType_MIN, NULL, NULL, 0}
#define aap2_BundleADU_init_default              {NULL, NULL, 0, 0, 0, _aap2_BundleADUFlags_MIN}
#define aap2_Bundle_init_default                 {NULL, NULL, 0, 0, 0, 0, 0, 0}
#define aap2_DispatchRequest_init_default        {false, aap2_Bundle_init_default, _aap2_DispatchReason_MIN}
#define aap2_Link_init_default                   {_aap2_LinkStatus_MIN, NULL, NULL}
#define aap2_Keepalive_init_default              {0}
#define aap2_AAPResponse_init_default            {_aap2_ResponseStatus_MIN, false, aap2_DispatchResult_init_default, false, aap2_Bundle_init_default}
#define aap2_DispatchResult_init_default         {0, NULL}
#define aap2_DispatchResult_NextHopEntry_init_default {NULL}
#define aap2_AAPMessage_init_zero                {0, {aap2_Welcome_init_zero}}
#define aap2_Welcome_init_zero                   {NULL}
#define aap2_ConnectionConfig_init_zero          {0, _aap2_AuthType_MIN, NULL, NULL, 0}
#define aap2_BundleADU_init_zero                 {NULL, NULL, 0, 0, 0, _aap2_BundleADUFlags_MIN}
#define aap2_Bundle_init_zero                    {NULL, NULL, 0, 0, 0, 0, 0, 0}
#define aap2_DispatchRequest_init_zero           {false, aap2_Bundle_init_zero, _aap2_DispatchReason_MIN}
#define aap2_Link_init_zero                      {_aap2_LinkStatus_MIN, NULL, NULL}
#define aap2_Keepalive_init_zero                 {0}
#define aap2_AAPResponse_init_zero               {_aap2_ResponseStatus_MIN, false, aap2_DispatchResult_init_zero, false, aap2_Bundle_init_zero}
#define aap2_DispatchResult_init_zero            {0, NULL}
#define aap2_DispatchResult_NextHopEntry_init_zero {NULL}

/* Field tags (for use in manual encoding/decoding) */
#define aap2_Welcome_node_id_tag                 1
#define aap2_ConnectionConfig_is_subscriber_tag  1
#define aap2_ConnectionConfig_auth_type_tag      2
#define aap2_ConnectionConfig_secret_tag         3
#define aap2_ConnectionConfig_endpoint_id_tag    4
#define aap2_ConnectionConfig_keepalive_seconds_tag 5
#define aap2_BundleADU_src_eid_tag               1
#define aap2_BundleADU_dst_eid_tag               2
#define aap2_BundleADU_creation_timestamp_ms_tag 3
#define aap2_BundleADU_sequence_number_tag       4
#define aap2_BundleADU_payload_length_tag        5
#define aap2_BundleADU_adu_flags_tag             6
#define aap2_Bundle_src_eid_tag                  1
#define aap2_Bundle_dst_eid_tag                  2
#define aap2_Bundle_creation_timestamp_ms_tag    3
#define aap2_Bundle_sequence_number_tag          4
#define aap2_Bundle_payload_length_tag           5
#define aap2_Bundle_fragment_offset_tag          6
#define aap2_Bundle_total_adu_length_tag         7
#define aap2_Bundle_lifetime_ms_tag              8
#define aap2_DispatchRequest_bundle_tag          1
#define aap2_DispatchRequest_reason_tag          2
#define aap2_Link_status_tag                     1
#define aap2_Link_peer_node_id_tag               2
#define aap2_Link_peer_cla_addr_tag              3
#define aap2_AAPMessage_welcome_tag              1
#define aap2_AAPMessage_config_tag               2
#define aap2_AAPMessage_adu_tag                  3
#define aap2_AAPMessage_dispatch_request_tag     4
#define aap2_AAPMessage_link_tag                 5
#define aap2_AAPMessage_keepalive_tag            6
#define aap2_DispatchResult_next_hops_tag        1
#define aap2_AAPResponse_response_status_tag     1
#define aap2_AAPResponse_dispatch_result_tag     2
#define aap2_AAPResponse_bundle_headers_tag      3
#define aap2_DispatchResult_NextHopEntry_node_id_tag 1

/* Struct field encoding specification for nanopb */
#define aap2_AAPMessage_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,welcome,msg.welcome),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,config,msg.config),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,adu,msg.adu),   3) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,dispatch_request,msg.dispatch_request),   4) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,link,msg.link),   5) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,keepalive,msg.keepalive),   6)
#define aap2_AAPMessage_CALLBACK NULL
#define aap2_AAPMessage_DEFAULT NULL
#define aap2_AAPMessage_msg_welcome_MSGTYPE aap2_Welcome
#define aap2_AAPMessage_msg_config_MSGTYPE aap2_ConnectionConfig
#define aap2_AAPMessage_msg_adu_MSGTYPE aap2_BundleADU
#define aap2_AAPMessage_msg_dispatch_request_MSGTYPE aap2_DispatchRequest
#define aap2_AAPMessage_msg_link_MSGTYPE aap2_Link
#define aap2_AAPMessage_msg_keepalive_MSGTYPE aap2_Keepalive

#define aap2_Welcome_FIELDLIST(X, a) \
X(a, POINTER,  SINGULAR, STRING,   node_id,           1)
#define aap2_Welcome_CALLBACK NULL
#define aap2_Welcome_DEFAULT NULL

#define aap2_ConnectionConfig_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, BOOL,     is_subscriber,     1) \
X(a, STATIC,   SINGULAR, UENUM,    auth_type,         2) \
X(a, POINTER,  SINGULAR, STRING,   secret,            3) \
X(a, POINTER,  SINGULAR, STRING,   endpoint_id,       4) \
X(a, STATIC,   SINGULAR, UINT32,   keepalive_seconds,   5)
#define aap2_ConnectionConfig_CALLBACK NULL
#define aap2_ConnectionConfig_DEFAULT NULL

#define aap2_BundleADU_FIELDLIST(X, a) \
X(a, POINTER,  SINGULAR, STRING,   src_eid,           1) \
X(a, POINTER,  SINGULAR, STRING,   dst_eid,           2) \
X(a, STATIC,   SINGULAR, UINT64,   creation_timestamp_ms,   3) \
X(a, STATIC,   SINGULAR, UINT64,   sequence_number,   4) \
X(a, STATIC,   SINGULAR, UINT64,   payload_length,    5) \
X(a, STATIC,   SINGULAR, UENUM,    adu_flags,         6)
#define aap2_BundleADU_CALLBACK NULL
#define aap2_BundleADU_DEFAULT NULL

#define aap2_Bundle_FIELDLIST(X, a) \
X(a, POINTER,  SINGULAR, STRING,   src_eid,           1) \
X(a, POINTER,  SINGULAR, STRING,   dst_eid,           2) \
X(a, STATIC,   SINGULAR, UINT64,   creation_timestamp_ms,   3) \
X(a, STATIC,   SINGULAR, UINT64,   sequence_number,   4) \
X(a, STATIC,   SINGULAR, UINT64,   payload_length,    5) \
X(a, STATIC,   SINGULAR, UINT64,   fragment_offset,   6) \
X(a, STATIC,   SINGULAR, UINT64,   total_adu_length,   7) \
X(a, STATIC,   SINGULAR, UINT64,   lifetime_ms,       8)
#define aap2_Bundle_CALLBACK NULL
#define aap2_Bundle_DEFAULT NULL

#define aap2_DispatchRequest_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, MESSAGE,  bundle,            1) \
X(a, STATIC,   SINGULAR, UENUM,    reason,            2)
#define aap2_DispatchRequest_CALLBACK NULL
#define aap2_DispatchRequest_DEFAULT NULL
#define aap2_DispatchRequest_bundle_MSGTYPE aap2_Bundle

#define aap2_Link_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    status,            1) \
X(a, POINTER,  SINGULAR, STRING,   peer_node_id,      2) \
X(a, POINTER,  SINGULAR, STRING,   peer_cla_addr,     3)
#define aap2_Link_CALLBACK NULL
#define aap2_Link_DEFAULT NULL

#define aap2_Keepalive_FIELDLIST(X, a) \

#define aap2_Keepalive_CALLBACK NULL
#define aap2_Keepalive_DEFAULT NULL

#define aap2_AAPResponse_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    response_status,   1) \
X(a, STATIC,   OPTIONAL, MESSAGE,  dispatch_result,   2) \
X(a, STATIC,   OPTIONAL, MESSAGE,  bundle_headers,    3)
#define aap2_AAPResponse_CALLBACK NULL
#define aap2_AAPResponse_DEFAULT NULL
#define aap2_AAPResponse_dispatch_result_MSGTYPE aap2_DispatchResult
#define aap2_AAPResponse_bundle_headers_MSGTYPE aap2_Bundle

#define aap2_DispatchResult_FIELDLIST(X, a) \
X(a, POINTER,  REPEATED, MESSAGE,  next_hops,         1)
#define aap2_DispatchResult_CALLBACK NULL
#define aap2_DispatchResult_DEFAULT NULL
#define aap2_DispatchResult_next_hops_MSGTYPE aap2_DispatchResult_NextHopEntry

#define aap2_DispatchResult_NextHopEntry_FIELDLIST(X, a) \
X(a, POINTER,  SINGULAR, STRING,   node_id,           1)
#define aap2_DispatchResult_NextHopEntry_CALLBACK NULL
#define aap2_DispatchResult_NextHopEntry_DEFAULT NULL

extern const pb_msgdesc_t aap2_AAPMessage_msg;
extern const pb_msgdesc_t aap2_Welcome_msg;
extern const pb_msgdesc_t aap2_ConnectionConfig_msg;
extern const pb_msgdesc_t aap2_BundleADU_msg;
extern const pb_msgdesc_t aap2_Bundle_msg;
extern const pb_msgdesc_t aap2_DispatchRequest_msg;
extern const pb_msgdesc_t aap2_Link_msg;
extern const pb_msgdesc_t aap2_Keepalive_msg;
extern const pb_msgdesc_t aap2_AAPResponse_msg;
extern const pb_msgdesc_t aap2_DispatchResult_msg;
extern const pb_msgdesc_t aap2_DispatchResult_NextHopEntry_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define aap2_AAPMessage_fields &aap2_AAPMessage_msg
#define aap2_Welcome_fields &aap2_Welcome_msg
#define aap2_ConnectionConfig_fields &aap2_ConnectionConfig_msg
#define aap2_BundleADU_fields &aap2_BundleADU_msg
#define aap2_Bundle_fields &aap2_Bundle_msg
#define aap2_DispatchRequest_fields &aap2_DispatchRequest_msg
#define aap2_Link_fields &aap2_Link_msg
#define aap2_Keepalive_fields &aap2_Keepalive_msg
#define aap2_AAPResponse_fields &aap2_AAPResponse_msg
#define aap2_DispatchResult_fields &aap2_DispatchResult_msg
#define aap2_DispatchResult_NextHopEntry_fields &aap2_DispatchResult_NextHopEntry_msg

/* Maximum encoded size of messages (where known) */
/* aap2_AAPMessage_size depends on runtime parameters */
/* aap2_Welcome_size depends on runtime parameters */
/* aap2_ConnectionConfig_size depends on runtime parameters */
/* aap2_BundleADU_size depends on runtime parameters */
/* aap2_Bundle_size depends on runtime parameters */
/* aap2_DispatchRequest_size depends on runtime parameters */
/* aap2_Link_size depends on runtime parameters */
/* aap2_AAPResponse_size depends on runtime parameters */
/* aap2_DispatchResult_size depends on runtime parameters */
/* aap2_DispatchResult_NextHopEntry_size depends on runtime parameters */
#define aap2_Keepalive_size                      0

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
