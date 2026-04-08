# **High-Performance Option Pricing Engine**

A high-throughput computational kernel and data pipeline for pricing financial derivatives. This system extracts real options market data from the Polygon.io API and ingests it into a multi-threaded C++ (C++17) pricing engine capable of evaluating millions of options per second.

## **Features**

* **Real-World Data Ingestion Pipeline**: A Python ETL script that queries live options snapshots via Polygon.io, calculates Time to Maturity (TTM), and formats the data into a high-density, flat CSV.  
* **Zero-Copy Memory-Mapped Parser**: A C++ CSV parser that bypasses standard file I/O. It maps the dataset directly into virtual memory (mmap) and uses std::string\_view and std::from\_chars to eliminate heap allocations during parsing.  
* **Black-Scholes & Binomial Models**: Calculates theoretical option prices and Greeks (Delta, Gamma, Vega, Theta, Rho) for both European and American exercise styles.  
* **Cache-Optimized Concurrency**: Automatically scales to utilize all available CPU cores. It divides the memory-mapped file into distinct byte-boundary chunks and distributes them across threads using std::async to avoid thread-creation overhead.

## **System Architecture**

The architecture separates data preparation from computation to maximize speed:

1. **The Data Layer (Python)**: Handles network latency and JSON parsing. Converts the data into a dense, headless CSV format.  
2. **The Compute Layer (C++)**: Maps the CSV into memory. Parses the string data using zero-copy methods into contiguous std::vector structures. Executes complex stochastic pricing models in parallel across all available cores.

## **Getting Started**

### **Prerequisites**

* Python 3.8+ (with pandas, numpy, python-dotenv, polygon-api-client)  
* A Polygon.io API key (Options Plan)  
* A C++ compiler supporting C++17 (GCC/Clang recommended for POSIX mmap support)

### **Step 1: Generate the Market Data**

Create a .env file in the project root and add your Polygon API key:
POLYGON\_API\_KEY=your\_key\_here

Or download the .env file contained in the repository and add your Polygon API key.

Run the data loader to fetch the options chain and generate the data.csv file:

python Data\_Loader.py

### **Step 2: Compile the C++ Engine**

Compile the engine with maximum performance optimizations (-O3):

g++ \-std=c++17 \-O3 \-pthread Option\_Pricer.cpp \-o engine

### **Step 3: Run the Engine**

Execute the compiled binary to ingest the CSV and benchmark the pricing models:

./engine

## **Remaining Project Roadmap**

This repository has completed Phase 1 (Core Computation) and Phase 2 (High-Speed Ingestion). The following phases are planned for future development:

### **Phase 3: Volatility Surface Generation**

* **Objective:** Replace the static volatility inputs with a dynamically generated Implied Volatility (IV) surface.  
* **Tasks:**  
  * Implement a root-finding algorithm (e.g., Newton-Raphson or Brent's method) in C++ to back-solve IV from real market quotes.  
  * Construct a 2D Bilinear/Bicubic interpolation matrix to query the volatility smile/skew based on strike and maturity.

### **Phase 4: Live Data Pipeline**

* **Objective:** Transition the engine from historical flat-file parsing to real-time market data processing.  
* **Tasks:**  
  * Integrate a C++ networking library (e.g., Boost.Asio).  
  * Connect to the Polygon.io live options WebSocket.  
  * Implement lock-free queues (ring buffers) to allow the networking thread to push live price updates to the pricing threads asynchronously.
