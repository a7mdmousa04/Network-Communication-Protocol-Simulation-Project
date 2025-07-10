/* -----------------------RTOS Project-----------------------------
    Description:
 *   Implements a FreeRTOS-based simulation of a lossy communication network
 *   using the Send-and-Wait and Go-Back-N protocols. Two sender tasks generate
 *   packets with random lengths and destinations; a switch task simulates packet
 *   drops and delays; a delay manager task enforces propagation/transmission
 *   delays; and two receiver tasks process incoming packets, detect losses,
 *   handle out-of-order delivery, and send acknowledgments.
 * ----------------Example of output ---------------------*
 * ================= SIMULATION COMPLETE =================
        All receivers have received 2000 packets each
        Simulation time: 173189 ms
        P_DROP: 0.040
        Timeout (TOUT_MS): 175 ms
        Window Size (DEFAULT_WINDOW_SIZE): 1
        Total packets received: 4010
        Sender 1: Generated=2005, Transmitted=2005, Retransmitted=100, ACKs=2003 ,Dropped(Max retry)= 0
        Sender 2: Generated=2007, Transmitted=2007, Retransmitted=111, ACKs=2006 ,Dropped(Max retry)= 0
        Receiver 3: Received=2010, Out-of-order=0, Duplicate=23, ACKs sent=2033
        - From Sender 1: 1009 packets
        - From Sender 2: 1001 packets
        Receiver 4: Received=2000, Out-of-order=0, Duplicate=19, ACKs sent=2019
        - From Sender 1: 994 packets
        - From Sender 2: 1006 packets
        Total data bytes received: 1982280 bytes
        Effective throughput: 11445 bytes/sec
        Effective throughput: 11 kbytes/sec
        =====================================================
        [SWITCH] Final Stats - Total: 8275, Data Dropped: 169, ACK Dropped: 42
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
/* =================== Parameter Definitions =================== */
#define NUM_SENDERS 2
#define NUM_RECEIVERS 2
// Packet parametres (from project spec)
#define L1 500
#define L2 1500
#define H 8 // header size: sender(1)+dest(1)+seq(4)+len(2)
#define K 40 // ACK size
#define C 100000 // 100 kbits/sec
#define D_MS 5 // propogation delay ms
#define P_DROP 0.04 // Packet drop probability { 0.02 , 0.04 ,0.08 }
#define P_ACK 0.01 // ACK drop probability
#define T1_MS 100 // Min packet interval (0.1 sec = 100ms)
#define T2_MS 200 // Max packet interval (0.2 sec = 200ms)
#define TOUT_MS 150 // Timeout as per project spec {150 ,175 ,200 }
#define MAX_RETRANSMISSIONS 4
#define MAX_PACKETS_TO_RECEIVE 2000
// Go-Back-N parametres (now configurable)
#define DEFAULT_WINDOW_SIZE 1 //{1 ,2 ,4 ,8 } when N = 1 this is the Send and Wait protocol (S&W)
#define MAX_SEQUENCE_NUM 4294967295U // 32-bit max
// Queue sizes
#define SWITCH_QUEUE_SIZE 20
#define DELAY_QUEUE_SIZE 30
#define RECEIVER_QUEUE_SIZE 15
#define ACK_QUEUE_SIZE 15
// Task stack sizes
#define SENDER_TASK_STACK_SIZE 1024
#define SENDER_TASK_PRIORITY 2
#define SWITCH_TASK_STACK_SIZE 1024
#define SWITCH_TASK_PRIORITY 3
#define DELAY_MGR_TASK_STACK_SIZE 1024
#define DELAY_MGR_TASK_PRIORITY 4
#define RECEIVER_TASK_STACK_SIZE 1024
#define RECEIVER_TASK_PRIORITY 2
#define PRINTF_BUFFER_SIZE 128
/* Packet Types */
typedef enum {
    PACKET_TYPE_DATA = 0,
    PACKET_TYPE_ACK = 1
} PacketType;
/* Simplified packet structure */
typedef struct {
    uint8_t sender_id;
    uint8_t destination;
    uint32_t sequence_number;
    uint16_t length;
    uint8_t packet_type; // 0 = data, 1 = ACK
} Packet;
/* Transmission Buffer Entry */
typedef struct {
    uint32_t sequence_number;
    uint8_t retransmission_count;
    TickType_t timeout_time;
    uint8_t in_use;
} TransmissionBufferEntry;
/* Sender State */
typedef struct {
    uint32_t next_seq_num[NUM_RECEIVERS];
    uint32_t base_seq_num[NUM_RECEIVERS];
    TransmissionBufferEntry tx_buffer[NUM_RECEIVERS][DEFAULT_WINDOW_SIZE];
    uint32_t packets_generated;
    uint32_t packets_transmitted;
    uint32_t packets_retransmitted;
    uint32_t packets_dropped_max_retrans;
    uint32_t acks_received;
    SemaphoreHandle_t buffer_mutex;
    uint8_t window_size;
    TickType_t last_timeout_check[NUM_RECEIVERS];
} SenderState;
/* Receiver State */
typedef struct {
    uint32_t expected_seq_num[NUM_SENDERS];
    uint32_t packets_received;
    uint32_t packets_out_of_order;
    uint32_t packets_duplicate;
    uint32_t acks_sent;
    uint32_t packets_received_from_sender[NUM_SENDERS]; // NEW: Track packets from each sender
    uint64_t total_bytes_received;
} ReceiverState;
/* Delay Queue Item structure */
typedef struct {
    Packet packet;
    TickType_t arrival_time;
    uint32_t tx_delay_ms;
} DelayQueueItem;
/* Global Variables */
static SenderState senderStates[NUM_SENDERS];
static ReceiverState receiverStates[NUM_RECEIVERS];
static uint32_t total_packets_received = 0;
static SemaphoreHandle_t terminationSemaphore;
static SemaphoreHandle_t stats_mutex;
static volatile uint8_t should_terminate = 0;
static TickType_t simulation_start_time;
/* Queues */
static QueueHandle_t switchQueue;
static QueueHandle_t delayQueue;
static QueueHandle_t receiverQueues[NUM_RECEIVERS];
static QueueHandle_t ackQueues[NUM_SENDERS];
/* Function prototypes */
static void senderTask(void* param);
static void switchTask(void* param);
static void delayManagerTask(void* param);
static void receiverTask(void* param);
static void checkTimeouts(uint8_t sender_id);
static void transmitPacket(uint8_t sender_id, uint8_t receiver_idx, uint32_t seq_num);
static void processAck(uint8_t sender_id, Packet* ack);
static void retransmitWindow(uint8_t sender_id, uint8_t receiver_idx);
/* Safe print function */
static void safe_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[PRINTF_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), format, args);
    printf("%s", buffer);
    va_end(args);
}
/* Helper functions */
static uint16_t random_packet_length(void) {
    // Give random packet lenght between L1 and L2
    return L1 + (rand() % (L2 - L1 + 1));
}
static uint32_t random_interval(void) {
    // Give random interval between T1_MS and T2_MS
    return T1_MS + (rand() % (T2_MS - T1_MS + 1));
}
static uint32_t transmission_delay(uint16_t len) {
    // Calculate how much time to send packet
    return (len * 8 * 1000) / C; // ms
}
static uint8_t is_in_window(uint32_t seq_num, uint32_t base, uint32_t window_size) {
    // Check if seq num is inside the window
    return (seq_num >= base && seq_num < base + window_size);
}
/* Process received ACK  */
static void processAck(uint8_t sender_id, Packet* ack) {
    // Handle ACK packets for the sender
    SenderState* state = &senderStates[sender_id - 1];
    uint8_t receiver_idx = ack->sender_id - 3;
    if (receiver_idx >= NUM_RECEIVERS) return;
    if (xSemaphoreTake(state->buffer_mutex, pdMS_TO_TICKS(10)) == pdPASS) {
        uint32_t ack_seq = ack->sequence_number;
        // Go-Back-N: ACK is cummulative, acknowledges all packets up to ack_seq
        if (ack_seq >= state->base_seq_num[receiver_idx]) {
            // Clear all buffer entrys for packets up to and include ack_seq
            while (state->base_seq_num[receiver_idx] <= ack_seq) {
                uint8_t buffer_idx = state->base_seq_num[receiver_idx] % state->window_size;
                TransmissionBufferEntry* entry = &state->tx_buffer[receiver_idx][buffer_idx];
                if (entry->in_use && entry->sequence_number == state->base_seq_num[receiver_idx]) {
                    entry->in_use = 0;
                    state->acks_received++;
                    safe_printf("[SENDER %d] ACK received for packet #%lu from Receiver %d\n",
                        sender_id, state->base_seq_num[receiver_idx], receiver_idx + 3);
                }
                state->base_seq_num[receiver_idx]++;
            }
        }
        xSemaphoreGive(state->buffer_mutex);
    }
}
/* Check for timeouts and handle retransmissions */
static void checkTimeouts(uint8_t sender_id) {
    // Check if any packet time out and need to be resent
    SenderState* state = &senderStates[sender_id - 1];
    TickType_t current_time = xTaskGetTickCount();
    if (xSemaphoreTake(state->buffer_mutex, pdMS_TO_TICKS(10)) == pdPASS) {
        for (int receiver_idx = 0; receiver_idx < NUM_RECEIVERS; receiver_idx++) {
            // Only check timeout for the oldest not acked packet (base of window)
            if (state->base_seq_num[receiver_idx] < state->next_seq_num[receiver_idx]) {
                uint8_t base_buffer_idx = state->base_seq_num[receiver_idx] % state->window_size;
                TransmissionBufferEntry* base_entry = &state->tx_buffer[receiver_idx][base_buffer_idx];
                if (base_entry->in_use &&
                    base_entry->sequence_number == state->base_seq_num[receiver_idx] &&
                    current_time >= base_entry->timeout_time) {
                    base_entry->retransmission_count++;
                    if (base_entry->retransmission_count >= MAX_RETRANSMISSIONS) {
                        // Drop the packet and slide the window
                        state->packets_dropped_max_retrans++;
                        safe_printf("[SENDER %d] Packet #%lu dropped after %d retransmissions\n",
                            sender_id, base_entry->sequence_number, MAX_RETRANSMISSIONS);
                        base_entry->in_use = 0;
                        state->base_seq_num[receiver_idx]++;
                    }
                    else {
                        // Resend all packets in the current window
                        safe_printf("[SENDER %d] Timeout for packet #%lu, retransmit window\n",
                            sender_id, base_entry->sequence_number);
                        retransmitWindow(sender_id, receiver_idx);
                        // Reset timeout for all packets in window
                        for (int i = 0; i < state->window_size; i++) {
                            TransmissionBufferEntry* entry = &state->tx_buffer[receiver_idx][i];
                            if (entry->in_use &&
                                is_in_window(entry->sequence_number, state->base_seq_num[receiver_idx], state->window_size)) {
                                entry->timeout_time = current_time + pdMS_TO_TICKS(TOUT_MS);
                            }
                        }
                    }
                }
            }
        }
        xSemaphoreGive(state->buffer_mutex);
    }
}
/* Retransmit all packets in the window */
static void retransmitWindow(uint8_t sender_id, uint8_t receiver_idx) {
    // Resend all packets that still in the window
    SenderState* state = &senderStates[sender_id - 1];
    for (uint32_t seq = state->base_seq_num[receiver_idx];
        seq < state->next_seq_num[receiver_idx] &&
        seq < state->base_seq_num[receiver_idx] + state->window_size;
        seq++) {
        uint8_t buffer_idx = seq % state->window_size;
        TransmissionBufferEntry* entry = &state->tx_buffer[receiver_idx][buffer_idx];
        if (entry->in_use && entry->sequence_number == seq) {
            Packet pkt;
            pkt.sender_id = sender_id;
            pkt.destination = receiver_idx + 3;
            pkt.sequence_number = seq;
            pkt.length = random_packet_length();
            pkt.packet_type = PACKET_TYPE_DATA;
            if (xQueueSend(switchQueue, &pkt, pdMS_TO_TICKS(10)) == pdPASS) {
                state->packets_retransmitted++;
                safe_printf("[SENDER %d] Retransmit packet #%lu to Receiver %d\n",
                    sender_id, seq, pkt.destination);
            }
        }
    }
}
/* Transmit a new packet  */
static void transmitPacket(uint8_t sender_id, uint8_t receiver_idx, uint32_t seq_num) {
    // Send a new packet if window is not full
    SenderState* state = &senderStates[sender_id - 1];
    if (seq_num >= state->base_seq_num[receiver_idx] + state->window_size) {
        return;
    }
    uint8_t buffer_idx = seq_num % state->window_size;
    TransmissionBufferEntry* entry = &state->tx_buffer[receiver_idx][buffer_idx];
    // Don't overwrite exist entry
    if (entry->in_use) {
        return;
    }
    Packet pkt;
    pkt.sender_id = sender_id;
    pkt.destination = receiver_idx + 3;
    pkt.sequence_number = seq_num;
    pkt.length = random_packet_length();
    pkt.packet_type = PACKET_TYPE_DATA;
    // Set up transmission buffer entry
    entry->sequence_number = seq_num;
    entry->retransmission_count = 0;
    entry->timeout_time = xTaskGetTickCount() + pdMS_TO_TICKS(TOUT_MS);
    entry->in_use = 1;
    if (xQueueSend(switchQueue, &pkt, pdMS_TO_TICKS(10)) == pdPASS) {
        state->packets_transmitted++;
        safe_printf("[SENDER %d] Sent packet #%lu to Receiver %d (%d bytes)\n",
            sender_id, seq_num, pkt.destination, pkt.length);
    }
    else {
        // Failed to send, mark as not in use
        entry->in_use = 0;
    }
}
/* Sender Task */
static void senderTask(void* param) {
    // This is main function for each sender
    uint8_t sender_id = (uint32_t)param;
    SenderState* state = &senderStates[sender_id - 1];
    Packet ack;
    TickType_t last_packet_time = 0;
    uint32_t packet_interval = random_interval();
    while (!should_terminate) {
        // Process ACKs
        while (xQueueReceive(ackQueues[sender_id - 1], &ack, 0) == pdPASS) {
            processAck(sender_id, &ack);
        }
        // Check timeouts sometimes
        checkTimeouts(sender_id);
        // Make new packets at some interval
        TickType_t current_time = xTaskGetTickCount();
        if (current_time - last_packet_time >= pdMS_TO_TICKS(packet_interval)) {
            for (int receiver_idx = 0; receiver_idx < NUM_RECEIVERS; receiver_idx++) {
                if (xSemaphoreTake(state->buffer_mutex, pdMS_TO_TICKS(5)) == pdPASS) {
                    // Check if we can send in the window
                    if (state->next_seq_num[receiver_idx] < state->base_seq_num[receiver_idx] + state->window_size) {
                        transmitPacket(sender_id, receiver_idx, state->next_seq_num[receiver_idx]);
                        state->next_seq_num[receiver_idx]++;
                        state->packets_generated++;
                    }
                    xSemaphoreGive(state->buffer_mutex);
                }
            }
            last_packet_time = current_time;
            packet_interval = random_interval();
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy waiting
    }
    vTaskDelete(NULL);
}
/*  Switch Task  */
static void switchTask(void* param) {
    // This task handle packets moving in the switch
    (void)param;
    Packet pkt;
    DelayQueueItem delay_item;
    uint32_t total_packets_processed = 0;
    uint32_t data_packets_dropped = 0;
    uint32_t ack_packets_dropped = 0;
    while (!should_terminate) {
        if (xQueueReceive(switchQueue, &pkt, pdMS_TO_TICKS(100)) == pdPASS) {
            total_packets_processed++;
            // Set drop probability depend on packet type
            double drop_prob = (pkt.packet_type == PACKET_TYPE_ACK) ? P_ACK : P_DROP;
            // Generate random number between 0.0 and 1.0
            double random_val = (double)rand() / (double)RAND_MAX;
            // Debug print every 100 packets to verify drop logic
            if (total_packets_processed % 100 == 0) {
                safe_printf("[SWITCH DEBUG] Processed %lu packets, Random: %.4f, Threshold: %.4f\n",
                    total_packets_processed, random_val, drop_prob);
            }
            if (random_val < drop_prob) {
                if (pkt.packet_type == PACKET_TYPE_ACK) {
                    ack_packets_dropped++;
                }
                else {
                    data_packets_dropped++;
                }
                safe_printf("[SWITCH] Dropped %s packet #%lu (Random: %.4f < %.4f)\n",
                    (pkt.packet_type == PACKET_TYPE_ACK) ? "ACK" : "DATA",
                    pkt.sequence_number, random_val, drop_prob);
                continue;
            }
            // Packet not dropped, add to delay queue
            delay_item.packet = pkt;
            delay_item.arrival_time = xTaskGetTickCount();
            delay_item.tx_delay_ms = D_MS + transmission_delay(pkt.length);
            if (xQueueSend(delayQueue, &delay_item, pdMS_TO_TICKS(10)) != pdPASS) {
                safe_printf("[SWITCH] Delay queue full, dropped packet #%lu\n", pkt.sequence_number);
                if (pkt.packet_type == PACKET_TYPE_DATA) {
                    data_packets_dropped++;
                }
            }
        }
    }
    // Print last drop statistics
    safe_printf("[SWITCH] Final Stats - Total: %lu, Data Dropped: %lu, ACK Dropped: %lu\n",
        total_packets_processed, data_packets_dropped, ack_packets_dropped);
    vTaskDelete(NULL);
}
/* Delay Manager Task */
static void delayManagerTask(void* param) {
    // This task wait for packet delays then forward them
    (void)param;
    DelayQueueItem item;
    TickType_t current_time;
    while (!should_terminate) {
        if (xQueueReceive(delayQueue, &item, pdMS_TO_TICKS(50)) == pdPASS) {
            current_time = xTaskGetTickCount();
            // Calculate elapsed time since packet come to switch
            TickType_t elapsed_ticks = current_time - item.arrival_time;
            uint32_t elapsed_ms = (elapsed_ticks * 1000) / configTICK_RATE_HZ;
            // Check if total delay (propogation + transmission) is finished
            if (elapsed_ms >= item.tx_delay_ms) {
                // Delay period finished, send the packet
                if (item.packet.packet_type == PACKET_TYPE_ACK) {
                    uint8_t sender_idx = item.packet.destination - 1;
                    if (sender_idx < NUM_SENDERS) {
                        if (xQueueSend(ackQueues[sender_idx], &(item.packet), 0) != pdPASS) {
                            // ACK queue full, drop silent
                            safe_printf("[DELAY] ACK queue full, dropped ACK for packet #%lu\n",
                                item.packet.sequence_number);
                        }
                    }
                }
                else {
                    uint8_t receiver_idx = item.packet.destination - 3;
                    if (receiver_idx < NUM_RECEIVERS) {
                        if (xQueueSend(receiverQueues[receiver_idx], &(item.packet), 0) != pdPASS) {
                            // Receiver queue full, drop silent
                            safe_printf("[DELAY] Receiver queue full, dropped packet #%lu\n",
                                item.packet.sequence_number);
                        }
                    }
                }
            }
            else {
                // Delay not finished yet, put packet back in queue
                // Small delay to prevent busy waiting
                vTaskDelay(pdMS_TO_TICKS(1));
                if (xQueueSend(delayQueue, &item, 0) != pdPASS) {
                    // Can't requeue, packet lost because queue overflow
                    safe_printf("[DELAY] Failed to requeue packet #%lu - packet lost\n",
                        item.packet.sequence_number);
                }
            }
        }
    }
    vTaskDelete(NULL);
}
/* Receiver Task */
static void receiverTask(void* param) {
    // This is main function for each receiver
    uint8_t receiver_id = 3 + (uint32_t)param;
    uint8_t receiver_idx = (uint32_t)param;
    ReceiverState* state = &receiverStates[receiver_idx];
    Packet pkt;
    while (!should_terminate) {
        if (xQueueReceive(receiverQueues[receiver_idx], &pkt, pdMS_TO_TICKS(100)) == pdPASS) {
            uint8_t sender_idx = pkt.sender_id - 1;
            if (pkt.destination == receiver_id && sender_idx < NUM_SENDERS) {
                if (pkt.sequence_number == state->expected_seq_num[sender_idx]) {
                    // Packet received in order
                    state->packets_received++;
                    state->packets_received_from_sender[sender_idx]++;
                    state->total_bytes_received += (pkt.length - H);
                    state->expected_seq_num[sender_idx]++;
                    if (xSemaphoreTake(stats_mutex, pdMS_TO_TICKS(10)) == pdPASS) {
                        total_packets_received++;
                        xSemaphoreGive(stats_mutex);
                    }
                    safe_printf("[RECEIVER %d] Received packet #%lu from Sender %d (in sequence)\n",
                        receiver_id, pkt.sequence_number, pkt.sender_id);
                    // Send ACK
                    Packet ack;
                    ack.sender_id = receiver_id;
                    ack.destination = pkt.sender_id;
                    ack.sequence_number = pkt.sequence_number;
                    ack.length = K;
                    ack.packet_type = PACKET_TYPE_ACK;
                    if (xQueueSend(switchQueue, &ack, 0) == pdPASS) {
                        state->acks_sent++;
                        safe_printf("[RECEIVER %d] Sent ACK for packet #%lu to Sender %d\n",
                            receiver_id, pkt.sequence_number, pkt.sender_id);
                    }
                    // Check if simulation must end
                    if (!should_terminate) {
                        uint8_t all_receivers_done = 1;
                        // Check if all receivers got at least 2000 packets
                        for (int i = 0; i < NUM_RECEIVERS; i++) {
                            if (receiverStates[i].packets_received < MAX_PACKETS_TO_RECEIVE) {
                                all_receivers_done = 0;
                                break;
                            }
                        }
                        if (all_receivers_done) {
                            should_terminate = 1;
                            TickType_t total_ticks = xTaskGetTickCount() - simulation_start_time;
                            uint32_t total_ms = (total_ticks * 1000) / configTICK_RATE_HZ;
                            safe_printf("\n================= SIMULATION COMPLETE =================\n");
                            safe_printf("All receivers have received 2000 packets each\n");
                            safe_printf("Simulation time: %lu ms\n", total_ms);
                            int p_drop_int = (int)(P_DROP * 1000);
                            safe_printf("P_DROP: 0.%03d\n", p_drop_int);
                            safe_printf("Timeout (TOUT_MS): %d ms\n", TOUT_MS);
                            safe_printf("Window Size (DEFAULT_WINDOW_SIZE): %d\n", DEFAULT_WINDOW_SIZE);
                            // Calculate total packets received for all receivers
                            uint32_t total_received = 0;
                            for (int i = 0; i < NUM_RECEIVERS; i++) {
                                total_received += receiverStates[i].packets_received;
                            }
                            safe_printf("Total packets received: %lu\n", total_received);
                            // Print detail statistics
                            for (int i = 0; i < NUM_SENDERS; i++) {
                                safe_printf("Sender %d: Generated=%lu, Transmitted=%lu, Retransmitted=%lu, ACKs=%lu, Dropped(Max retry)=%lu\n",
                                    i + 1, senderStates[i].packets_generated, senderStates[i].packets_transmitted,
                                    senderStates[i].packets_retransmitted, senderStates[i].acks_received,senderStates[i].packets_dropped_max_retrans);
                            }
                            for (int i = 0; i < NUM_RECEIVERS; i++) {
                                safe_printf("Receiver %d: Received=%lu, Out-of-order=%lu, Duplicate=%lu, ACKs sent=%lu\n",
                                    i + 3, receiverStates[i].packets_received, receiverStates[i].packets_out_of_order,
                                    receiverStates[i].packets_duplicate, receiverStates[i].acks_sent);
                                for (int j = 0; j < NUM_SENDERS; j++) {
                                    safe_printf("  - From Sender %d: %lu packets\n",
                                        j + 1, receiverStates[i].packets_received_from_sender[j]);
                                }
                            }
                            // Calculate and print throughput
                            if (total_ms > 0) {
                                // Calculate total data bytes received for all receivers
                                uint64_t total_data_bytes = 0;
                                for (int i = 0; i < NUM_RECEIVERS; i++) {
                                    total_data_bytes += receiverStates[i].total_bytes_received;
                                }
                                total_data_bytes = total_data_bytes / 2; // to caluclate the throughput for sender , receiver pair
                                // Calculate throughput using double precision
                                double total_time_sec = (double)total_ms / 1000.0;
                                double throughput_bps = total_data_bytes / total_time_sec;  // Fixed division
                                uint32_t throughput_int = (uint32_t)throughput_bps;
                                safe_printf("Total data bytes received: %lu bytes\n", (uint32_t)total_data_bytes);
                                safe_printf("Effective throughput: %lu bytes/sec\n", throughput_int);
                                safe_printf("Effective throughput: %lu kbytes/sec\n", throughput_int / 1000);
                            }
                            safe_printf("=====================================================\n");
                            xSemaphoreGive(terminationSemaphore);
                        }
                    }
                }
                else if (pkt.sequence_number < state->expected_seq_num[sender_idx]) {
                    // Duplicate packet
                    state->packets_duplicate++;
                    safe_printf("[RECEIVER %d] Duplicate packet #%lu from Sender %d (expected #%lu)\n",
                        receiver_id, pkt.sequence_number, pkt.sender_id, state->expected_seq_num[sender_idx]);
                    // Still send ACK for duplicate (this is important for Go-Back-N)
                    Packet ack;
                    ack.sender_id = receiver_id;
                    ack.destination = pkt.sender_id;
                    ack.sequence_number = pkt.sequence_number;
                    ack.length = K;
                    ack.packet_type = PACKET_TYPE_ACK;

                    if (xQueueSend(switchQueue, &ack, 0) == pdPASS) {
                        state->acks_sent++;
                    }

                }
                else {
                    // Out of order packet - drop it
                    state->packets_out_of_order++;
                    safe_printf("[RECEIVER %d] Out-of-order packet #%lu from Sender %d (expected #%lu) - DROPPED\n",
                        receiver_id, pkt.sequence_number, pkt.sender_id, state->expected_seq_num[sender_idx]);
                }
            }
        }
    }
    vTaskDelete(NULL);
}
/* FreeRTOS Hooks */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    // This function run if a task use too much stack
    (void)xTask;
    safe_printf("\n!!! STACK OVERFLOW IN TASK: %s !!!\n", pcTaskName);
    while (1);
}
void vApplicationMallocFailedHook(void) {
    // This function run if heap run out of memory
    safe_printf("\n!!! HEAP EXHAUSTED !!!\n");
    while (1);
}
void vApplicationIdleHook(void) {}
void vApplicationTickHook(void) {}
/* Main function */
int main(void) {
    initialise_monitor_handles();
    srand(time(NULL));
    safe_printf("Starting Network Simulation with Go-Back-N Protocol\n");
    safe_printf("Window Size (N): %d\n", DEFAULT_WINDOW_SIZE);
    int p_drop_int = (int)(P_DROP * 1000);
    safe_printf("P_DROP: 0.%03d\n", p_drop_int);
    int p_ACK_int = (int)(P_ACK * 1000);
    safe_printf("ACK Drop Probability: %.03d\n", p_ACK_int);
    safe_printf("Timeout: %d ms\n", TOUT_MS);
    safe_printf("Packet Interval: %d-%d ms\n", T1_MS, T2_MS);
    safe_printf("=====================================\n");
    simulation_start_time = xTaskGetTickCount();
    // Make synchronization objects
    terminationSemaphore = xSemaphoreCreateBinary();
    stats_mutex = xSemaphoreCreateMutex();
    // Make queues
    switchQueue = xQueueCreate(SWITCH_QUEUE_SIZE, sizeof(Packet));
    delayQueue = xQueueCreate(DELAY_QUEUE_SIZE, sizeof(DelayQueueItem));
    for (int i = 0; i < NUM_RECEIVERS; i++) {
        receiverQueues[i] = xQueueCreate(RECEIVER_QUEUE_SIZE, sizeof(Packet));
    }
    for (int i = 0; i < NUM_SENDERS; i++) {
        ackQueues[i] = xQueueCreate(ACK_QUEUE_SIZE, sizeof(Packet));
    }
    // Init sender states
    for (int i = 0; i < NUM_SENDERS; i++) {
        senderStates[i].buffer_mutex = xSemaphoreCreateMutex();
        senderStates[i].window_size = DEFAULT_WINDOW_SIZE;
        senderStates[i].packets_generated = 0;
        senderStates[i].packets_transmitted = 0;
        senderStates[i].packets_retransmitted = 0;
        senderStates[i].packets_dropped_max_retrans = 0;
        senderStates[i].acks_received = 0;
        for (int j = 0; j < NUM_RECEIVERS; j++) {
            senderStates[i].next_seq_num[j] = 0;
            senderStates[i].base_seq_num[j] = 0;
            senderStates[i].last_timeout_check[j] = 0;
            for (int k = 0; k < DEFAULT_WINDOW_SIZE; k++) {
                senderStates[i].tx_buffer[j][k].in_use = 0;
                senderStates[i].tx_buffer[j][k].sequence_number = 0;
                senderStates[i].tx_buffer[j][k].retransmission_count = 0;
                senderStates[i].tx_buffer[j][k].timeout_time = 0;
            }
        }
    }
    // Init receiver states
    for (int i = 0; i < NUM_RECEIVERS; i++) {
        receiverStates[i].packets_received = 0;
        receiverStates[i].packets_out_of_order = 0;
        receiverStates[i].packets_duplicate = 0;
        receiverStates[i].acks_sent = 0;
        for (int j = 0; j < NUM_SENDERS; j++) {
            receiverStates[i].expected_seq_num[j] = 0;
            receiverStates[i].packets_received_from_sender[j] = 0;
        }
    }
    // Make tasks
    xTaskCreate(senderTask, "Sender1", SENDER_TASK_STACK_SIZE, (void*)1, SENDER_TASK_PRIORITY, NULL);
    xTaskCreate(senderTask, "Sender2", SENDER_TASK_STACK_SIZE, (void*)2, SENDER_TASK_PRIORITY, NULL);
    xTaskCreate(switchTask, "Switch", SWITCH_TASK_STACK_SIZE, NULL, SWITCH_TASK_PRIORITY, NULL);
    xTaskCreate(delayManagerTask, "DelayMgr", DELAY_MGR_TASK_STACK_SIZE, NULL, DELAY_MGR_TASK_PRIORITY, NULL);
    xTaskCreate(receiverTask, "Receiver3", RECEIVER_TASK_STACK_SIZE, (void*)0, RECEIVER_TASK_PRIORITY, NULL);
    xTaskCreate(receiverTask, "Receiver4", RECEIVER_TASK_STACK_SIZE, (void*)1, RECEIVER_TASK_PRIORITY, NULL);
    // Start the scheduler
    vTaskStartScheduler();
    safe_printf("Scheduler ended!\n");
    return 0;
}

