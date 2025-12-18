# 🚀 Mini-DBMS Storage Engine

A modular C++ Storage Engine that implements a low-level database architecture. This project features a **Buffer Manager** with an **LRU Caching Policy** and a persistent **B+ Tree Indexing** system.

## 📁 Project Structure
- `src/`: Core implementation of the main execution logic.
- `include/`: Header files and class definitions for the Storage Engine.
- `docs/`: Technical specifications and architectural diagrams.
- `tests/`: Sample execution logs showing system behavior.
- `scripts/`: Automation scripts for building and cleaning the project.

## 🏗️ Architecture
The system is built on a 3-layer hierarchy to simulate a real-world database engine:
1. **Disk Layer:** Manages 4KB page-aligned binary files.
2. **Buffer Layer:** Minimizes Disk I/O by keeping frequent pages in RAM.
3. **Logic Layer:** Handles B+ Tree node splitting and data organization.



## 🛠️ Key Features
- **LRU Eviction:** Automatically kicks out the least recently used pages when RAM is full.
- **Binary Persistence:** Writes data to a `database.db` file that survives program restarts.
- **Node Splitting:** Robust B+ Tree logic that handles overflow by promoting keys to parents.
- **Detailed Logging:** Real-time terminal logs for Disk I/O and Buffer Hits/Misses.

## 🚀 Getting Started

### Prerequisites
- C++11 compiler or higher (`g++`).

### Build and Run
Use the provided scripts for easy execution:
```bash
chmod +x scripts/build.sh
./scripts/build.sh
