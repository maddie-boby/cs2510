#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <pthread.h>

#include "message.h"
#include "message_net.h"

pthread_t recv_thread;
pthread_t send_thread;

struct info {
    int id;
    int fd;
};

struct sender_args {
    struct info *info;
    FILE *in;
};

void print_message(struct message *m)
{
    if (m->kind == MESG_CONN) {
        if (m->id == 0) {
            printf("<server>: connection accepted\n");
        } else {
            printf("<server>: <%d> connected\n", m->id);
        }
    } else if (m->kind == MESG_DCON) {
        if (m->id == 0) {
            printf("<server>: disconnected");
            if (m->length > 0) {
                printf(" - %s\n", m->payload);
            } else {
                printf("\n");
            }
        } else {
            printf("<server>: <%d> disconnected\n", m->id);
        }
    } else if (m->kind == MESG_DATA) {
        if (m->id == 0) {
            printf("<server>: ");
        } else {
            printf("<%d>: ", m->id);
        }
        printf("%s\n", m->payload);
    }
}

int handshake(struct info *info)
{
    int id;
    int fd;
    struct message *m = NULL;
    int ret = -1;

    id = info->id;
    fd = info->fd;

    m = create_message_header(0, (uint16_t) id, MESG_CONN);

    ret = send_message(fd, m);
    free(m);
    if (ret != 0) {
        return ret;
    }
    m = recv_header(fd);
    if (m->kind == MESG_DCON) {
        // printf("Error: Server refused connection\n");
        recv_payload(fd, m);
    }
    print_message(m);

    return (m->kind) == MESG_CONN ? 0 : -1;    
}

void *receiver(void *data)
{
    struct info *info;
    int fd;
    int id;
    struct message *m;
    int ret;

    info = data;

    fd = info->fd;
    id = info->id;

    while (1) {
        m = recv_header(fd);
        if (m->kind == MESG_DATA) {
            ret = recv_payload(fd, m);
            if (ret == -1) {
                printf("Error receiving message\n");
                free_message(m);
                break;
            }
        }
        print_message(m);
        if (m->kind == MESG_DCON && m->id == 0) {
            free_message(m);
            break;
        }
    }
    pthread_cancel(send_thread);
    return NULL;
}

void *sender(void *data)
{
    struct sender_args *args;
    struct info *info;
    int fd;
    int id;

    args = data;
    info = args->info;

    fd = info->fd;
    id = info->id;

    while (1) {
        char *line = NULL;
        size_t len = 0;
        int send_len = 0;
        int slen = 0;

        struct message *m = NULL;
        int ret = -1;
        int read = 0;

        read = getline(&line, &len, args->in);
        if (read == -1) {
            printf("Error: sender()-getline() read=-1\n");    
            break;
        }
        slen = strlen(line);
        if (line[slen - 1] == '\n') {
            line[slen - 1] = '\0';
        }
        if (strcmp(line, "<<quit>>") == 0) {
            struct message *qmsg = NULL;
            qmsg = create_message_header(0, id, MESG_DCON);
            ret = send_message(fd, qmsg);
            if (ret == -1) {
                printf("Error: sender()-send_message()--header ret=-1\n");
            }
            free(line);
            break;
        }
        if (slen > MESG_MAX_LEN) {
            send_len = MESG_MAX_LEN;
            line[send_len - 1] = '\0';
        } else {
            send_len = (int) slen;
        }

        m = create_message(send_len, id, MESG_DATA, line);

        ret = send_message(fd, m);
        free(m);
        free(line);
        if (ret == -1) {
            printf("Error: sender()-send_message() %s\n", strerror(errno));
            break;
        }
        if (args->in != stdin) {
            usleep(10);
        }
    }

    pthread_cancel(recv_thread);

    return NULL;
}

void print_usage(char *name)
{
    printf("Chat client. Enter <<quit>> to exit\n");
    printf("Usage: %s --client_id=<int> --server=<ip-addr> --port=<int> [--file=<file>]\n", name);
    printf("--client_id\tA positive integer\n");
    printf("--server\tThe server's ip address\n");
    printf("--port\tThe server's port\n");
    printf("--file\tScript file for test drivers\n");
}

