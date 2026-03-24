# **High-Performance Option Pricing Engine**

A high-throughput computational kernel written in modern C++ (C++17) for pricing financial derivatives. This engine is designed to simulate the core mathematical and concurrent processing layers of a quantitative trading system, capable of pricing millions of options per second across multi-core architectures.

## **Features**

* **Black-Scholes Model**: Calculates both theoretical option prices and first/second-order Greeks (Delta, Gamma, Vega, Theta, Rho) for European options.  
* **Binomial Tree Model**: Implements the Cox-Ross-Rubinstein (CRR) model with early-exercise boundary logic for American options.  
* **High-Performance Concurrency Engine**: Automatically scales to utilize all available CPU cores using a highly optimized chunking strategy.  
* **Memory Safe & Cache-Optimized**: Designed for maximum CPU cache locality to avoid cache-miss penalties during heavy parallel workloads.

## **System Architecture & Optimizations**

This project focuses heavily on High-Performance Computing (HPC) principles common in algorithmic trading:

1. **Thread Chunking vs. 1:1 Spawning**: Rather than suffering the massive OS-level overhead of spawning a thread for each option contract, the ParallelPricingEngine divides the portfolio into equally sized chunks based on std::thread::hardware\_concurrency(). These chunks are dispatched to worker threads using std::async, minimizing context switching.  
2. **Contiguous Memory Locality**: Both input parameters and output results are stored in std::vector structures. By guaranteeing contiguous memory layout, the engine maximizes L1/L2 cache hits across CPU cores.  
3. **Pre-Allocation in Tight Loops**: The O(N²) backward induction step inside the Binomial Tree pre-allocates memory for leaf nodes before the tight loop begins, completely eliminating dynamic heap fragmentation during execution.  
4. **Lock-Free Parallelism**: The architecture enforces a strict read-only state on the input portfolio and provides dedicated, pre-allocated memory slots for each output result, avoiding the need for mutexes or atomic locks entirely.

## **Getting Started**

### **Prerequisites**

* A C++ compiler supporting C++17 or higher (GCC, Clang, or MSVC).  
* CMake (optional, for broader build integration) or standard Make.

### **Compilation**

To compile the engine with maximum performance optimizations (-O3) and threading support, use the following command:

g++ \-std=c++17 \-O3 \-pthread PricingEngine.cpp \-o engine

### **Execution**

Run the compiled binary to execute the benchmark suite. The engine will generate a random portfolio of 1,000,000 options and benchmark both sequential and parallel execution speeds.

./engine

## **Future Roadmap**

This repository currently serves as the computational core. Future phases will integrate the data-handling infrastructure required for a complete production trading system:

* **Phase 2: High-Speed Data Ingestion**  
  * Implement a zero-copy CSV parser using memory-mapped files (mmap) to ingest gigabytes of historical options chain data directly into the pricing engine.  
* **Phase 3: Volatility Surface Generation**  
  * Integrate root-finding algorithms (e.g., Newton-Raphson) to calculate Implied Volatility from live market quotes.  
  * Build a 2D Bilinear/Bicubic interpolation matrix to query the volatility smile/skew dynamically.  
* **Phase 4: Live Data Pipeline**  
  * Introduce a C++ networking layer (via Boost.Asio or cpprestsdk) and lock-free ring buffers to process live market data via WebSockets asynchronously.
