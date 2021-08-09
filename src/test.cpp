#include "integrator.h"
#include "reverse.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <span>
#include <chrono>
#include <thread>
#include <mutex>
#include <fmt/format.h>


std::chrono::high_resolution_clock::time_point t1;
void start_timer()
{
  t1 = std::chrono::high_resolution_clock::now();
}
void stop_timer()
{
  auto t2 = std::chrono::high_resolution_clock::now();

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
  fmt::print("{}\n", dur.count());
}

template<typename F>
double derivative(const F& func, double x)
{
  constexpr double eps = std::numeric_limits<double>::epsilon();
  double dx = std::cbrt(eps);

  return (func(x+dx) - func(x-dx)) / (2.0*dx);
}

using Point = std::pair<double, double>;

bool is_correct_integral(std::vector<integrator::MemberFuncPtr>& compiled_expr,
                integrator::Composer& composer,
                std::span<const Point> integrand_points)
{
    constexpr auto loss_cutoff = 1.0e-10;

    double loss = 0.0;
    for (const auto& point : integrand_points) {
      auto x = point.first;
      auto y = point.second;
      double pred_deriv = derivative([&composer, &compiled_expr](double x) {
          return composer.eval(compiled_expr, x); },
          x);

      double delta = pred_deriv - y;
      loss += delta * delta;
    }

    return loss < loss_cutoff;
}

// The function to be integrated
double func(double x)
{
  return x / std::tan(x);
}

std::pair<std::string, unsigned long>
search(const std::vector<Point>& integrand_points,
       unsigned int seed,
       unsigned int num_threads,
       unsigned long max_attempts)
{
  auto result_mtx = std::mutex{};
  auto result_str = std::string{};
  auto num_attempts = 0ul;

  if (max_attempts == 0) {
    max_attempts = std::numeric_limits<decltype(max_attempts)>::max();
  }

  auto searcher =
    [&result_mtx, &result_str, &num_attempts, &max_attempts, &integrand_points]
    (unsigned int seed)
    {
      integrator::Composer composer(seed);

      constexpr auto N = 10000;
      while (true) {
        for (int attempt = 1; attempt < N; ++attempt) {
          auto [raw_expr, compiled_expr] = composer.compose(20);
          if (is_correct_integral(compiled_expr, composer, integrand_points)) {
            auto lk = std::scoped_lock{result_mtx};
            num_attempts += attempt;
            if (result_str.empty()) {
              result_str = raw_expr;
            }
            return;
          }
        }
        auto lk = std::scoped_lock{result_mtx};
        num_attempts += N;
        if (!result_str.empty() || num_attempts > max_attempts) {
          return;
        }
      }

    };

  srand(seed);
  auto workers = std::vector<std::thread>{};
  for (auto i = 0u; i < num_threads; ++i) {
    workers.emplace_back(std::thread(searcher, rand()));
  }

  for (auto& t : workers) {
    t.join();
  }

  return {result_str, num_attempts};
}


int main()
{
  std::vector<double> xs = {0.2, 0.5, 0.9, 1.5, 2.0};
  auto points = std::vector<Point>{};
  std::transform(xs.begin(), xs.end(),
                 std::back_inserter(points),
                 [](auto x) { return Point{x, func(x)}; });


  start_timer();
  auto [str, attempts] = search(points, 4, 4, 100'000'000);
  stop_timer();
  // auto [str, attempts] = search(points, 4, 8, 100'000'000);
  fmt::print("{}\n{}\n", str, attempts);
  if (!str.empty()) {
    fmt::print("{}\n", infix_from_reverse_polish(str));
  }

}


