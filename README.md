# Network Communication Protocol Simulation Using FreeRTOS

## 📌 Overview

This project simulates a **network communication protocol** in an embedded system environment using **FreeRTOS**. It models communication between two sender nodes and two receiver nodes through a central **switch**, introducing realistic factors such as **packet loss**, **propagation delay**, and **transmission delay**.

The goal is to evaluate and compare the performance of **Send-and-Wait (S&W)** and **Go-Back-N (GBN)** protocols under various network conditions.

---

## 🛠️ Features

- Implementation of real-time tasks using **FreeRTOS**:
  - Sender Tasks
  - Receiver Tasks
  - Switch Task
  - Delay Manager Task
- Dynamic packet generation and memory management
- Support for S&W and GBN protocols with configurable **window size (N)**
- Simulation of:
  - Propagation and transmission delays
  - Packet loss for data and ACKs
  - Timeouts and retransmissions

---

## 📦 Packet Structure

Each packet consists of:
- **Sender ID** (1 byte)
- **Destination ID** (1 byte)
- **Sequence Number** (4 bytes)
- **Packet Length** (2 bytes)
- **Packet Type** (1 byte) — Data or ACK
- **Payload** of random size between **500 to 1500 bytes**

ACK packets are fixed at **40 bytes**.

---

## 🔄 Protocols Implemented

- **Send-and-Wait (S&W)**:
  - One packet in flight at a time
  - Retransmission on timeout (up to 4 attempts)

- **Go-Back-N (GBN)**:
  - Window sizes: 2, 4, 8, 16
  - Cumulative ACKs
  - Retransmission of full window on timeout

---

## 📊 Performance Metrics

Measured metrics include:
- **Throughput** in KB/s
- **Average number of transmissions** per packet
- **Dropped packets** after 4 failed retransmissions

Performance was evaluated under various values of:
- `P_drop`: {0.01, 0.02, 0.04, 0.08}
- `Timeout (Tout)`: {150, 175, 200} ms
- `Window Size (N)`: {1 (S&W), 2, 4, 8}

---

## 🧪 Tools and Technologies

- **FreeRTOS**
- **C (Embedded C Programming)**
- **Eclipse CDT IDE**
- **Emulation Board**

---

## 👨‍💻 Contributors

- Ahmed Mohamed Mohamed Abdelhamid Mousa 
- Ahmed Mohamed Hanafy Rehan 

---

## 📚 References

- *Mastering the FreeRTOS Real-Time Kernel v1.1.0*