int main(int argc, char **argv)
{

    int client_id;
    char *ip_str;
    int port;
    char *port_str;



    pthread_attr_t attr;

    int fd;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;
    char s[INET6_ADDRSTRLEN];
    struct sockaddr *sa;
    void *inaddr;

    FILE *in;
    struct info *info;
    struct sender_args *sargs;


    int ret;

    struct option long_options[] = {
        {"client_id", required_argument, 0, 0},
        {"server", required_argument, 0, 0},
        {"port", required_argument, 0, 0},
        {"file", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }

    in = stdin;

    while (1) {
        int c = -1;
        int err;
        int optindex;

        c = getopt_long(argc, argv, "c:i:p:f:", long_options, &optindex);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 0:
            if (optindex == 0) {
                // client_id
                if (optarg == NULL) {
                    printf("Error: no client id given\n");
                    return -1;
                }
                client_id = atoi(optarg);
                if (client_id < 1 || client_id > UINT16_MAX) {
                    printf("Error: Illegal client number. Must be between 1 and %d\n", UINT16_MAX);
                    return -1;
                }
            } else if (optindex == 1) {
                // server
                if (optarg == NULL) {
                    printf("Error: no server ip specified\n");
                    return -1;
                }
                ip_str = optarg;
            } else if (optindex == 2) {
                // port
                if (optarg == NULL) {
                    printf("Error: no port given\n");
                    return -1;
                }
                port = atoi(optarg);
                if (port < 1 || port > UINT16_MAX) {
                    printf("Error: Illegal port. Must be between 1 and %d\n", UINT16_MAX);
                    return -1;
                }
                port_str = optarg;
            } else if (optindex == 3) {
                in = fopen(optarg, "r");
                if (in == NULL) {
                    printf("Error opening file %s\n", optarg);
                    return -1;
                }
            }
            break;
        case 'c':
            // client_id
            if (optarg == NULL) {
                printf("Error: no client id given\n");
                return -1;
            }
            client_id = atoi(optarg);
            if (client_id < 1 || client_id > UINT16_MAX) {
                printf("Error: Illegal client number. Must be between 1 and %d\n", UINT16_MAX);
                return -1;
            }
            break;
        case 'i':
            // server
            if (optarg == NULL) {
                printf("Error: no server ip specified\n");
                return -1;
            }
            ip_str = optarg;
            break;
        case 'p':
            // port
            if (optarg == NULL) {
                printf("Error: no port given\n");
                return -1;
            }
            port = atoi(optarg);
            if (port < 1 || port > UINT16_MAX) {
                printf("Error: Illegal port. Must be between 1 and %d\n", UINT16_MAX);
                return -1;
            }
            port_str = optarg;
            break;
        case 'f':
            in = fopen(optarg, "r");
            if (in == NULL) {
                printf("Error opening file %s\n", optarg);
                return -1;
            }
            break;
        case '?':
            printf("Error: unknown argument\n");
            return -1;
        case ':':
            printf("Error: argument required\n");
            return -1;
        }
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(ip_str, port_str, &hints, &servinfo);
    if (ret != 0) {
        printf("Error: main()-getaddrinfo()\n");
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            // perror("main()-socket()");
            continue;
        }

        ret = connect(fd, p->ai_addr, p->ai_addrlen);
        if (ret == -1) {
            close(fd);
            continue;
        }
        break;
    }
    if (p == NULL) {
        printf("Error: main()-Failed to connect\n");
        return -1;
    }


    sa = (struct sockaddr *) p->ai_addr;
    if (sa->sa_family == AF_INET) {
        inaddr = &(((struct sockaddr_in*)sa)->sin_addr);
    } else {
        inaddr = &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

    inet_ntop(p->ai_family, inaddr, s, sizeof(s));

    printf("Connected to %s\n", s);

    freeaddrinfo(servinfo);

    info = malloc(sizeof(*info));
    info->fd = fd;
    info->id = client_id;

    sargs = malloc(sizeof(*sargs));
    sargs->in = in;
    sargs->info = info;

    ret = handshake(info);
    if (ret != 0) {
        close(fd);
        return 0;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_thread, &attr, receiver, info);
    pthread_create(&send_thread, &attr, sender, sargs);

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    if (in != stdin) fclose(in);
    close(fd);
    free(info);
    free(sargs);
    return 0;
}
