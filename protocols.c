/*
 * @author  Michael Conard <maconard@mtu.edu>
 *
 * Purpose: Contains the protocol code for leader election/neighbor discovery.
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

#define MAIN_QUEUE_SIZE         (8)
#define MAX_IPC_MESSAGE_SIZE    (256)
#define IPV6_ADDRESS_LEN        (46)
#define MAX_NEIGHBORS           (20)

// External functions defs
extern int ipc_msg_send(char *message, kernel_pid_t destinationPID, bool blocking);
extern int ipc_msg_reply(char *message, msg_t incoming);
extern int ipc_msg_send_receive(char *message, kernel_pid_t destinationPID, msg_t *response, uint16_t type);

// Forward declarations
kernel_pid_t leader_election(int argc, char **argv);
void *_leader_election(void *argv);

// Data structures (i.e. stacks, queues, message structs, etc)
static char protocol_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t _protocol_msg_queue[MAIN_QUEUE_SIZE];
static msg_t msg_in;//, msg_out;

// ************************************
// START MY CUSTOM THREAD DEFS

kernel_pid_t leader_election(int argc, char **argv) {
    if (argc != 2) {
        puts("Usage: leader_election <port>");
        return 0;
    }

    kernel_pid_t protocolPID = thread_create(protocol_stack, sizeof(protocol_stack), THREAD_PRIORITY_MAIN - 2, THREAD_CREATE_STACKTEST, _leader_election, argv[1], "Protocol_Thread");

    printf("MAIN: thread_create(..., protocol_thread) returned: %" PRIkernel_pid "\n", protocolPID);
    if (protocolPID <= KERNEL_PID_UNDEF) {
        (void) puts("MAIN: Error - failed to start leader election thread");
        return 0;
    }

    return protocolPID;
}

void *_leader_election(void *argv) {
    (void)argv;

    msg_init_queue(_protocol_msg_queue, MAIN_QUEUE_SIZE);
    kernel_pid_t udpServerPID = 0;
    char msg_content[MAX_IPC_MESSAGE_SIZE];

    char leader[IPV6_ADDRESS_LEN] = "unknown";

    // array of max 20 neighbors
    //int numNeighbors = 0;
    //char neighbors[MAX_NEIGHBORS][IPV6_ADDRESS_LEN];
    //int neighbors_val[MAX_NEIGHBORS] = { 0 };
    
    uint64_t delayND = 60*1000000; //20sec
    uint64_t delayLE = 60*1000000; //30sec
    uint64_t lastND = 0;
    uint64_t lastLE = 0;
    uint64_t timeToRun;
    uint64_t now;
    
    (void) puts("LE: Success - started protocol thread");
    int res = 0;
    while(1) {
        res = msg_try_receive(&msg_in);
        //printf("LE: after msg_try_receive, code=%d\n", res);
        if (res == 1) {
            if (msg_in.type == 0 && udpServerPID == (kernel_pid_t)0) { // process UDP server PID

                (void) puts("LE: in type==0 block");
                udpServerPID = (kernel_pid_t)(*(int*)msg_in.content.ptr);
                printf("LE: Protocol thread recorded %" PRIkernel_pid " as the UDP server thread's PID\n", udpServerPID);

            } else if (msg_in.type == 1) { // report about the leader

                printf("LE: in type==1 block, content=%s\n", (char*)msg_in.content.ptr);
                printf("LE: replying with msg=%s, size=%u\n", leader, strlen(leader));
                ipc_msg_reply(leader, msg_in);

            } else if (msg_in.type > 1 && msg_in.type < MAX_IPC_MESSAGE_SIZE) { // process string message of size msg_in.type

                printf("LE: in type>1 block, type=%u\n", (uint16_t)msg_in.type);
                strncpy(msg_content, (char*)msg_in.content.ptr, (uint16_t)msg_in.type+1);
                printf("LE: Protocol thread received IPC message: %s from PID=%" PRIkernel_pid "\n", msg_content, msg_in.sender_pid);

            } else {

                (void) puts("LE: Protocol thread received an illegal or too large IPC message");

            }
            memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE);
            //memset(msg_out.content.ptr, 0, MAX_IPC_MESSAGE_SIZE);
        }
        
        now = xtimer_now_usec64();
        timeToRun = lastND + delayND;
        //printf("%ld\n", (long)temp);
        if(now > timeToRun) {
            lastND = xtimer_now_usec64();
            (void) puts("LE: Running neighbor discovery...");
        }

        now = xtimer_now_usec64();
        timeToRun = lastLE + delayLE;
        if(now > timeToRun) {
            lastLE = xtimer_now_usec64();
            (void) puts("LE: Running leader election...");
        }
        
        xtimer_sleep(1); // wait 1 sec
    }

    return 0;
}

// END MY CUSTOM THREAD DEFS
// ************************************
