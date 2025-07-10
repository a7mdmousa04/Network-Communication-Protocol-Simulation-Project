**Embedded system project**

**RTOS Project**

**Dr. Khaled fouad**

  **BN**   **Sec**   **Student ID**   **[اسم الطالب]{dir="rtl"}**
  -------- --------- ---------------- --------------------------------------------
  21       1         9230178          [احمد محمد محمد عبدالحميد موسى]{dir="rtl"}
  17       1         9230169          [احمد محمد حنفى ريحان]{dir="rtl"}

System Design
=============

> This project simulates a network communication system using FreeRTOS
> to model the behavior of senders, receivers, and a central switch
> managing a lossy communication link. The simulation aims to evaluate
> the performance of the Go-Back-N protocol under varying network
> conditions.
>
> The system consists of two sender nodes (Node 1, Node 2) and two
> receiver nodes (Node 3, Node 4). Senders generate data packets
> destined for a randomly chosen receiver. These packets traverse a
> switch node which introduces propagation delay, transmission delay
> based on packet size and link capacity, and simulates packet loss.
>
> ![C:\\Users\\Ahmed\\OneDrive\\Desktop\\qraph1.png](media/image1.png){width="6.325in"
> height="5.191666666666666in"}

Figure 1 : Message Sequence showing Tasks interaction

Packet Structure
----------------

Packets are dynamically allocated and contain a header and payload. The
header includes:

-   Sender ID (1 byte)

-   Destination ID (1 byte)

-   Sequence Number (4 bytes)

-   Packet Length (2 bytes)

-   Packet Type (1 byte: 0 for Data, 1 for ACK)

> The payload size L varies randomly between L1 (500 bytes) and L2 (1500
> bytes). Acknowledgement
>
> (ACK) packets have a fixed size K (40 bytes).

RTOS Implementation
-------------------

> The system utilizes FreeRTOS components to manage concurrency and
> communication:

### **Tasks:-** Separate tasks model each sender, each receiver, the switch, and a delay

> manager.

a.  **Sender Task :-** Generates packets at random intervals (T1 to T2
    ms), manages a transmission buffer for each destination receiver,
    handles ACKs, detects timeouts (Tout), and implements the Go-Back-N
    retransmission logic ( window size N , max retransmissions) and S&W
    Protocol ( if N equals 1) .

b.  **Receiver Task :**- Waits for packets on its dedicated queue,
    checks sequence

> numbers, sends ACKs for in-order packets via the switch, and discards
> out-of order
>
> packets (as per Go-Back-N). It tracks received packets and total
> bytes.

c.  **Switch Task :-** Receives packets from senders and ACKs from
    receivers via a

> central switchQueue . It simulates packet drops based on probabilities
>
> P\_drop (for data) and P\_ack (for ACKs). Non-dropped packets are
> forwarded
>
> to the delayManagerTask .

d.  **Delay Manager Task :-** Receives packets from the switch via delay
    Queue . It

> Holds packets for the calculated total delay (propagation D +
> transmission
>
> (L\*8)/C) before forwarding them to the appropriate receiver queue
>
> ( receiver Queues ) or sender ACK queue ( ack Queues ).

### **Queues:-** Used for inter-task communication:

a.  **Switch Queue :-** Central queue for all packets entering the
    switch.

b.  **Delay Queue :-** Holds packets undergoing simulated delay.

c.  **Receiver Queues \[NUM\_RECEIVERS\] :-** Dedicated queue for each
    receiver.

d.  **Ack Queues \[NUM\_SENDERS\] :-** Dedicated queue for ACKs destined
    for each

> sender.

### Semaphores:-

a.  **buffer\_mutex :-** Protects access to the sender\'s transmission
    buffer andstate variables.

b.  **stats\_mutex :-** Protects access to global statistics.

c.  **Termination Semaphore :-** Signals the end of the simulation (when
    2000 packets are received for each receiver ).

Protocols
---------

### Send and wait protocol (S&W) 

> The Send-and-Wait (S&W) protocol is a simple Automatic Repeat request
> (ARQ) scheme in which the sender transmits one packet and then pauses
> its transmission until it receives an acknowledgment (ACK) for that
> packet. This approach ensures reliable data delivery over lossy links
> by retransmitting packets when ACKs are lost or corrupted.( special
> case of Go-Back-N with N = 1).

### Go-Back-N Protocol

