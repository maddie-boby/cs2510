#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include "message.h"


struct message *create_message(uint32_t length, uint16_t id, uint16_t kind, uint8_t *payload)
{
    struct message *m = NULL;
    
    m = malloc(sizeof(*m));
    m->length = (length > MESG_MAX_LEN) ? MESG_MAX_LEN : length;
    m->id = id;
    m->kind = kind;
    m->payload = malloc(m->length);
    memcpy(m->payload, payload, m->length);

    // ensure null termination
    m->payload[m->length - 1] = '\0';

    return m;
}

struct message *create_message_header(uint32_t length, uint16_t id, uint16_t kind)
{
    struct message *m = NULL;
    
    m = malloc(sizeof(*m));
    m->length = length;
    m->id = id;
    m->kind = kind;
    m->payload = NULL;

    return m;
}

void free_message(struct message *m)
{
    if (m->payload) free(m->payload);
    free(m);
}

void message_marshal(struct message *m, uint8_t **out, int *out_len)
{
    uint8_t *cursor = NULL;
    uint32_t *int_ptr;
    uint16_t *short_ptr;
    int size = 0;

    if (m == NULL) {
        *out = NULL;
        *out_len = 0;
        return;
    }

    size = 2 + 2 + 4 + m->length;
    *out = calloc(size, 1);
    cursor = *out;

    short_ptr = (uint16_t *) cursor;
    *short_ptr = htons(m->id);
    cursor += sizeof(*short_ptr);
    
    short_ptr = (uint16_t *) cursor;
    *short_ptr = htons(m->kind);
    cursor += sizeof(*short_ptr);

    int_ptr = (uint32_t *) cursor;
    *int_ptr = htonl(m->length);
    cursor += sizeof(*int_ptr);

    if (m->payload != NULL) {
        memcpy(cursor, m->payload, m->length);
    }
    *out_len = size;
}

struct message *message_unmarshal(uint8_t *data)
{
    struct message *m = NULL;
    uint8_t *cursor;
    uint32_t *int_ptr;
    uint16_t *short_ptr;
    
    cursor = data;

    m = malloc(sizeof(*m));
    
    short_ptr = (uint16_t *) cursor;
    m->id = ntohs(*short_ptr);
    cursor += sizeof(*short_ptr);
    
    short_ptr = (uint16_t *) cursor;
    m->kind = ntohs(*short_ptr);
    cursor += sizeof(*short_ptr);

    int_ptr = (uint32_t *) cursor;
    m->length = ntohl(*int_ptr);
    cursor += sizeof(*int_ptr);

    m->payload = calloc(m->length, 1);
    memcpy(m->payload, cursor, m->length);

    return m;
}

struct message *message_unmarshal_header(uint8_t *data)
{
    struct message *m = NULL;
    uint8_t *cursor;
    uint32_t *int_ptr;
    uint16_t *short_ptr;
    
    cursor = data;

    m = malloc(sizeof(*m));
    
    short_ptr = (uint16_t *) cursor;
    m->id = ntohs(*short_ptr);
    cursor += sizeof(*short_ptr);
    
    short_ptr = (uint16_t *) cursor;
    m->kind = ntohs(*short_ptr);
    cursor += sizeof(*short_ptr);

    int_ptr = (uint32_t *) cursor;
    m->length = ntohl(*int_ptr);
    cursor += sizeof(*int_ptr);

    m->payload = NULL;

    return m;
}

