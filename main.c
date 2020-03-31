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

#define MAIN_QUEUE_SIZE     (8)

// Data structures (i.e. stacks, queues, etc)
//static char leader_election_stack[THREAD_STACKSIZE_DEFAULT];=
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

// External functions defs
extern int udp_send(int argc, char **argv);
extern int udp_server(int argc, char **argv);

// Forward declarations
static int hello_world(int argc, char **argv);
static int elect_leader(int argc, char **argv);

//void *leader_election(void *argv);

// State variables
bool running_ND = false;
bool running_LE = false;

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
static int elect_leader(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int cmp;

    if (argc <= 1) {
        puts("Usage: elect_leader <start | stop>");
        return -1;
    }

    cmp = strcmp(argv[1],"start");
    if(cmp == 0 && running_ND == false) {
        // launch leader election thread
        puts("Initiating leader election");

        // start internal UDP server
        char *args[] = { "udp_server", "3141", NULL };
        udp_server(2, args);

        running_LE = true;
    } else if(cmp == 0) {
        puts("Leader election is already running!");
    }

    cmp = strcmp(argv[1],"stop");
    if(cmp == 0 && running_ND == true) {
        //running_LE = false;
        puts("Can't stop leader election...");
        // TODO
    } else if(cmp == 0) {
        puts("Leader election isn't running!");
    }

    return 0;
}

// END MY CUSTOM RIOT SHELL COMMANDS
// ************************************

// ************************************
// START MY CUSTOM THREAD DEFS

/*
void *leader_election(void *argv) {
    (void)argv;

    while(1) {
        //multicast: FF02::1 ?
        puts("Running leader election...");
        //char *args[] = { "udp_send", "ff02::1", "3141", "disc", NULL };
        xtimer_sleep(20);

    }

    return 0;
}
*/

// END MY CUSTOM THREAD DEFS
// ************************************

// shell command structure
const shell_command_t shell_commands[] = {
    {"hello", "prints hello world", hello_world},
    {"elect_leader", "launches leader election protocol", elect_leader},
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
    int ipv6_thread = gnrc_ipv6_init();
    if(ipv6_thread == EOVERFLOW || ipv6_thread == EEXIST) {
        (void) puts("Error initalizing ipv6 thread");
    }

    (void) puts("Welcome to RIOT!");

    // start the RIOT shell for this node
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
