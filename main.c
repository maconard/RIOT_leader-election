/*
 * Original default example:
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * Further edits and final product:
 * @author      Michael Conard <maconard@mtu.edu>
 *
 * Purpose: Apply a leader election protocol to a network on RIOT.
 */

// Standard C includes
#include <stdio.h>
#include <string.h>
#include <msg.h>

// Standard RIOT includes
#include "thread.h"
#include "shell.h"
#include "shell_commands.h"

// Networking includes
#include "net/gnrc/pktdump.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/ndp.h"
#include "net/gnrc/pkt.h"

#define MAIN_QUEUE_SIZE         (8)
#define MAX_IPC_MESSAGE_SIZE    (256)
#define IPV6_ADDRESS_LEN        (46)
#define MAX_NEIGHBORS           (20)

// Data structures (i.e. stacks, queues, message structs, etc)
static char protocol_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static msg_t _protocol_msg_queue[MAIN_QUEUE_SIZE];
static msg_t msg_in, msg_out;


// External functions defs
extern int udp_send(int argc, char **argv);
extern int udp_server(int argc, char **argv);

// Forward declarations
static int hello_world(int argc, char **argv);
static int who_is_leader(int argc, char **argv);

void *leader_election(void *argv);

// State variables
bool running_ND = false;
bool running_LE = false;

kernel_pid_t protocolPID = 0;

// ************************************
// START MY CUSTOM RIOT SHELL COMMANDS

// hello world shell command, prints hello
static int hello_world(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("hello world!\n");

    return 0;
}

// leader election shell command, initiates leader election
static int who_is_leader(int argc, char **argv) {
    (void)argc;
    (void)argv;

    msg_out.type = 1;
    msg_out.content.ptr = (void*)"who_is_leader";

    while (1) {
        msg_send_receive(&msg_out, &msg_in, protocolPID);
        printf("The current leader is: %s\n", (char*)msg_in.content.ptr);
    }

    return 0;
}

// END MY CUSTOM RIOT SHELL COMMANDS
// ************************************

// ************************************
// START MY CUSTOM THREAD DEFS

void *leader_election(void *argv) {
    (void)argv;

    msg_init_queue(_protocol_msg_queue, MAIN_QUEUE_SIZE);
    kernel_pid_t udpServerPID = 0;
    char msg_content[MAX_IPC_MESSAGE_SIZE];

    // array of max 20 neighbors
    /*
    int numNeighbors = 0;
    char neighbors[MAX_NEIGHBORS][IPV6_ADDRESS_LEN];
    int neighbors_val[MAX_NEIGHBORS] = { 0 };
    */
    char leader[IPV6_ADDRESS_LEN] = "unknown";

    /*
    uint64_t delayND = 20*1000000; //20sec
    uint64_t delayLE = 30*1000000; //30sec
    uint64_t lastND = 0;
    uint64_t lastLE = 0;
    uint64_t timeToRun;
    uint64_t now;
    */

    (void) puts("LE: Success - started protocol thread");
    while(1) {
        int res;
        //multicast: FF02::1 ?
        //char *args[] = { "udp_send", "ff02::1", "3141", "disc", NULL };
        //xtimer_sleep(20);

        //printf("LE: top of thread loop\n");
        memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE * (sizeof msg_content[0]));
        res = msg_try_receive(&msg_in);
        //printf("LE: after msg_try_receive\n");
        if (res == 1) {
            if (msg_in.type == 0 && udpServerPID == (kernel_pid_t)0) {
                // process UDP server PID
                printf("LE: in type==0 block\n");
                udpServerPID = (kernel_pid_t)(*(int*)msg_in.content.ptr);
                printf("LE: Protocol thread recorded %" PRIkernel_pid " as the UDP server thread's PID\n", udpServerPID);
            } else if (msg_in.type == 1) {
                // report about the leader
                printf("LE: in type==1 block\n");
                sprintf(msg_content, "%s", leader);
                msg_out.content.ptr = (void*)msg_content;
                msg_out.type = (uint16_t)strlen(msg_content);
                msg_reply(&msg_in, &msg_out);
            } else if (msg_in.type > 1 && msg_in.type < MAX_IPC_MESSAGE_SIZE) {
                // process string message of size msg_in.type
                printf("LE: in type>1 block\n");
                strncpy(msg_content, (char*)msg_in.content.ptr, (uint16_t)msg_in.type);
                printf("LE: Protocol thread received IPC message: %s from %" PRIkernel_pid "\n", msg_content, msg_in.sender_pid);
            } else {
                (void) puts("LE: Protocol thread received an illegal or too large IPC message");
            }
        }
        //printf("LE: bottom of thread loop\n");

        /*
        now = xtimer_now_usec64();
        timeToRun = lastND + delayND;
        //printf("%ld\n", (long)temp);
        if(now > timeToRun) {
            lastND = xtimer_now_usec64();
            printf("  Running neighbor discovery...%ld\n", (long)lastND);
        }

        now = xtimer_now_usec64();
        timeToRun = lastLE + delayLE;
        if(now > timeToRun) {
            lastLE = xtimer_now_usec64();
            printf("  Running leader election...%ld\n", (long)lastLE);
        }
        */
    }

    return 0;
}

// END MY CUSTOM THREAD DEFS
// ************************************

// shell command structure
const shell_command_t shell_commands[] = {
    {"hello", "prints hello world", hello_world},
    {"who_is_leader", "reports who the current leader is", who_is_leader},
    //{ "udp", "send udp packets", udp_send },
    //{ "udps", "start udp server", udp_server },
    { NULL, NULL, NULL }
};

// main method
int main(void)
{
    // initialize networking and packet tools
    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL, gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    // start ipv6 support thread
/*
    int ipv6_thread = gnrc_ipv6_init();
    if(ipv6_thread == EOVERFLOW || ipv6_thread == EEXIST) {
        (void) puts("MAIN: Error - failed to start ipv6 thread");
        return -1;
    }
*/

    // start my protocol thread
    printf("MAIN: Trying to launch protocol thread\n");
    protocolPID = thread_create(protocol_stack, sizeof(protocol_stack), THREAD_PRIORITY_MAIN - 2,
                                    THREAD_CREATE_STACKTEST, leader_election, (void*)3141, "Protocol_Thread");
    printf("MAIN: thread_create(..., protocol_thread) returned: %" PRIkernel_pid "\n", protocolPID);
    if (protocolPID <= KERNEL_PID_UNDEF) {
        (void) puts("MAIN: Error - failed to start leader election thread");
        return -1;
    }
    printf("MAIN: Lauched protocol thread\n");

    // start internal UDP server
    printf("MAIN: Trying to write PID\n");
    char pidBuf[16];
    sprintf(pidBuf, "%u", (uint16_t)protocolPID);
    char *args[] = { "udp_server", pidBuf, NULL };
    
    printf("MAIN: Trying to launch UDP server thread\n");
    if (udp_server(2, args) == -1) {
        (void) puts("MAIN: Error - failed to start UDP server thread");
    }

    (void) puts("MAIN: Welcome to RIOT!");

    // start the RIOT shell for this node
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
