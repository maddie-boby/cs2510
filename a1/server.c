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

#include "uthash/uthash.h"

#include "jobqueue.h"
#include "message.h"
#include "message_net.h"
#include "linked_list.h"

#define MAX_BACKLOG 10

struct client {
    int id; // hash key
    // uint16_t id;
    struct llist *sockfds;
    // int sockfd;
    struct llist_node *last_read;
    UT_hash_handle hh;
};

struct message_log {
    struct llist *list;
    pthread_rwlock_t lock;
};
static struct message_log *log;

// struct client_list {
//     struct llist *list;
//     pthread_mutex_t lock;
// };
// static struct client_list *client_list;

static struct client *clients;
static pthread_rwlock_t clients_lock;

static struct jobqueue *sendqueue;

// static pthread_t *listener_threads;
static int listener_threads_size;
static int num_listeners;
static struct llist *listener_threads;
static pthread_t sender_thread;


static struct message_log *create_message_log(void)
{
    struct message_log *log = NULL;

    log = malloc(sizeof(*log));

    log->list = create_llist();

    pthread_rwlock_init(&log->lock, 0);
    return log;
}

static struct llist_node *message_log_append(struct message_log *l, struct message *m)
{
    llist_insert_tail(l->list, m);
    return l->list->tail;
}

static void init_client_hash(void)
{
    pthread_rwlock_init(&clients_lock, 0);
}

static void client_hash_add_client(struct client *c)
{
    struct client *tmp;

    HASH_FIND_INT(clients, &c->id, tmp);
    if (tmp == NULL) {
        HASH_ADD_INT(clients, id, c);
    }
}

static void client_hash_add_connection(int id, int fd)
{
    struct client *c;

    HASH_FIND_INT(clients, &id, c);
    if (c == NULL) {
        return;
    }
    llist_insert_tail(c->sockfds, (void *) fd);

}

static void client_hash_remove_connection(int id, int fd)
{
    struct client *c;

    HASH_FIND_INT(clients, &id, c);

    if (c == NULL) {
        return;
    }
    llist_remove_value(c->sockfds, (void *) fd);
}

static struct client *client_hash_get(int id)
{
    struct client *c;

    HASH_FIND_INT(clients, &id, c);
    return c;
}

static struct client *create_client(uint16_t id)
{
    struct client *c = NULL;

    c = malloc(sizeof(*c));
    c->id = id;
    c->sockfds = create_llist();
    c->last_read = NULL;

    return c;
}

static void free_client(struct client *c)
{
    free_llist(c->sockfds);
    free(c);
}

static void *sender(void *data)
{
    int ret; 
    while (1) {
        struct message *m = NULL;
        struct llist_node *last_node = NULL;
        m = jobqueue_dequeue(sendqueue);
        
        pthread_rwlock_wrlock(&log->lock);
        {
            last_node = message_log_append(log, m);
        }
        pthread_rwlock_unlock(&log->lock);

        pthread_rwlock_wrlock(&clients_lock);
        {
            struct client *c = NULL;

            c = clients;
            while (c != NULL) {
                if (llist_is_empty(c->sockfds)) {
                    c = c->hh.next;
                    continue;
                }
                if (c->id != m->id) {
                    struct llist_node *n = NULL;
                    n = c->sockfds->head;
                    while (n != NULL) {
                        int fd = (int) n->data;
                        ret = send_message(fd, m);
                        if (ret == -1) {
                            printf("Notice: sender()-failed to send to client (client: %d)", c->id);
                        }
                        n = n->next;
                    }
                }
                c->last_read = last_node;
                c = c->hh.next;
            }
        }
        pthread_rwlock_unlock(&clients_lock);
        
    }
    return NULL;
}

static int handshake(int clientfd, struct message **m)
{
    int id;

    *m = recv_header(clientfd);

    if (*m == NULL) {
        printf("Error: handshake()-recv_header()=NULL (fd: %d)\n", clientfd);
        return -1;
    }

    if ((*m)->kind != MESG_CONN) {
        printf("Error: handshake()-Incorrect message kind (id: %d, kind: %d)", (*m)->id, (*m)->kind);
        return -1;
    }
    id = (*m)->id;
    return id;
}


