#include <iostream>
#include <vector>
#define _USE_MATH_DEFINES
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <iomanip>
#include <random>
#include <thread>

// --- Core Data Structures ---

enum class OptionType { Call, Put };
enum class ExerciseStyle { European, American };

struct OptionParams {
  double S; // Spot price
  double K; // Strike price
  double T; // Time to maturity (in years)
  double r; // Risk-free interest rate
  double v; // Volatility
  OptionType type;
  ExerciseStyle style;
};

struct Greeks {
  double delta;
  double gamma;
  double vega;
  double theta;
  double rho;
};

struct PricingResult {
  double price;
  Greeks greeks;
};

// --- Math Utilities ---

const double PI = 3.14159265358979323846;

// Standard Normal Probability Density Function
inline double norm_pdf(double x) {
  return std::exp(-0.5 * x * x) / std::sqrt(2.0 * PI);
}

// Standard Normal Cumulative Distribution Function
inline double norm_cdf(double x) { return 0.5 * std::erfc(-x * M_SQRT1_2); }

// --- Pricing Models ---

class BlackScholes {
public:
  static PricingResult price(const OptionParams &p) {
    PricingResult result;

    // Handle expiration cases safely
    if (p.T <= 0.0) {
      result.price = (p.type == OptionType::Call) ? std::max(0.0, p.S - p.K)
                                                  : std::max(0.0, p.K - p.S);
      result.greeks = {0, 0, 0, 0, 0};
      return result;
    }

    double sqrtT = std::sqrt(p.T);
    double d1 =
        (std::log(p.S / p.K) + (p.r + 0.5 * p.v * p.v) * p.T) / (p.v * sqrtT);
    double d2 = d1 - p.v * sqrtT;

    double Nd1 = norm_cdf(d1);
    double Nd2 = norm_cdf(d2);
    double Nd1_prime = norm_pdf(d1);

    double exp_rT = std::exp(-p.r * p.T);

    if (p.type == OptionType::Call) {
      result.price = p.S * Nd1 - p.K * exp_rT * Nd2;

      // Call Greeks
      result.greeks.delta = Nd1;
      result.greeks.theta =
          -(p.S * Nd1_prime * p.v) / (2.0 * sqrtT) - p.r * p.K * exp_rT * Nd2;
      result.greeks.rho = p.K * p.T * exp_rT * Nd2;
    } else {
      double N_minus_d1 = norm_cdf(-d1);
      double N_minus_d2 = norm_cdf(-d2);

      result.price = p.K * exp_rT * N_minus_d2 - p.S * N_minus_d1;

      // Put Greeks
      result.greeks.delta = Nd1 - 1.0;
      result.greeks.theta = -(p.S * Nd1_prime * p.v) / (2.0 * sqrtT) +
                            p.r * p.K * exp_rT * N_minus_d2;
      result.greeks.rho = -p.K * p.T * exp_rT * N_minus_d2;
    }

    // Gamma and Vega are identical for Calls and Puts
    result.greeks.gamma = Nd1_prime / (p.S * p.v * sqrtT);
    result.greeks.vega = p.S * Nd1_prime * sqrtT;

    return result;
  }
};

class BinomialTree {
public:
  // Cox-Ross-Rubinstein (CRR) model for Option Pricing
  static double price(const OptionParams &p, int steps = 500) {
    if (p.T <= 0.0)
      return (p.type == OptionType::Call) ? std::max(0.0, p.S - p.K)
                                          : std::max(0.0, p.K - p.S);

    double dt = p.T / steps;
    double u = std::exp(p.v * std::sqrt(dt));
    double d = 1.0 / u;
    double p_prob = (std::exp(p.r * dt) - d) / (u - d);
    double discount = std::exp(-p.r * dt);

    // Pre-allocate value array for the tree leaf nodes
    std::vector<double> values(steps + 1);

    // Initialize leaf nodes at maturity
    for (int i = 0; i <= steps; ++i) {
      double S_T = p.S * std::pow(u, steps - i) * std::pow(d, i);
      values[i] = (p.type == OptionType::Call) ? std::max(0.0, S_T - p.K)
                                               : std::max(0.0, p.K - S_T);
    }

    // Step backwards through the tree
    for (int step = steps - 1; step >= 0; --step) {
      for (int i = 0; i <= step; ++i) {
        // Calculate expected value
        values[i] =
            discount * (p_prob * values[i] + (1.0 - p_prob) * values[i + 1]);

        // American option early exercise check
        if (p.style == ExerciseStyle::American) {
          double S_t = p.S * std::pow(u, step - i) * std::pow(d, i);
          double exercise_val = (p.type == OptionType::Call)
                                    ? std::max(0.0, S_t - p.K)
                                    : std::max(0.0, p.K - S_t);
          values[i] = std::max(values[i], exercise_val);
        }
      }
    }

    return values[0];
  }
};

// --- Multi-Threaded Pricing Engine ---

class ParallelPricingEngine {
public:
  // Prices a portfolio using Black-Scholes concurrently
  static std::vector<PricingResult>
  priceBatchBS(const std::vector<OptionParams> &portfolio) {
    return executeParallel(portfolio, [](const OptionParams &p) {
      return BlackScholes::price(p);
    });
  }

