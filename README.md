
# Cache Architecture Simulator

**Cycle-Accurate Performance Modeling for the Grundlagen Rechnerarchitektur (GRA) Lab**

This project was developed as part of the **Grundlagen Rechnerarchitektur (GRA)** lab course at the **Technical University of Munich (TUM)**. The simulator provides a cycle-accurate environment to evaluate the efficiency and hardware complexity of different cache mapping strategies using SystemC.

**Contributors:**
* **Adam Yahmadi**
* **Adam Hassine**
* **Othman Souguir**


## Technical Features

* **Hybrid Architecture:** Utilizes a C frontend for high-speed CSV trace parsing and a SystemC C++ backend for hardware-level clock synchronization.
* **Cache Models:**
    * **Direct-Mapped Cache:** Implements fixed-slot indexing with tag-based conflict handling.
    * **Fully Associative Cache:** Implements a parallel search mechanism with **LRU (Least Recently Used)** eviction logic.
* **Performance Metrics:** Reports total cycles, cache hits, misses, and a **Primitive Gate Count** to estimate the theoretical silicon area required for the design.


## Project Structure

```text
cache-simulator/
├── src/
│   ├── main.c           # CLI parsing and CSV ingestion logic
│   ├── simulation.cpp   # SystemC module implementations
│   └── simulation.hpp   # Module definitions and C-Linkage interface
├── examples/            # Sample memory access traces (.csv)
├── Makefile             # Multi-platform build system
└── README.md            # Project documentation
```


## Installation and Build

### Prerequisites
* **SystemC:** Ensure SystemC is installed and the `SYSTEMC_HOME` environment variable is set to your installation path.
* **Compiler:** GCC (g++) or Clang (C++14 support required).

### Compilation
To build the simulator with performance optimizations:
```bash
make release
```
For a version with debugging symbols:
```bash
make debug
```


## Usage

The simulator accepts CSV files where each line represents a memory request: `[Operation (W/R)], [Hex Address], [Data (Decimal)]`.

### Basic Execution
```bash
./systemcc [options] <input_file.csv>
```

### CLI Options
| Flag | Description | Default |
| :--- | :--- | :--- |
| `-c`, `--cycles` | Maximum number of cycles to simulate | 3000 |
| `--directmapped` | Simulate a Direct-Mapped cache | Enabled |
| `--fullassociative` | Simulate a Fully Associative cache | Disabled |
| `--cacheLines` | Number of cache lines (Must be power of 2) | 256 |
| `--cacheLineSize` | Line size in bytes (Must be power of 2) | 32 |
| `--tf <filename>` | Output path for the VCD tracefile | None |


## Academic Context
This software was created for educational purposes within the scope of the **GRA Lab at TUM**. It demonstrates the fundamental trade-offs between cache hit rates and the hardware complexity (gate count) of different associativity levels.


## License
This project is for academic use within the TUM GRA course framework. All rights reserved by the contributors.
