/*
 * @author  Michael Conard <maconard@mtu.edu>
 *
 * Purpose: Contains the protocol code for leader election/neighbor discovery.
 */

#define ENABLE_LEADER (1)

// Standard C includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <msg.h>

// Standard RIOT includes
#include "thread.h"
#include "xtimer.h"
#include "random.h"

#define MAIN_QUEUE_SIZE         (32)
#define MAX_IPC_MESSAGE_SIZE    (256)
#define IPV6_ADDRESS_LEN        (46)
#define MAX_NEIGHBORS           (20)

// Leader Election values
#define K 	(3)
#define T1	(8*1000000)
#define T2	(6*1000000)

// External functions defs
extern int ipc_msg_send(char *message, kernel_pid_t destinationPID, bool blocking);
extern int ipc_msg_reply(char *message, msg_t incoming);
extern int ipc_msg_send_receive(char *message, kernel_pid_t destinationPID, msg_t *response, uint16_t type);

// Forward declarations
kernel_pid_t leader_election(int argc, char **argv);
void *_leader_election(void *argv);
void substr(char *s, int a, int b, char *t);

// Data structures (i.e. stacks, queues, message structs, etc)
static char protocol_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t _protocol_msg_queue[MAIN_QUEUE_SIZE];
static msg_t msg_in;//, msg_out;

// into t from s starting at a for length b
void substr(char *s, int a, int b, char *t) 
{
    memset(t, 0, b);
    strncpy(t, s+a, b);
}

int alreadyANeighbor(char **neighbors, char *ipv6) {
    for(int i = 0; i < MAX_NEIGHBORS; i++) {
        if(strcmp(neighbors[i], ipv6) == 0) return 1;
    }
    return 0;
}

int getNeighborIndex(char **neighbors, char *ipv6) {
    for(int i = 0; i < MAX_NEIGHBORS; i++) {
        if(strcmp(neighbors[i], ipv6) == 0) return i;
    }
    return -1;
}

void msgAllNeighbors(uint32_t min, char *leader, char *me) {
    char msg[MAX_IPC_MESSAGE_SIZE] = "le_ack:"; 
    char neighborM[4] = { 0 };                   
    if(min < 10) {
        sprintf(neighborM, "00%d",min);
    } else if (min < 100) {
        sprintf(neighborM, "0%d",min);
    } else {
        sprintf(neighborM, "%d",min);
    }
    strcat(msg,neighborM);
    strcat(msg,":");
    strcat(msg,leader);
    strcat(msg,";");
    strcat(msg,me);
    ipc_msg_send(msg, udpServerPID, false);
    printf("LE: sent out new minimum info, %s and %s\n", neighborM, leader);
}

int indexOfSemi(char *ipv6) {
	for (int i = 0; i < strlen(ipv6); i++) {
		if (ipv6[i]  == ';') {
			return i+1; // start of second id
		}
	}
	return -1;
}