  // Prices a portfolio using Binomial Tree concurrently (Prices only, leaving
  // Greeks null for simplicity in this demo)
  static std::vector<PricingResult>
  priceBatchBinomial(const std::vector<OptionParams> &portfolio,
                     int steps = 200) {
    return executeParallel(portfolio, [steps](const OptionParams &p) {
      PricingResult res;
      res.price = BinomialTree::price(p, steps);
      res.greeks = {0, 0, 0, 0, 0};
      return res;
    });
  }

private:
  // Core template for chunked parallel execution
  template <typename Func>
  static std::vector<PricingResult>
  executeParallel(const std::vector<OptionParams> &portfolio,
                  Func pricingFunction) {
    size_t total_options = portfolio.size();
    std::vector<PricingResult> results(total_options);

    // Determine optimal number of threads and chunk size
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
      num_threads = 4; // Fallback

    size_t chunk_size = total_options / num_threads;
    if (chunk_size == 0) {
      chunk_size = total_options;
      num_threads = 1;
    }

    std::vector<std::future<void>> futures;

    for (size_t t = 0; t < num_threads; ++t) {
      size_t start_idx = t * chunk_size;
      size_t end_idx =
          (t == num_threads - 1) ? total_options : start_idx + chunk_size;

      // Dispatch chunk to a worker thread via std::async
      futures.push_back(
          std::async(std::launch::async, [start_idx, end_idx, &portfolio,
                                          &results, pricingFunction]() {
            for (size_t i = start_idx; i < end_idx; ++i) {
              results[i] = pricingFunction(portfolio[i]);
            }
          }));
    }

    // Wait for all threads to complete
    for (auto &f : futures) {
      f.get();
    }

    return results;
  }
};

// --- Test and Benchmark ---

std::vector<OptionParams> generateRandomPortfolio(size_t size) {
  std::vector<OptionParams> portfolio(size);

  // Setup random generation
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> d_S(50.0, 150.0);
  std::uniform_real_distribution<> d_K(50.0, 150.0);
  std::uniform_real_distribution<> d_T(0.1, 3.0);
  std::uniform_real_distribution<> d_r(0.01, 0.05);
  std::uniform_real_distribution<> d_v(0.1, 0.5);
  std::uniform_int_distribution<> d_type(0, 1);

  for (size_t i = 0; i < size; ++i) {
    portfolio[i] = {d_S(gen),
                    d_K(gen),
                    d_T(gen),
                    d_r(gen),
                    d_v(gen),
                    d_type(gen) == 0 ? OptionType::Call : OptionType::Put,
                    ExerciseStyle::European};
  }
  return portfolio;
}

int main() {
  std::cout << "=========================================\n";
  std::cout << " High-Performance Option Pricing Engine  \n";
  std::cout << "=========================================\n\n";

  const size_t PORTFOLIO_SIZE = 1'000'000;
  std::cout << "Generating portfolio of " << PORTFOLIO_SIZE
            << " random options...\n";
  auto portfolio = generateRandomPortfolio(PORTFOLIO_SIZE);

  // --- Benchmark Black-Scholes Sequential ---
  std::cout << "\nBenchmarking Black-Scholes (Sequential)...\n";
  auto start_seq = std::chrono::high_resolution_clock::now();

  std::vector<PricingResult> seq_results(PORTFOLIO_SIZE);
  for (size_t i = 0; i < PORTFOLIO_SIZE; ++i) {
    seq_results[i] = BlackScholes::price(portfolio[i]);
  }

  auto end_seq = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> seq_time = end_seq - start_seq;
  std::cout << "Sequential Time: " << seq_time.count() << " ms\n";

  // --- Benchmark Black-Scholes Parallel ---
  std::cout << "\nBenchmarking Black-Scholes (Parallel - "
            << std::thread::hardware_concurrency() << " cores)...\n";
  auto start_par = std::chrono::high_resolution_clock::now();

  auto par_results = ParallelPricingEngine::priceBatchBS(portfolio);

  auto end_par = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> par_time = end_par - start_par;
  std::cout << "Parallel Time:   " << par_time.count() << " ms\n";
  std::cout << "Speedup Factor:  " << std::fixed << std::setprecision(2)
            << (seq_time.count() / par_time.count()) << "x\n";

  // --- Benchmark Binomial Tree Parallel ---
  // Note: We test on a smaller subset due to the O(N^2) complexity of the tree.
  const size_t BINOMIAL_PORTFOLIO_SIZE = 10'000;
  const int TREE_STEPS = 250;
  std::vector<OptionParams> small_portfolio(
      portfolio.begin(), portfolio.begin() + BINOMIAL_PORTFOLIO_SIZE);

  std::cout << "\nBenchmarking Binomial Tree (" << TREE_STEPS << " steps, "
            << BINOMIAL_PORTFOLIO_SIZE << " options, Parallel)...\n";
  auto start_bin = std::chrono::high_resolution_clock::now();

  auto bin_results =
      ParallelPricingEngine::priceBatchBinomial(small_portfolio, TREE_STEPS);

  auto end_bin = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> bin_time = end_bin - start_bin;
  std::cout << "Binomial Time:   " << bin_time.count() << " ms\n";

  // --- Verify Output ---
  std::cout << "\nSample Output (Option 0):\n";
  std::cout << "Spot: " << portfolio[0].S << ", Strike: " << portfolio[0].K
            << ", Type: "
            << (portfolio[0].type == OptionType::Call ? "Call" : "Put") << "\n";
  std::cout << "BS Price: " << par_results[0].price << "\n";
  std::cout << "Delta: " << par_results[0].greeks.delta << "\n";
  std::cout << "Gamma: " << par_results[0].greeks.gamma << "\n";
  std::cout << "Binomial Tree Price: " << bin_results[0].price << "\n";

  return 0;
}