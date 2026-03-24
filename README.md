# KTP Reliable UDP Sockets

## 📌 Overview

This project implements a **reliable data transfer protocol over UDP** using custom KTP sockets.  
It ensures reliable communication using sliding window, acknowledgments, retransmissions, and flow control mechanisms.

---

## ⚙️ Features

- Sliding Window Protocol (Sender & Receiver side)
- Acknowledgment (ACK) handling with sequence numbers
- Receiver Window (rwnd) based flow control
- Timeout-based retransmission
- Simulated packet loss using probability `P`
- Shared memory + semaphore synchronization
- Multi-threaded design:
  - Receiver Thread (R)
  - Sender Thread (S)
  - Garbage Collector Thread (GC)
- **Support for multiple concurrent KTP sockets (multi-user support)**

---

## 🛠️ Tech Stack

- C Programming
- UDP Sockets
- POSIX Threads
- System V Shared Memory & Semaphores

---

## 🚀 How to Run

### Step 1: Compile
```bash
make
```

### Step 2: Run init process
```bash
./init
```

### Step 3: Run receiver (User 2)
```bash
./user2 127.0.0.1 9093 127.0.0.1 8082 output.txt
```

### Step 4: Run sender (User 1)
```bash
./user1 127.0.0.1 8082 127.0.0.1 9093 input.txt
```

---

## 👥 Multi-User Support

The implementation supports **multiple concurrent KTP sockets** using shared memory.

To test multiple users, run multiple sender-receiver pairs with different port numbers:

```bash
# Pair 1
./user2 127.0.0.1 9001 127.0.0.1 8001 output1.txt
./user1 127.0.0.1 8001 127.0.0.1 9001 input.txt

# Pair 2
./user2 127.0.0.1 9002 127.0.0.1 8002 output2.txt
./user1 127.0.0.1 8002 127.0.0.1 9002 input.txt
```

Each socket operates independently using separate entries in shared memory.

---

## 📊 Performance Analysis

To simulate unreliable networks, packet loss is introduced using probability `P`.

- `P` varies from **0.05 to 0.50**
- Higher `P` ⇒ more packet drops ⇒ more retransmissions

Metric used:
```
Ratio = Total Transmissions / Total Messages
```

### Observations:
- As `P` increases → retransmissions increase
- Ratio increases with higher packet loss
- Confirms correctness of retransmission mechanism

---

## 📊 Key Concepts

- Reliable transport over unreliable UDP
- Sliding window protocol (Go-Back-N style behavior)
- Flow control using receiver window (rwnd)
- Timeout-based retransmission
- Handling out-of-order and duplicate packets
- Concurrent socket management using shared memory

---

## 👨‍💻 Author

KAVYA RAI  
IIT Kharagpur