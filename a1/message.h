#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>

#define MESG_DATA 0
#define MESG_CONN 1
#define MESG_DCON 2

#define MESG_HEADER_LEN 8
#define MESG_MAX_LEN 2040

// Message format:
// 0-1 | Sender ID
// 2-3 | Message kind
// 4-7 | payload length INCLUDING '\0'
// 8: | null terminated payload
struct message {
    uint16_t id;
    uint16_t kind;
    uint32_t length;
    uint8_t *payload;
};

struct message *create_message(uint32_t length, uint16_t id, uint16_t kind, uint8_t *payload);
struct message *create_message_header(uint32_t length, uint16_t id, uint16_t kind);

void free_message(struct message *m);

void message_marshal(struct message *m, uint8_t **out, int *out_len);

struct message *message_unmarshal(uint8_t *data);
struct message *message_unmarshal_header(uint8_t *data);

#endif