static int fast_forward(struct client *c, int fd)
{
    struct llist_node *cursor;
    struct message *m;
    int ret;

    pthread_rwlock_rdlock(&log->lock);
    {
        cursor = (c->last_read == NULL) ? log->list->head : c->last_read;
        while (cursor != NULL) {
            m = cursor->data;
            if (m->id != c->id) {
                ret = send_message(fd, m);
                if (ret == -1) {
                    goto err;
                }
            }
            cursor = cursor->next;
        }
        c->last_read = log->list->tail;
    }
    pthread_rwlock_unlock(&log->lock);

    return 0;
err:
    pthread_rwlock_unlock(&log->lock);
    return -1;
}

static void *listener(void *data)
{
    struct client *c = NULL;
    struct message *handshake_msg = NULL;
    int client_id = -1;
    int ret;

    int fd;
    
    fd = (int) data;

    client_id = handshake(fd, &handshake_msg);
    if (client_id <= 0) {
        struct message *emsg = NULL;
        char *estr = "Error: incorrect handshake kind";

        printf("Error: listner()-handshake()\n");


        emsg = create_message(strlen(estr) + 1, 0, MESG_DCON, estr);
        send_message(fd, emsg);

        free_message(emsg);
        free_message(handshake_msg);
        close(fd);
        return (void *) -1;
    }

    pthread_rwlock_rdlock(&clients_lock);
    {
        c = client_hash_get(client_id);
    }
    pthread_rwlock_unlock(&clients_lock);

    if (c == NULL) {
        struct message *emsg = NULL;
        char *estr = "Error: unknown client id";

        emsg = create_message(strlen(estr) + 1, 0, MESG_DCON, estr);
        send_message(fd, emsg);
        free_message(emsg);
        free_message(handshake_msg);
        close(fd);
        return (void *) -1;
    } else {
        struct message *amsg = NULL;
        struct message *cmsg = NULL;
        amsg = create_message_header(0, 0, MESG_CONN);
        send_message(fd, amsg);
        free_message(amsg);
        cmsg = create_message_header(0, client_id, MESG_CONN);
        jobqueue_enqueue(sendqueue, cmsg);
    }

    ret = fast_forward(c, fd);

    pthread_rwlock_wrlock(&clients_lock);
    {
        client_hash_add_connection(client_id, fd);
    }
    pthread_rwlock_unlock(&clients_lock);

    while (1) {
        struct message *m = NULL;
        m = recv_header(fd);
        if (m->kind == MESG_DCON) {
            jobqueue_enqueue(sendqueue, m);
            goto out;
        } else if (m->kind == MESG_CONN) {
            // drop duplicate CONN messages
            free_message(m);
            continue;
        }
        ret = recv_payload(fd, m);
        if (ret != 0 || m == NULL) {
            // TODO: check errno and don't just disconnect
            goto out;
        }
        jobqueue_enqueue(sendqueue, m);
    }

out:
    pthread_rwlock_wrlock(&clients_lock);
    {
        client_hash_remove_connection(client_id, fd);
    }
    pthread_rwlock_unlock(&clients_lock);
    close(fd);

    return NULL;
}

static int parse_client_ids(char *id_str, uint16_t **ids, int *id_count)
{
    char *tok = NULL;
    uint16_t *l_ids;
    int l_id_count;

    l_ids = calloc(5, sizeof(uint16_t));
    l_id_count = 0;

    tok = strtok(id_str, " ,");
    while (tok != NULL) {
        int i = atoi(tok);
        if (i <= 0 || i > UINT16_MAX) {
            return -1;
        }
        if (l_id_count != 0 && l_id_count % 5 == 0) {
            l_ids = realloc(l_ids, l_id_count + 5);
        }
        l_ids[l_id_count] = (uint16_t) i;
        l_id_count++;

        tok = strtok(NULL, " ,");
    }
    *ids = l_ids;
    *id_count = l_id_count;

    return 0;
}

