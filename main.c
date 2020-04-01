/*
 * @author  Michael Conard <maconard@mtu.edu>
 *
 * Purpose: Apply a leader election protocol to a network on RIOT.
 */

// Standard C includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <msg.h>

// Standard RIOT includes
#include "thread.h"
#include "shell.h"
#include "shell_commands.h"
#include "xtimer.h"

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
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static msg_t msg_in, msg_out;

// External functions defs
extern int udp_send(int argc, char **argv);
extern int udp_server(int argc, char **argv);
extern kernel_pid_t leader_election(int argc, char **argv);

// Forward declarations
static int hello_world(int argc, char **argv);
static int who_is_leader(int argc, char **argv);
static int run(int argc, char **argv);

// State variables
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

// query who the leader is from the shell
static int who_is_leader(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char msg[14] = "who_is_leader";
    char leader[IPV6_ADDRESS_LEN];

    msg_out.type = 1;
    msg_out.content.ptr = &msg;

    msg_send_receive(&msg_out, &msg_in, protocolPID);
    sprintf(leader, "%s", (char*)msg_in.content.ptr);
    printf("MAIN: The current leader is: %s\n", leader);

    // test msg
    msg_out.content.ptr = &msg;
    msg_out.type = strlen(msg);
    msg_send(&msg_in, protocolPID);

    return 0;
}

// END MY CUSTOM RIOT SHELL COMMANDS
// ************************************

// shell command structure
const shell_command_t shell_commands[] = {
    {"hello", "prints hello world", hello_world},
    {"leader", "reports who the current leader is", who_is_leader},
    { NULL, NULL, NULL }
};

// initiates leader election
static int run(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // start ipv6 support thread
    (void) puts("MAIN: Trying to start IPv6 thread");
    int ipv6_thread = gnrc_ipv6_init();
    if(ipv6_thread == EOVERFLOW || ipv6_thread == EEXIST) {
        (void) puts("MAIN: Error - failed to start ipv6 thread");
        return -1;
    }
    (void) puts("MAIN: Launched IPv6 thread");

    // start my protocol thread
    (void) puts("MAIN: Trying to start protocol thread");
    char *argsLE[] = { "leader_election", "3141", NULL };
    
    protocolPID = leader_election(2, argsLE);
    if (protocolPID == 0) {
        (void) puts("MAIN: Error - failed to start protocol thread");
        return -1;
    }
    printf("MAIN: Lauched protocol thread, PID=%" PRIkernel_pid "\n", protocolPID);

    // start internal UDP server
    (void) puts("MAIN: Trying to launch UDP server thread");
    char pidBuf[16];
    sprintf(pidBuf, "%" PRIkernel_pid "", protocolPID);
    char *argsUDP[] = { "udp_server", pidBuf, NULL };
    
    if (udp_server(2, argsUDP) == -1) {
        (void) puts("MAIN: Error - failed to start UDP server thread");
    }
    (void) puts("MAIN: Launched UDP server thread");

    running_LE = true;

    return 0;
}

// main method
int main(void)
{
    // initialize networking and packet tools
    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL, gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    run(0, NULL);

    (void) puts("MAIN: Welcome to RIOT!");

    // start the RIOT shell for this node
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
