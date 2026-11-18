/**
 * @file test_check.h
 * @brief Minimal check helpers for the standalone CTest unit tests (Spec 05, RF-2, RF-9).
 *
 * CHECK_NEAR prints expected vs actual and the tolerance on failure and counts failures;
 * a test program returns TEST_RESULT() (0 = pass, 1 = fail).
 */
#ifndef SURROGATE_TEST_CHECK_H
#define SURROGATE_TEST_CHECK_H
#include <cmath>
#include <cstdio>

static int g_failures = 0;
static int g_checks = 0;

/** @brief Check |actual - expected| <= tol; print diagnostics on failure. */
#define CHECK_NEAR(actual, expected, tol, what)                                        \
  do {                                                                                 \
    ++g_checks;                                                                        \
    const double a_ = (actual), e_ = (expected), t_ = (tol);                           \
    if (!(std::fabs(a_ - e_) <= t_) || std::isnan(a_)) {                               \
      ++g_failures;                                                                    \
      std::printf("FAIL %s: expected %.12g, actual %.12g, |diff| %.3g > tol %.3g (%s:%d)\n", \
                  (what), e_, a_, std::fabs(a_ - e_), t_, __FILE__, __LINE__);         \
    }                                                                                  \
  } while (0)

/** @brief Check a boolean condition. */
#define CHECK_TRUE(cond, what)                                                         \
  do {                                                                                 \
    ++g_checks;                                                                        \
    if (!(cond)) { ++g_failures; std::printf("FAIL %s (%s:%d)\n", (what), __FILE__, __LINE__); } \
  } while (0)

/** @brief Print summary and return the process exit code. */
#define TEST_RESULT()                                                                  \
  (std::printf("%d checks, %d failures\n", g_checks, g_failures), g_failures ? 1 : 0)

#endif