static void print_usage(char *name)
{
    printf("Usage: %s --client_ids=<int,...> --port=<int>\n", name);
    printf("--client_ids\tA comma separated list of integers without spaces\n");
    printf("--port\tThe port to listen on\n");
}

int main(int argc, char **argv)
{
    pthread_t *listener_thread;
    pthread_attr_t attr;

    int serverfd = 0;
    int clientfd = 0;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int sin_size;
    int client_name_len = 0;
    int enable;

    int ret;


    uint16_t *client_ids = NULL;
    int id_count = 0;
    int port = -1;

    struct option long_options[] = {
        {"client_ids", required_argument, 0, 0},
        {"port", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }

    while (1) {
        int c = -1;
        int err;
        // int this_option_optind = optind ? optind : 1 ;
        int optindex;
        c = getopt_long(argc, argv, "c:p:", long_options, &optindex);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 0:
            if (optindex == 0) {
                // client_ids
                if (optarg == NULL) {
                    printf("Error: No argument given for client_ids\n");
                    return -1;
                }
                err = parse_client_ids(optarg, &client_ids, &id_count);
                if (err != 0) {
                    printf("Error: illegal client id given\n");
                    return -1;
                } else if (id_count < 2) {
                    printf("Error: at least 2 ids required\n");
                }                
            } else if (optindex == 1) {
                // port
                if (optarg == NULL) {
                    printf("Error: port not given\n");
                    return -1;
                }
                port = atoi(optarg);
                if (port <= 0) {
                    printf("Port number must be > 0\n");
                    return -1;
                } else if (port > UINT16_MAX) {
                    printf("Port number too high, must be < %d\n", UINT16_MAX);
                    return -1;
                }

            }
            break;
        case 'c':
            // client_ids
            if (optarg == NULL) {
                printf("Error: No argument given for client_ids\n");
                return -1;
            }
            err = parse_client_ids(optarg, &client_ids, &id_count);
            if (err != 0) {
                printf("Error: illegal client id given\n");
                return -1;
            } else if (id_count < 2) {
                printf("Error: at least 2 ids required\n");
                return -1;
            }
            break;
        case 'p':
            // port
            port = atoi(optarg);
            if (port > UINT16_MAX) {
                printf("Port number too high, must be < %d\n", UINT16_MAX);
                return -1;
            }
            break;
        case '?':
            printf("Error: Unknown argument\n");
            return -1;
        case ':':
            printf("Error: Argument required\n");
            return -1;
        }
    }

    if (client_ids == NULL) {
        printf("Error: No client ids found\n");
        return -1;
    }

    init_client_hash();

    for (int i = 0; i < id_count; i++) {
        struct client *c = NULL;
        c = create_client(client_ids[i]);
        pthread_rwlock_wrlock(&clients_lock);
        {
            client_hash_add_client(c);
        }
        pthread_rwlock_unlock(&clients_lock);
    }

    sendqueue = create_jobqueue(0);

    log = create_message_log();

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // listener_threads = malloc(id_count * sizeof(pthread_t));
    // listener_threads_size = id_count;
    // num_listeners = 0;
    listener_threads = create_llist();

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        printf("Error: socket()\n");
        return -1;
    }

    enable = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t) port);

    ret = bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("Error: bind()\n");
        return -1;
    }

    ret = listen(serverfd, MAX_BACKLOG);
    if (ret != 0) {
        printf("Error: listen()\n");
        return -1;
    }

    printf("Started server on port %d with client ids:", port);
    for (int i = 0; i < id_count; i++) {
        printf(" %d", client_ids[i]);
    }
    printf("\n");


    pthread_create(&sender_thread, &attr, sender, NULL);

    while (1) {

        sin_size = sizeof(client_addr);
        clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &sin_size);
        if (clientfd < 0) {
            printf("Error: accept()\n");
            exit(1);
        }
        listener_thread = calloc(sizeof(pthread_t), 1);
        llist_insert_tail(listener_threads, listener_thread);
        
        pthread_create(listener_thread, &attr, listener, (void *) clientfd);
    }

    pthread_join(sender_thread, NULL);

    return 0;
}
