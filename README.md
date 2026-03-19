# KTP Reliable UDP Sockets

## 📌 Overview

This project implements a **reliable data transfer protocol over UDP** using custom KTP sockets.
It ensures reliable communication using sliding window, acknowledgments, and retransmissions.

---

## ⚙️ Features

* Sliding Window Protocol
* Acknowledgment (ACK) handling
* Receiver Window (rwnd) flow control
* Timeout-based retransmission
* Simulated packet loss
* Shared memory + semaphore synchronization
* Multi-threaded design (Sender, Receiver, GC threads)

---

## 🛠️ Tech Stack

* C Programming
* UDP Sockets
* POSIX Threads
* System V Shared Memory & Semaphores

---

## 🚀 How to Run

### Step 1: Compile

```bash
make
```

### Step 2: Run init

```bash
./init
```

### Step 3: Run receiver

```bash
./user2 127.0.0.1 9093 127.0.0.1 8082 output.txt
```

### Step 4: Run sender

```bash
./user1 127.0.0.1 8082 127.0.0.1 9093 input.txt
```

---

## 📊 Key Concepts

* Reliable transport over unreliable UDP
* Sliding window protocol
* Flow control and congestion handling
* Packet loss simulation and recovery

---

## 👨‍💻 Author

KAVYA RAI

IIT Kharagpur