> The simulation models the Go-Back-N protocol with a selectable window
> size (1, 2, 4, or 8). The sender can have up to N unacknowledged
> packets in flight and uses cumulative ACKs---an ACK for packet i
> confirms all packets up to i. If the base packet's timer expires, the
> sender retransmits the entire window, and any packet exceeding four
> retransmissions is dropped. The receiver only accepts in-order
> packets, discarding any that arrive out of sequence.

![](media/image2.png){width="2.9881944444444444in" height="6.416666666666667in"}Simulation Flow
-----------------------------------------------------------------------------------------------

### **Initialization**

-   ### The system sets up tasks, queues, and semaphores, and initializes the state of senders and receivers.

### **Packet Handling**

-   **Generation**: Senders create packets with random intervals,
    > sequence numbers, and destinations.

-   **Transmission**: Senders attempt to send packets if their window
    > isn't full, starting timers and forwarding packets to the switch
    > queue.

### **Switch & Delay**

-   The switch processes packets, applying a drop probability, and
    > forwards non-dropped packets to the delay manager.

-   The delay manager holds packets for a calculated delay before
    > sending them to the receiver's queue or sender's ACK queue.

### **Receiver & ACK**

-   Receivers accept in-order packets, count them, and generate ACKs.
    > Out-of-order packets are discarded.

-   ACKs return to the sender, updating the window and allowing new
    > transmissions.

### **Timeout & Retransmission**

-   Expired timers trigger retransmission of the base packet in the
    > sender's window, with packets dropped after max retries.

### **Termination** 

-   The simulation ends once 2000 packets are successfully received by
    > each receiver.

Results and Discussion 
======================

The simulation evaluated the performance of the S&W and Go-Back-N
protocols by measuring the throughput (in KBytes/sec) required to
successfully receive 2000 packets for each receiver under various
network conditions as shown in Figure 3, specifically varying packet
drop probability (P\_drop), retransmission timeout (Tout), and Go-Back-N
window size (N).

![C:\\Users\\Ahmed\\OneDrive\\Desktop\\output\\2.21.png](media/image3.png){width="4.941666666666666in"
height="2.55in"}

Figure 3 : Example of Simulation output for (S&W)

![](media/image4.emf){width="2.6729166666666666in" height="1.8652777777777778in"}the average number of transmissions of a packet as function of Pdrop E (Figure 4 )
-------------------------------------------------------------------------------------------------------------------------------------------------------------------

E =
$\frac{\text{Total\ num\ of\ tansmissions}}{\text{Total\ num\ of\ packets}}$
for each value of P\_drop so for P ={0.01 ,0.02 ,0.04 ,0.08} E =
{1.01544, 1.02866, 1.0535, 1.09025}

**packets dropped due to being transmitted more than 4 times**. 
---------------------------------------------------------------

> After simulations with different values of P\_drop packets dropped due
> to being transmitted more than 4 Times equal zero .

Throughput Analysis
-------------------

> The primary performance metric measured was throughput, defined as the
> total bytes of Successfully received unique packets divided by the
> total simulation time.

### Impact of Window Size (N)

The Go-Back-N protocol aims to improve upon the basic Send-and-Wait
(N=1) by allowing multiple packets to be outstanding. The table below
summarizes the throughput achieved for different window sizes (N), drop
probabilities (P\_drop), and

timeouts (T, equivalent to Tout in ms).

Table 1 : P\_drop , T\_out , N Index and Corresponding Throughput in
Kbyte

  window sizes (N)       1 ( S&W Protocol )   2              4              8
  ---------------------- -------------------- -------------- -------------- --------------
  P\_drop=0.02 , T=150   11.863 Kbyte         12.729 Kbyte   12.760 Kbyte   12.827 Kbyte
  P\_drop=0.02,T=175     11.795 Kbyte         12.678 Kbyte   12.874 Kbyte   12.936 Kbyte
  P\_drop=0.02,T=200     11.815 Kbyte         12.738 Kbyte   12.878 Kbyte   12.994 Kbyte
  P\_drop=0.04,T=150     11.579 Kbyte         12.815 Kbyte   12.884 Kbyte   13.026 Kbyte
  P\_drop=0.04,T=175     11.574 Kbyte         12.656 Kbyte   12.872 Kbyte   12.918 Kbyte
  P\_drop=0.04,T=200     11.524 Kbyte         12.588 Kbyte   12.854 Kbyte   13.035 Kbyte
  P\_drop=0.08,T=150     11.099 Kbyte         12.702 Kbyte   12.754 Kbyte   12.970 Kbyte
  P\_drop=0.08,T=175     10.974 Kbyte         12.515 Kbyte   12.908 Kbyte   13.019 Kbyte
  P\_drop=0.08,T=200     10.834 Kbyte         12.159 Kbyte   12.754 Kbyte   13.240 Kbyte

