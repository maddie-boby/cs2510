#ifndef __MESSAGE_NET_H__
#define __MESSAGE_NET_H__

struct message;

struct message *recv_header(int fd);
int recv_payload(int fd, struct message *m);
int send_message(int fd, struct message *m);

#endif
