# High-Performance Option Pricing Engine

## Overview
This repository contains a high-frequency, multithreaded C++ option pricing engine. It is designed to handle both massive batch processing of historical option chains and ultra-low latency real-time pricing of live market data streams. The engine is built from the ground up to minimize memory allocations, maximize CPU core utilization, and strictly avoid locking overhead.

## How to Run the Program

### Prerequisites
- A **C++17** compliant compiler (e.g., `g++` or `clang++`).
- A **POSIX-compliant OS** (Linux/macOS) is required for the `mmap` zero-copy memory mapping.
- *For Phase 4 (Live WebSocket functionality)*: You will need **Boost** (`Boost.Beast`, `Boost.Asio`) and **OpenSSL** installed on your system.

### Compilation
Navigate to the project directory and compile the program using `g++` with high optimization flags (`-O3`) and threading enabled (`-pthread`):
```bash
g++ -O3 -pthread -std=c++17 Option_Pricer.cpp -o engine
```

### Execution
Run the compiled executable. Ensure that `data.csv` (the flat file containing the options chain) is located in the same directory.
```bash
./engine
```

---

## How It Works: Architectural Components

1. **Zero-Copy Memory-Mapped Parser (`mmap`)**: Instead of reading the CSV file line-by-line into strings, the engine maps the raw file directly into memory using POSIX `mmap`. It then creates `std::string_view` windows over this memory.
2. **Parallel Chunking**: The memory block is mathematically divided into chunks ending on newline characters. Each CPU core is assigned a chunk, converting raw text bytes into `OptionParams` structures concurrently.
3. **`<charconv>` Micro-parsing**: Float generation completely bypasses traditional stream operators or `std::stod`, utilizing `<charconv>` (`std::from_chars`) for allocation-free, direct hardware-level parsing.
4. **Lock-Free Ring Buffer (SPSC Queue)**: Real-time thread communication utilizes a Single-Producer/Single-Consumer queue powered by `std::atomic` sizes. By strictly using `memory_order_acquire` and `memory_order_release`, it eliminates `std::mutex` overhead.
5. **Event-Driven WebSocket Loop**: An asynchronous networking loop acts as the producer, feeding live ticks from Polygon.io directly into the memory barrier, awakening pricing threads to instantly generate new Greeks.

---

## The Mathematics

### Black-Scholes Model
The engine leverages the closed-form Black-Scholes-Merton differential equation to calculate European option prices and Greeks (Delta, Gamma, Theta, Vega, Rho). The mathematical core relies on standard normal cumulative distribution functions ($N(d_1)$ and $N(d_2)$).

### Binomial Tree (Cox-Ross-Rubinstein)
A discrete-time lattice-based model that steps backward from expiration. Unlike Black-Scholes, it can price American options by calculating the early-exercise premium at each node in the tree $S_t$.

### Implied Volatility Solver
Market prices fluctuate based on supply and demand, changing the market's expectation of future volatility. 
- **Newton-Raphson Method**: A fast, derivative-based root-finding algorithm. The function targets $f(v) = BlackScholes(v) - MarketPrice$. It uses Vega ($f'(v)$) to iterate $v_{n+1} = v_n - f(v_n)/Vega$ until the theoretical price matches the market price.
- **Brent's Method**: A robust bracket-based algorithm. If Newton-Raphson fails (e.g., deep out-of-the-money options where Vega approaches zero), the engine falls back to Brent's method to guarantee convergence.

### Volatility Surface (Bilinear Interpolation)
Because market data only provides implied volatilities at discrete strikes ($K$) and times to maturity ($T$), pricing synthetic or theoretical contracts requires interpolation. The engine constructs a 2D matrix of known IVs and performs **Bilinear Interpolation**—taking the weighted average of the four nearest known coordinate points—to dynamically predict IV anywhere on the surface.

---

## Understanding the Console Output

When you run the executable, it performs a suite of benchmarks and tests:

1. **Fast CSV Parser Tests**: Validates that `<charconv>` correctly parses floating-point structures without memory mutation.
2. **Loading Real Data**: Outputs the time taken to memory-map and concurrently parse `data.csv`. (Typically >500MB/s throughput).
3. **Sequential vs. Parallel Black-Scholes**: Compares traditional linear throughput across an entire portfolio against the CPU-distributed `ParallelPricingEngine`, printing the precise speedup factor achieved.
4. **Binomial Tree Benchmark**: Runs the computationally expensive $O(N \cdot Steps^2)$ discrete tree model across a subset of options to verify accurate lattice generation.
5. **Sample Output**: Prints the computed theoretical price and exact Greeks for the first row of data.
6. **Milestone 1 (Newton-Raphson)**: Tests the root-finding algorithm. It attempts to back-calculate the IV for a contract given a target market price of `$10.45`, verifying it arrives perfectly at `0.20`.
7. **Milestone 2 (Volatility Surface)**: Validates bilinear interpolation by constructing a mock grid of known volatilities and querying the matrix for an exact coordinate between strikes, returning a smooth `0.16` weighting.
8. **Milestone 3 (SPSC Queue Benchmark)**: The ultimate latency test. It races passing 1,000,000 live tick structs across threads using a standard `std::mutex` vs. the `std::atomic` lock-free ring buffer. It calculates the execution multiple (often >5x faster), demonstrating why lock-free pipelines are critical for HFT.
9. **Milestone 4 (Real-Time Event Loop)**: Simulates the final live data pipeline. It mocks an incoming JSON packet from a Polygon.io WebSocket, parses it, passes it through the lock-free queue, fetches the surface volatility, and prints the newly calculated live option Delta. It also measures the end-to-end engine latency in milliseconds.
