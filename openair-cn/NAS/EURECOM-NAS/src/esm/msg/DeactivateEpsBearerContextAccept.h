#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "ProtocolDiscriminator.h"
#include "EpsBearerIdentity.h"
#include "ProcedureTransactionIdentity.h"
#include "MessageType.h"
#include "ProtocolConfigurationOptions.h"

#ifndef DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_H_
#define DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_H_

/* Minimum length macro. Formed by minimum length of each mandatory field */
#define DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_MINIMUM_LENGTH (0)

/* Maximum length macro. Formed by maximum length of each field */
#define DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_MAXIMUM_LENGTH ( \
    PROTOCOL_CONFIGURATION_OPTIONS_MAXIMUM_LENGTH )

/* If an optional value is present and should be encoded, the corresponding
 * Bit mask should be set to 1.
 */
# define DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_PROTOCOL_CONFIGURATION_OPTIONS_PRESENT (1<<0)

typedef enum deactivate_eps_bearer_context_accept_iei_tag {
    DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_PROTOCOL_CONFIGURATION_OPTIONS_IEI  = 0x27, /* 0x27 = 39 */
} deactivate_eps_bearer_context_accept_iei;

/*
 * Message name: Deactivate EPS bearer context accept
 * Description: This message is sent by the UE to acknowledge deactivation of the EPS bearer context requested in the corresponding Deactivate EPS bearer context request message. See table 8.3.11.1.
 * Significance: dual
 * Direction: UE to network
 */

typedef struct deactivate_eps_bearer_context_accept_msg_tag {
    /* Mandatory fields */
    ProtocolDiscriminator                           protocoldiscriminator:4;
    EpsBearerIdentity                               epsbeareridentity:4;
    ProcedureTransactionIdentity                    proceduretransactionidentity;
    MessageType                                     messagetype;
    /* Optional fields */
    uint32_t                                        presencemask;
    ProtocolConfigurationOptions                    protocolconfigurationoptions;
} deactivate_eps_bearer_context_accept_msg;

int decode_deactivate_eps_bearer_context_accept(deactivate_eps_bearer_context_accept_msg *deactivateepsbearercontextaccept, uint8_t *buffer, uint32_t len);

int encode_deactivate_eps_bearer_context_accept(deactivate_eps_bearer_context_accept_msg *deactivateepsbearercontextaccept, uint8_t *buffer, uint32_t len);

#endif /* ! defined(DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT_H_) */

