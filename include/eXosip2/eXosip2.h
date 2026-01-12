#ifndef EXOSIP2_H
#define EXOSIP2_H

// This is a mock header file to simulate the presence of the eXosip2 library,
// as it could not be installed in the current environment.
// It allows the project to be compilable and demonstrates API usage.

#include <osip2/osip.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mock structures
typedef struct eXosip_t eXosip_t;
typedef struct eXosip_event eXosip_event_t;

// Mock event types
typedef enum eXosip_event_type {
    EXOSIP_REGISTRATION_SUCCESS,
    EXOSIP_REGISTRATION_FAILURE,
    EXOSIP_MESSAGE_NEW,
    EXOSIP_CALL_INVITE
} eXosip_event_type_t;

// Mock API functions
eXosip_t* eXosip_malloc(void);
int eXosip_init(eXosip_t *excontext);
void eXosip_quit(eXosip_t *excontext);
int eXosip_listen_addr(eXosip_t *excontext, int transport, const char *addr, int port, int family, int secure);
int eXosip_register_build_initial_register(eXosip_t *excontext, const char *from, const char *proxy, const char *contact, int expires, osip_message_t **reg);
int eXosip_register_send_register(eXosip_t *excontext, int rid, osip_message_t *reg);
eXosip_event_t* eXosip_event_wait(eXosip_t *excontext, int tv_s, int tv_ms);
void eXosip_event_free(eXosip_event_t *je);
int eXosip_add_authentication_info(eXosip_t *excontext, const char *userid, const char *username, const char *passwd, const char *ha1, const char *realm);

struct eXosip_event {
    eXosip_event_type_t type;
    int rid;
    // Other fields would be here in the real header
};

#ifdef __cplusplus
}
#endif

#endif // EXOSIP2_H