### Impact of Packet Drop Probability (P\_drop)

The probability of packet loss significantly impacts performance. Figure
5 illustrates the relationship between throughput and P\_drop for
different timeout values (for N = {1, 2, 4, 8}).( N = 1 is the S&W
protocol )![](media/image5.emf){width="6.930555555555555in"
height="2.951388888888889in"}

Figure 5 : throughput as a function of Pdrop for different values of
timeout period Tout

###  Impact of Timeout (Tout)

> The probability of packet loss significantly impacts performance.
> Figure 6 illustrates the relationship between throughput as a function
> of Tout for different values of Pdrop values (for N = {1 ,2 ,4 ,8} ).
> ![](media/image6.emf){width="6.666666666666667in" height="2.1875in"}

Figure 6 : throughput as a function of Tout for different values of
Pdrop

References
==========

1.  Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0.

Code Snippets 
=============

+----------------------------------------------------------------------+
| int main(void) {                                                     |
|                                                                      |
|     initialise\_monitor\_handles();                                  |
|                                                                      |
|     srand(time(NULL));                                               |
|                                                                      |
|     simulation\_start\_time = xTaskGetTickCount();                   |
|                                                                      |
|     terminationSemaphore = xSemaphoreCreateBinary();                 |
|                                                                      |
|     stats\_mutex = xSemaphoreCreateMutex();                          |
|                                                                      |
|     switchQueue = xQueueCreate(SWITCH\_QUEUE\_SIZE, sizeof(Packet)); |
|                                                                      |
|     delayQueue = xQueueCreate(DELAY\_QUEUE\_SIZE,                    |
| sizeof(DelayQueueItem));                                             |
|                                                                      |
|     for (int i = 0; i \< NUM\_RECEIVERS; i++) {                      |
|                                                                      |
|         receiverQueues\[i\] = xQueueCreate(RECEIVER\_QUEUE\_SIZE,    |
| sizeof(Packet)); }                                                   |
|                                                                      |
|     for (int i = 0; i \< NUM\_SENDERS; i++) {                        |
|                                                                      |
|         ackQueues\[i\] = xQueueCreate(ACK\_QUEUE\_SIZE,              |
| sizeof(Packet));}                                                    |
|                                                                      |
|     for (int i = 0; i \< NUM\_SENDERS; i++) {                        |
|                                                                      |
|         senderStates\[i\].buffer\_mutex = xSemaphoreCreateMutex();   |
|                                                                      |
|         senderStates\[i\].window\_size = DEFAULT\_WINDOW\_SIZE;      |
|                                                                      |
|         for (int j = 0; j \< NUM\_RECEIVERS; j++) {                  |
|                                                                      |
|             senderStates\[i\].next\_seq\_num\[j\] = 0;               |
|                                                                      |
|             senderStates\[i\].base\_seq\_num\[j\] = 0;} }            |
|                                                                      |
| xTaskCreate(senderTask,\"Sender1                                     |
| \",SENDER\_TASK\_STACK\_SIZE,(void\*)1,SENDER\_TASK\_PRIORITY,NULL); |
|                                                                      |
| xTaskCreate(senderTask,\"Sender2\", SENDER\_TASK\_STACK\_SIZE,       |
| (void\*)2, SENDER\_TASK\_PRIORITY,NULL);                             |
|                                                                      |
| xTaskCreate(switchTa                                                 |
| sk,\"Switch\",SWITCH\_TASK\_STACK\_SIZE,NULL,SWITCH\_TASK\_PRIORITY, |
| NULL);                                                               |
|                                                                      |
| xTaskCreate(delayManagerTask\"DelayMgr\                              |
| ",DELAYMGR\_TASK\_STACK\_SIZE,NULL,DELAY\_MGR\_TASK\_PRIORITY,NULL); |
|                                                                      |
| xTaskCreate(receiverTask,\"Receiver3\",                              |
| RECEIVER\_TASK\_STACK\_SIZE,void\*)0,RECEIVER\_TASK\_PRIORITY,NULL); |
|                                                                      |
| xTaskCreate(receiverTask,\"Receiver4\",                              |
| RECEIVER\_TASK\_STACK\_SIZE(void\*)1,RECEIVER\_TASK\_PRIORITY,NULL); |
|                                                                      |
| vTaskStartScheduler();                                               |
|                                                                      |
|     return 0;}                                                       |
+----------------------------------------------------------------------+
