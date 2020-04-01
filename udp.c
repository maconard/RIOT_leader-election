// Originally taken from: https://github.com/RIOT-OS/Tutorials/tree/master/task-06
// @author Michael Conard
// Purpose: A tutorial UDP file that I have modified to fit my leader election needs.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <msg.h>

#include "net/sock/udp.h"
#include "net/ipv6/addr.h"
#include "thread.h"
#include "xtimer.h"

#define SERVER_MSG_QUEUE_SIZE   (8)
#define SERVER_BUFFER_SIZE      (64)
#define MAX_IPC_MESSAGE_SIZE    (256)

static bool server_running = false;
static sock_udp_t sock;
static char server_buffer[SERVER_BUFFER_SIZE];
static char server_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t server_msg_queue[SERVER_MSG_QUEUE_SIZE];
static msg_t msg_in, msg_out;

const int SERVER_PORT = 3142;

bool runningND = false;
bool runningLE = false;

void *_udp_server(void *args)
{
    printf("UDP: Entered UDP server code\n");
    sock_udp_ep_t server = { .port = SERVER_PORT, .family = AF_INET6 };
    msg_init_queue(server_msg_queue, SERVER_MSG_QUEUE_SIZE);
    kernel_pid_t leaderPID = (kernel_pid_t)(*(int*)args);

    if(sock_udp_create(&sock, &server, NULL, 0) < 0) {
        return NULL;
    }

    server_running = true;
    printf("UDP: Success - started UDP server on port %u\n", server.port);

    char msg_content[MAX_IPC_MESSAGE_SIZE];
    msg_out.type = 0;
    msg_out.content.ptr = (void*)"init";

    bool ipcMsgReceived = false;
    int failCount = 0;
    while (1) {
        if (failCount == 10) {
            printf("UDP: Error - timed out on communicating with protocol thread\n");
            return NULL;
        }

        // wait for protocol thread to initialize    
        int res = msg_send(&msg_out, leaderPID);
        if (res == -1) {
            // msg failed because protocol thread doesn't exist or we have the wrong PID
            printf("UDP: Error - UDP server thread can't communicate with protocol thread\n");
            failCount++;
        } else if (res == 0) {
            // msg failed because protocol thread isn't ready to receive yet
            failCount++;
        } else if (res == 1) {
            // msg succeeded
            printf("UDP: thread successfully initiated communication with the protocol thread\n");
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
            puts("UDP: no UDP data received");
        }
        else {
            server_buffer[res] = '\0';
            printf("UDP: recvd: %s\n", server_buffer);
        }

        // incoming thread message
        memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE * (sizeof msg_content[0]));
        ipcMsgReceived = false;
        msg_try_receive(&msg_in);
        if (msg_in.type > 0 && msg_in.type < MAX_IPC_MESSAGE_SIZE) {
            // process string message of size msg_in.type
            strncpy(msg_content, (char*)msg_in.content.ptr, (uint16_t)msg_in.type);
            ipcMsgReceived = true;
            printf("UDP: received IPC message: %s from %" PRIkernel_pid "\n", msg_content, msg_in.sender_pid);
        } else {
            (void) puts("UPD: received an illegal or too large IPC message");
        }

        // react to IPC message
        if (ipcMsgReceived) {
            
        }
    }

    return NULL;
}

int udp_send(int argc, char **argv)
{
    int res;
    sock_udp_ep_t remote = { .family = AF_INET6 };

    if (argc != 4) {
        puts("Usage: udp <ipv6-addr> <port> <payload>");
        return -1;
    }

    if (ipv6_addr_from_str((ipv6_addr_t *)&remote.addr, argv[1]) == NULL) {
        puts("Error: unable to parse destination address");
        return 1;
    }
    if (ipv6_addr_is_link_local((ipv6_addr_t *)&remote.addr)) {
        /* choose first interface when address is link local */
        gnrc_netif_t *netif = gnrc_netif_iter(NULL);
        remote.netif = (uint16_t)netif->pid;
    }
    remote.port = atoi(argv[2]);
    if((res = sock_udp_send(NULL, argv[3], strlen(argv[3]), &remote)) < 0) {
        puts("could not send");
    }
    else {
        printf("Success: send %u byte to %s\n", (unsigned) res, argv[1]);
    }
    return 0;
}

int udp_send_multi(int argc, char **argv)
{
    int res;
    sock_udp_ep_t remote = { .family = AF_INET6 };

    if (argc != 3) {
        puts("Usage: udp <port> <payload>");
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
        puts("could not send");
    }
    else {
        printf("Success: send %u byte to %s\n", (unsigned) res, "ff02::1");
    }
    return 0;
}

int udp_server(int argc, char **argv)
{
    if (argc != 2) {
        puts("Usage: udps <thread_pid>");
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
