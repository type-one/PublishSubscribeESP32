#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

namespace {

template <template <class...> class Future>
std::vector<Future<int>> make_test_futures(std::initializer_list<int> values) {
  std::vector<Future<int>> futures;
  for (int val : values)
    futures.push_back(pco::make_ready_future(val));
  return futures;
}

/**
 * \brief Verifies extract values from futures for future get transformation.
 */
TEST(future_get_transformation, extract_values_from_futures) {
  std::vector<pco::future<int>> futures =
      make_test_futures<pco::future>({100, 500, 42, 0xff});

  std::vector<int> values;
  values.reserve(futures.size());
  std::transform(futures.begin(), futures.end(), std::back_inserter(values),
                 pco::future_get);
  EXPECT_EQ(values, (std::vector<int>{100, 500, 42, 0xff}));
}

/**
 * \brief Verifies extract values from shared futures for future get transformation.
 */
TEST(future_get_transformation, extract_values_from_shared_futures) {
  std::vector<pco::shared_future<int>> futures =
      make_test_futures<pco::shared_future>({100, 500, 42, 0xff});

  std::vector<int> values;
  values.reserve(futures.size());
  std::transform(futures.begin(), futures.end(), std::back_inserter(values),
                 pco::future_get);
  EXPECT_EQ(values, (std::vector<int>{100, 500, 42, 0xff}));
}

} // namespace
