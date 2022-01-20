#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "message.h"
#include "message_net.h"

struct message *recv_header(int fd)
{
    struct message *m;
    int recvd = 0;
    int n = -1;
    uint8_t recv_buf[MESG_HEADER_LEN];
    uint8_t *cursor = recv_buf;

    memset(recv_buf, 0, MESG_HEADER_LEN);

    while (recvd < MESG_HEADER_LEN) {
        int to_read = MESG_HEADER_LEN-recvd;
        n = recv(fd, cursor, to_read, 0);
        if (n < 0) {
            printf("Error: recv_header()-recv() (fd: %d)", fd);
            return NULL;
        }
        recvd += n;
        cursor += n;
    }
    m = message_unmarshal_header(recv_buf);
    return m;
}

int recv_payload(int fd, struct message *m)
{
    int recvd = 0;
    int n = -1;
    uint8_t *cursor = NULL;
    uint8_t *discard_buf = NULL;
    int recv_len = 0;

    recv_len = m->length;
    m->length = (m->length <= MESG_MAX_LEN) ? m->length : MESG_MAX_LEN;
    m->payload = malloc(m->length);
    cursor = m->payload;

    while (recvd < m->length) {
        int to_recv = m->length-recvd;
        n = recv(fd, cursor, to_recv, 0);
        if (n < 0) {
            printf("Error: recv_payload()-recv() (client: %d | fd: %d)\n", m->id, fd);
            return n;
        }
        recvd += n;
        cursor += n;
    }
    if (recvd < recv_len) {
        discard_buf = malloc(2048);
        while (recvd < recv_len) {
            int bytes_left = recv_len-recvd;
            int to_recv = (bytes_left < 2048) ? bytes_left : 2048;
            n = recv(fd, discard_buf, to_recv, 0);
            if (n < 0) {
                printf("Error: recv_payload()-recv() (client: %d | fd: %d)\n", m->id, fd);
                free(discard_buf);
                return n;
            }
            recvd += n;
        }
        free(discard_buf);
    }
    // this should be true anyway but just in case!
    m->payload[m->length - 1] = 0;
    return 0;
}

int send_message(int fd, struct message *m)
{
    int sent = 0;
    int n = -1;
    uint8_t *mesg_bytes = NULL;
    uint8_t *cursor;
    int send_len = 0;

    message_marshal(m, &mesg_bytes, &send_len);
    if (mesg_bytes == NULL || send_len == 0) {
        printf("Error: write_message()-message_marshal()\n");
        goto err;
    }

    cursor = mesg_bytes;
    
    while (sent < send_len) {
        int to_send = send_len-sent;
        n = send(fd, cursor, to_send, 0);
        if (n < 0) {
            printf("Error: write_message()-send() (fd: %d)", fd);
            goto err;
        }
        sent += n;
        cursor += n;
    }

    free(mesg_bytes);

    return 0;
err:
    if (mesg_bytes) free(mesg_bytes);
    return -1;
}