// return -1 if a<b, 1 if a>b, 0 if a==b
int minIPv6(char *ipv6_a, char *ipv6_b) {
	int minLength = strlen(ipv6_a);
	if (strlen(ipv6_b) < minLength) minLength = strlen(ipv6_b);
	
	for (int i = 0; i < minLength; i++) {
		if (ipv6_a[i] < ipv6_b[i]) {
			return -1;
		} else if (ipv6_b[i] < ipv6_a[i]) {
			return 1;
		}
	}
	return 0;
}

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

    char ipv6[IPV6_ADDRESS_LEN] = { 0 };
    char ipv6_2[IPV6_ADDRESS_LEN] = { 0 };
    char myIPv6[IPV6_ADDRESS_LEN] = { 0 };
    char initND[8] = "nd_init";
    char initLE[8] = "le_init";
    char neighborM[4] = { 0 };

    // array of MAX neighbors
    int numNeighbors = 0;
    char **neighbors = (char**)calloc(MAX_NEIGHBORS, sizeof(char*));
    for(int i = 0; i < MAX_NEIGHBORS; i++) {
        neighbors[i] = (char*)calloc(IPV6_ADDRESS_LEN, sizeof(char));
    }
    uint32_t neighborsVal[MAX_NEIGHBORS] = { 0 };

    random_init(xtimer_now_usec());  
    
    uint64_t delayND = 60*1000000; //60sec
    uint64_t delayLE = 45*1000000; //45sec
    uint64_t lastND = 0;
    uint64_t lastLE = xtimer_now_usec64();
    uint64_t timeToRun;
    uint64_t now;
    uint64_t startTimeLE = 0;
    uint64_t endTimeLE = 0;
    uint64_t convergenceTimeLE;
    bool hasElectedLeader = false;
    bool runningND = false;
    bool runningLE = false;
    bool allowLE = false;
    int stateND = 0;
    int stateLE = 0;
    int countedMs = 0;
    int discoveryCount = 0;
    bool quit = false;
	bool completeND = false;

	// Ali's LE variabnles
	int counter = K; //k
    uint32_t m = (random_uint32() % 254)+1; // my leader election value, range 1 to 255
    uint32_t min = m;                       // the min of my neighborhood 
	uint32_t tempMin = 257;
    char leader[IPV6_ADDRESS_LEN] = "unknown";	// the "leader so far"
    char tempLeader[IPV6_ADDRESS_LEN];	// temp leader for a round of communication
	uint64_t t1 = T1;
	uint64_t t2 = T2;
    uint64_t lastT1 = 0;
    uint64_t lastT2 = 0;
    
    printf("LE: Success - started protocol thread with m=%d\n", m);
    int res = 0;
    while (1) {
        if (quit) break;
        // process messages
        memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE);
        res = msg_try_receive(&msg_in);
        //printf("LE: after msg_try_receive, code=%d\n", res);
        if (res == 1) {
            if (msg_in.type == 0 && udpServerPID == (kernel_pid_t)0) { // process UDP server PID

                //(void) puts("LE: in type==0 block");
                udpServerPID = *(kernel_pid_t*)msg_in.content.ptr;
                printf("LE: Protocol thread recorded %" PRIkernel_pid " as the UDP server thread's PID\n", udpServerPID);
                continue;

            } else if (msg_in.type == 1 && strlen(myIPv6) == 0) { // process my IPv6

                //(void) puts("LE: in type==1 block");
                strncpy(myIPv6, (char*)msg_in.content.ptr, strlen((char*)msg_in.content.ptr)+1);
                strcpy(leader, myIPv6);
                printf("LE: Protocol thread recorded %s as it's IPv6\n", leader);
                allowLE = true;
                continue;

            } else if (msg_in.type == 2) { // report about the leader

                printf("LE: in type==2 block, content=%s\n", (char*)msg_in.content.ptr);
                printf("LE: replying with msg=%s, size=%u\n", leader, strlen(leader));
                ipc_msg_reply(leader, msg_in);
                continue;

            } else if (msg_in.type > 2 && msg_in.type < MAX_IPC_MESSAGE_SIZE) { // process string message of size msg_in.type

                //printf("LE: in type>2 block, type=%u\n", (uint16_t)msg_in.type);
                strncpy(msg_content, (char*)msg_in.content.ptr, (uint16_t)msg_in.type+1);
                printf("LE: Protocol thread received IPC message: %s from PID=%" PRIkernel_pid "\n", msg_content, msg_in.sender_pid);

            } else {

                (void) puts("LE: Protocol thread received an illegal or too large IPC message");
                continue;

            }
        }

        // react to input
        if (res == 1) {
            if (numNeighbors < MAX_NEIGHBORS && strncmp(msg_content, "nd_ack:", 7) == 0) {
                // a discovered neighbor has responded
                substr(msg_content, 7, IPV6_ADDRESS_LEN, ipv6); // obtain neighbor's ID
                if (!alreadyANeighbor(neighbors, ipv6)) {
                    strcpy(neighbors[numNeighbors], ipv6); // record their ID
                    printf("**********\nLE: recorded new neighbor, %s\n**********\n", (char*)neighbors[numNeighbors]);
                    numNeighbors++;

                    char msg[MAX_IPC_MESSAGE_SIZE] = "nd_hello:";
                    strcat(msg, ipv6);
                    ipc_msg_send(msg, udpServerPID, false);

                    lastND = xtimer_now_usec64();
                } else {
                    printf("LE: Hi %s, we've already met\n", ipv6);
                }
            } else if (numNeighbors > 0 && strncmp(msg_content, "le_ack:", 7) == 0) {
                // a neighbor has responded
                substr(msg_content, 7, 3, neighborM); // obtain m value
                substr(msg_content, 11, IPV6_ADDRESS_LEN, ipv6); // obtain ID
				int j = indexOfSemi(msg_content);
				substr(msg_content, j, IPV6_ADDRESS_LEN, ipv6_2); // obtain neighbor ID
                int i = getNeighborIndex(neighbors, ipv6_2);
                if (atoi(neighborM) <= 0) continue;
                printf("LE: new m value %d from %s, id=%s\n", atoi(neighborM), ipv6_2, ipv6);
                if (neighborsVal[i] == 0) countedMs++;
                neighborsVal[i] = (uint32_t)atoi(neighborM);
                if (neighborsVal[i] < tempMin) {
                    strcpy(tempLeader, ipv6);
                    tempMin = neighborsVal[i];
                    printf("LE: new tempMin=%d, tempLeader %s\n", tempMin, tempLeader);
                } 
                lastLE = xtimer_now_usec64();
            } else if (numNeighbors > 0 && strncmp(msg_content, "le_m?:", 6) == 0) {
                // someone wants my m
                msgAllNeighbors(min, leader, myIPv6);
            }
        }

        xtimer_usleep(100000); // wait 0.1 seconds
        if (udpServerPID == 0) continue;
        
        // neighbor discovery
        if (!runningND && !completeND) {
            // check if it's time to run, then initialize
            now = xtimer_now_usec64();
            if (discoveryCount == 0) {
                timeToRun = delayND/3;
            } else {
                timeToRun = lastND + delayND;
            }
            if (now > timeToRun) {
                lastND = xtimer_now_usec64();
                (void) puts("LE: Running neighbor discovery...");
                runningND = true;
                discoveryCount += 1;
				completeND = true;
            }
        } else if (runningND) {
            // perform neighbor discovery
            switch(stateND) {   // check what state of neighbor discovery we are in
                case 0 : // case 0: send out multicast ping
                    ipc_msg_send(initND, udpServerPID, false);
                    stateND = 1;
                    break;
                case 1 : // case 1: listen for acknowledgement
                    if (lastND < xtimer_now_usec64() - 8000000) {
                        // if no acks for 8 seconds, terminate
                        stateND = 0;
                        if (numNeighbors > 0) {
                            runningND = false;
                            lastND = xtimer_now_usec64();
                        }
                    }
                    break;
                default:
                    printf("LE: neighbor discovery in invalid state %d\n", stateND);
                    break;
            }
        }   

        if (!runningLE && !hasElectedLeader) {
            // check if it's time to run, then initialize
            now = xtimer_now_usec64();
            timeToRun = lastLE + delayLE;
            if (ENABLE_LEADER == 1 && !hasElectedLeader && allowLE && now > timeToRun) {
                lastLE = xtimer_now_usec64();
                (void) puts("LE: Running leader election...");
                runningLE = true;
                allowLE = false;
                startTimeLE = (uint64_t)(xtimer_now_usec64()/1000000);
				counter = K;
				stateLE = 0;
            }
        } else if (runningLE) {
            // perform leader election
            switch(stateLE) {   // check what state of leader election we are in
                case 0 : // case 0: send out multicast ping
					printf("LE: case 0, leader=%s, min=%d\n", leader, min);
                    ipc_msg_send(initLE, udpServerPID, false);
                    stateLE = 1;
                    countedMs = 0;
					lastT2 = xtimer_now_usec64();
                    break;
                case 1 : // case 1: line 4 of psuedocode
                    if (countedMs == numNeighbors || lastT2 < xtimer_now_usec64() - t2) {
						printf("LE: case 1, leader=%s, min=%d, heardFrom=%d\n", leader, min, countedMs);
                        stateLE = 2;
                        lastT2 = xtimer_now_usec64();
						tempMin = 257;
						countedMs = 0;
						for (int i = 0; i < MAX_NEIGHBORS; i++) {
							neighborsVal[i] = 0;
						}
                    }
                    break;
				case 2 : // case 2: line 5 of pseudocode
					if (lastT1 < xtimer_now_usec64() - t1) {
						printf("LE: case 2, leader=%s, min=%d, counter=%d\n", leader, min, counter);
						stateLE = 3;
		                lastT2 = xtimer_now_usec64();
						lastT1 = xtimer_now_usec64();
					}
					break;
				case 3 : // case 3: lines 5a-f of pseudocode, some contained in response above
					if (countedMs == numNeighbors || lastT2 < xtimer_now_usec64() - t2) {
						printf("LE: case 3, leader=%s, min=%d, heardFrom=%d, counter=%d\n", leader, min, countedMs, counter);
						
						if (tempMin < min) {
							printf("LE: case 3, tempMin=%d < min=%d\n", tempMin, min);
							min = tempMin;
							strcpy(leader, tempLeader);
							counter = K;
						} else if (tempMin == min && counter > 0) {
							printf("LE: case 3, tempMin=%d == min=%d\n", tempMin, min);
							counter = counter - 1;
							int tie = minIPv6(leader, tempLeader);
							if (tie == 1) {
								// new leader wins tie
								strcpy(leader, tempLeader);
								printf("LE: tempLeader wins tie\n");
							} else {
								// else the old leader won the tie, so no change
								printf("LE: existing leader wins tie\n");
							}
						} else if (counter == 0) {
							printf("LE case 3, counter == 0 so quit\n");
							stateLE = 5;
							break;
						}

						tempMin = 257;
						countedMs = 0;
						for (int i = 0; i < MAX_NEIGHBORS; i++) {
							neighborsVal[i] = 0;
						}

						msgAllNeighbors(min, leader, myIPv6);
						stateLE = 2;
					}

					// go back to line 5 of pseudocode
					//if (lastT1 < xtimer_now_usec64() - t1) {
					//	stateLE = 2;
					//}
					break;
                case 5 :
                    printf("LE: %s elected as the leader, via m=%u!\n", leader, min);
                    if (strcmp(leader, myIPv6) == 0) {
                        printf("LE: Hey, that's me! I'm the leader!\n");
                    }
                    endTimeLE = (uint64_t)(xtimer_now_usec64()/1000000);
                    convergenceTimeLE = endTimeLE - startTimeLE;
                    printf("LE: leader election took %" PRIu64 " seconds to converge\n", convergenceTimeLE);
                    runningLE = false;
                    hasElectedLeader = true;
                    countedMs = 0;
                    stateLE = 0;
                    lastLE = xtimer_now_usec64();
                    quit = true;
                    break;
                default:
                    printf("LE: neighbor discovery in invalid state %d\n", stateND);
                    return 0;
                    break;
            }
        }
        
        xtimer_usleep(100000); // wait 0.1 seconds
    }

    for(int i = 0; i < MAX_NEIGHBORS; i++) {
        free(neighbors[i]);
    }
    free(neighbors);

    // mini loop that just stays up to report the leader
    while (1) {
        memset(msg_content, 0, MAX_IPC_MESSAGE_SIZE);
        res = msg_try_receive(&msg_in);
        if (res == 1) {
            if (msg_in.type == 2) { // report about the leader

                printf("LE: reporting that the leader is %s\n", leader);
                ipc_msg_reply(leader, msg_in);
                continue;

            }
        }
    }

    return 0;
}

// END MY CUSTOM THREAD DEFS
// ************************************
