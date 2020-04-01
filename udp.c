/*
 * Original sample file taken from: https://github.com/RIOT-OS/Tutorials/tree/master/task-06
 *
 * All changes and final product:
 * @author  Michael Conard <maconard@mtu.edu>
 *
 * Purpose: A UDP server designed to work with my leader election protocol.
 */

// Standard C includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <msg.h>

// Standard RIOT includes
#include "thread.h"
#include "xtimer.h"

// Networking includes
#include "net/sock/udp.h"
#include "net/ipv6/addr.h"

#define SERVER_MSG_QUEUE_SIZE   (8)
#define SERVER_BUFFER_SIZE      (64)
#define MAX_IPC_MESSAGE_SIZE    (256)

// External functions defs
extern int ipc_msg_send(char *message, kernel_pid_t destinationPID, bool blocking);
extern int ipc_msg_reply(char *message, msg_t incoming);
extern int ipc_msg_send_receive(char *message, kernel_pid_t destinationPID, msg_t *response, uint16_t type);

// Forward declarations
void *_udp_server(void *args);
int udp_send(int argc, char **argv);
int udp_send_multi(int argc, char **argv);
int udp_server(int argc, char **argv);

// Data structures (i.e. stacks, queues, message structs, etc)
static char server_buffer[SERVER_BUFFER_SIZE];
static char server_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t server_msg_queue[SERVER_MSG_QUEUE_SIZE];
static sock_udp_t sock;
static msg_t msg_in, msg_out;

// State variables
static bool server_running = false;
const int SERVER_PORT = 3142;

void *_udp_server(void *args)
{
    //printf("UDP: Entered UDP server code\n");
    sock_udp_ep_t server = { .port = SERVER_PORT, .family = AF_INET6 };
    msg_init_queue(server_msg_queue, SERVER_MSG_QUEUE_SIZE);
    kernel_pid_t leaderPID = (kernel_pid_t)atoi(args);
    int failCount = 0;

    if(sock_udp_create(&sock, &server, NULL, 0) < 0) {
        return NULL;
    }

    server_running = true;
    printf("UDP: Success - started UDP server on port %u\n", server.port);

    char msg_content[MAX_IPC_MESSAGE_SIZE];
    msg_out.type = 0;
    msg_out.content.ptr = (void*)"init";

    printf("UDP: Trying to communicate with process PID=%" PRIkernel_pid  "\n", leaderPID);
    while (1) {
        if (failCount == 10) {
            (void) puts("UDP: Error - timed out on communicating with protocol thread");
            return NULL;
        }

        // wait for protocol thread to initialize    
        int res = msg_try_send(&msg_out, leaderPID);
        if (res == -1) {
            // msg failed because protocol thread doesn't exist or we have the wrong PID
            (void) puts("UDP: Error - UDP server thread can't communicate with protocol thread");
            failCount++;
        } else if (res == 0) {
            // msg failed because protocol thread isn't ready to receive yet
            failCount++;
        } else if (res == 1) {
            // msg succeeded
            printf("UDP: thread successfully initiated communication with the PID=%" PRIkernel_pid  "\n", leaderPID);
            break;
        }

        xtimer_sleep(1); // wait 1 second
    }

    while (1) {
        // incoming UDP
        int res;
        if ((res = sock_udp_recv(&sock, server_buffer,
                                 sizeof(server_buffer) - 1, 0, //SOCK_NO_TIMEOUT,
                                 NULL)) < 0) {
            if(res != 0 && res != -ETIMEDOUT && res != -EAGAIN) 
                printf("UDP: Error - failed to receive UDP, %d\n", res);
        }
        else if (res == 0) {
            (void) puts("UDP: no UDP data received");
        }
        else {
            server_buffer[res] = '\0';
            printf("UDP: recvd: %s\n", server_buffer);
        }

        // incoming thread message
        res = msg_try_receive(&msg_in);
        if (res == 1) {
            if (msg_in.type > 0 && msg_in.type < MAX_IPC_MESSAGE_SIZE) {
                // process string message of size msg_in.type
                strncpy(msg_content, (char*)msg_in.content.ptr, (uint16_t)msg_in.type);
                printf("UDP: received IPC message: %s from %" PRIkernel_pid "\n", msg_content, msg_in.sender_pid);
            } else {
                printf("UPD: received an illegal or too large IPC message, type=%u", msg_in.type);
            }

            memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE * (sizeof msg_content[0]));
        }

        xtimer_sleep(1); // wait 1 second
    }

    return NULL;
}

int udp_send(int argc, char **argv)
{
    int res;
    sock_udp_ep_t remote = { .family = AF_INET6 };

    if (argc != 4) {
        (void) puts("UDP: Usage - udp <ipv6-addr> <port> <payload>");
        return -1;
    }

    if (ipv6_addr_from_str((ipv6_addr_t *)&remote.addr, argv[1]) == NULL) {
        (void) puts("UDP: Error - unable to parse destination address");
        return 1;
    }
    if (ipv6_addr_is_link_local((ipv6_addr_t *)&remote.addr)) {
        /* choose first interface when address is link local */
        gnrc_netif_t *netif = gnrc_netif_iter(NULL);
        remote.netif = (uint16_t)netif->pid;
    }
    remote.port = atoi(argv[2]);
    if((res = sock_udp_send(NULL, argv[3], strlen(argv[3]), &remote)) < 0) {
        printf("UDP: Error - could not send message \"%s\" to %s\n", argv[3], argv[1]);
    }
    else {
        printf("UDP: Success - sent %u byte to %s\n", (unsigned) res, argv[1]);
    }
    return 0;
}

int udp_send_multi(int argc, char **argv)
{
    //multicast: FF02::1
    int res;
    sock_udp_ep_t remote = { .family = AF_INET6 };

    if (argc != 3) {
        (void) puts("UDP: Usage - udp <port> <payload>");
        return -1;
    }

    ipv6_addr_set_all_nodes_multicast((ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MCAST_SCP_LINK_LOCAL);

    if (ipv6_addr_is_link_local((ipv6_addr_t *)&remote.addr)) {
        /* choose first interface when address is link local */
        gnrc_netif_t *netif = gnrc_netif_iter(NULL);
        remote.netif = (uint16_t)netif->pid;
    }
    remote.port = atoi(argv[1]);
    if((res = sock_udp_send(NULL, argv[2], strlen(argv[2]), &remote)) < 0) {
        printf("UDP: Error - could not send message \"%s\" to FF01::1\n", argv[2]);
    }
    else {
        printf("UDP: Success - sent %u byte to FF02::1\n", (unsigned)res);
    }
    return 0;
}

int udp_server(int argc, char **argv)
{
    if (argc != 2) {
        puts("MAIN: Usage - udps <thread_pid>");
        return -1;
    }

    if ((server_running == false) &&
        thread_create(server_stack, sizeof(server_stack), THREAD_PRIORITY_MAIN - 1,
                      THREAD_CREATE_STACKTEST, _udp_server, argv[1], "UDP_Server_Thread")
        <= KERNEL_PID_UNDEF) {
        printf("MAIN: Error - failed to start UDP server thread\n");
        return -1;
    }

    return 0;
}
