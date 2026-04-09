#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "portable_concurrency/bits/alias_namespace.h"
#include "portable_concurrency/bits/either.h"

namespace portable_concurrency {
namespace test {
namespace {

static_assert(!std::is_copy_constructible<
                  detail::either<detail::monostate, int, std::string>>::value,
              "either should not be copyable");
static_assert(!std::is_copy_assignable<
                  detail::either<detail::monostate, int, std::string>>::value,
              "either should not be copyable");

// Visitation test first since all access is going to happen through visitors.
// If allmost all test fails one should debug visitation tests first

struct bad_variant_access : std::exception {
  const char *what() const noexcept override { return "bad_variant_access"; }
};

template <typename T> struct get_visitor {
  T &operator()(T &val) const { return val; }

  template <typename U> T &operator()(const U &) const {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    throw bad_variant_access{};
#else
    std::terminate();
#endif
  }
};

/**
 * \brief Verifies visitor access empty for either.
 */
TEST(Either, visitor_access_empty) {
  const detail::either<detail::monostate, int> either{};
  struct {
    bool operator()(detail::monostate) { return true; }
    bool operator()(int) { return false; }
  } visitor;
  EXPECT_TRUE(either.visit(visitor));
}

/**
 * \brief Verifies visitor access first for either.
 */
TEST(Either, visitor_access_first) {
  const detail::either<detail::monostate, int, std::string,
                       std::unique_ptr<double>>
      either{detail::in_place_index_t<1>{}, 42};
  EXPECT_EQ(either.visit(get_visitor<const int>{}), 42);
}

/**
 * \brief Verifies visitor access second for either.
 */
TEST(Either, visitor_access_second) {
  detail::either<detail::monostate, int, std::string, std::unique_ptr<double>>
      either{detail::in_place_index_t<2>{}, "42"};
  EXPECT_EQ(either.visit(get_visitor<std::string>{}), "42");
}

/**
 * \brief Verifies visitor access third for either.
 */
TEST(Either, visitor_access_third) {
  const detail::either<detail::monostate, int, std::string,
                       std::unique_ptr<double>>
      either{detail::in_place_index_t<3>{}, std::make_unique<double>(3.14)};
  EXPECT_EQ(*either.visit(get_visitor<const std::unique_ptr<double>>{}), 3.14);
}

template <typename T, typename E> T &get(E &either) {
  return either.visit(get_visitor<T>{});
}

template <typename T, typename E> const T &get(const E &either) {
  return either.visit(get_visitor<const T>{});
}

/**
 * \brief Verifies visitor modifies first for either.
 */
TEST(Either, visitor_modifies_first) {
  detail::either<detail::monostate, int> either{detail::in_place_index_t<1>{},
                                                100500};
  struct {
    void operator()(detail::monostate) {}
    void operator()(int &val) { val += 42; }
  } visitor;
  either.visit(visitor);
  EXPECT_EQ(get<int>(either), 100542);
}

/**
 * \brief Verifies default constructed is empty for either.
 */
TEST(Either, default_constructed_is_empty) {
  detail::either<detail::monostate, int, std::string> either;
  EXPECT_TRUE(either.empty());
}

/**
 * \brief Verifies first value constructed has correct state for either.
 */
TEST(Either, first_value_constructed_has_correct_state) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<1>{}, 42};
  EXPECT_EQ(either.state(), 1u);
}

/**
 * \brief Verifies second value constructed has correct state for either.
 */
TEST(Either, second_value_constructed_has_correct_state) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<2>{}, "42"};
  EXPECT_EQ(either.state(), 2u);
}

/**
 * \brief Verifies moved from is empty for either.
 */
TEST(Either, moved_from_is_empty) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<2>{}, "42"};
  detail::either<detail::monostate, int, std::string> either2 =
      std::move(either);
  EXPECT_TRUE(either.empty());
}

/**
 * \brief Verifies moved to same state is first for first src for either.
 */
TEST(Either, moved_to_same_state_is_first_for_first_src) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<1>{}, 42};
  detail::either<detail::monostate, int, std::string> either2 =
      std::move(either);
  EXPECT_EQ(either2.state(), 1u);
}

/**
 * \brief Verifies moved to state is second for second src for either.
 */
TEST(Either, moved_to_state_is_second_for_second_src) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<2>{}, "42"};
  detail::either<detail::monostate, int, std::string> either2 =
      std::move(either);
  EXPECT_EQ(either2.state(), 2u);
}

/**
 * \brief Verifies moved to is empty for empty src for either.
 */
TEST(Either, moved_to_is_empty_for_empty_src) {
  detail::either<detail::monostate, int, std::string> either;
  detail::either<detail::monostate, int, std::string> either2 =
      std::move(either);
  EXPECT_TRUE(either2.empty());
}

/**
 * \brief Verifies empty then emplaced with first is first for either.
 */
TEST(Either, empty_then_emplaced_with_first_is_first) {
  detail::either<detail::monostate, int, std::string> either;
  either.emplace(detail::in_place_index_t<1>{}, 42);
  EXPECT_EQ(either.state(), 1u);
}

/**
 * \brief Verifies empty then emplaced with second is second for either.
 */
TEST(Either, empty_then_emplaced_with_second_is_second) {
  detail::either<detail::monostate, int, std::string> either;
  either.emplace(detail::in_place_index_t<1>{}, 42);
  EXPECT_EQ(either.state(), 1u);
}

/**
 * \brief Verifies switched from first to second is second for either.
 */
TEST(Either, switched_from_first_to_second_is_second) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<1>{}, 324};
  either.emplace(detail::in_place_index_t<2>{}, "42");
  EXPECT_EQ(either.state(), 2u);
}

/**
 * \brief Verifies switched from secons to first is first for either.
 */
TEST(Either, switched_from_secons_to_first_is_first) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<2>{}, "324"};
  either.emplace(detail::in_place_index_t<1>{}, 42);
  EXPECT_EQ(either.state(), 1u);
}

/**
 * \brief Verifies get value on cunstructed as first for either.
 */
TEST(Either, get_value_on_cunstructed_as_first) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<1>{}, 42};
  EXPECT_EQ(get<int>(either), 42);
}

/**
 * \brief Verifies get value on cunstructed as second for either.
 */
TEST(Either, get_value_on_cunstructed_as_second) {
  detail::either<detail::monostate, int, std::string> either{
      detail::in_place_index_t<2>{}, "help"};
  EXPECT_EQ(get<std::string>(either), "help");
}

/**
 * \brief Verifies get value on emplaced with first for either.
 */
TEST(Either, get_value_on_emplaced_with_first) {
  detail::either<detail::monostate, int, std::string> either;
  either.emplace(detail::in_place_index_t<1>{}, 42);
  EXPECT_EQ(get<int>(either), 42);
}

/**
 * \brief Verifies get first value after move for either.
 */
TEST(Either, get_first_value_after_move) {
  detail::either<detail::monostate, int, std::string> either_old{
      detail::in_place_index_t<1>{}, 42};
  detail::either<detail::monostate, int, std::string> either_new =
      std::move(either_old);
  EXPECT_EQ(get<int>(either_new), 42);
}

/**
 * \brief Verifies get second value after move for either.
 */
TEST(Either, get_second_value_after_move) {
  detail::either<detail::monostate, int, std::string> either_old{
      detail::in_place_index_t<2>{}, "voodo"};
  detail::either<detail::monostate, int, std::string> either_new =
      std::move(either_old);
  EXPECT_EQ(get<std::string>(either_new), "voodo");
}

struct EitherCleanup : ::testing::Test {
  using sptr_t = std::shared_ptr<int>;
  std::shared_ptr<int> sval = std::make_shared<int>(100500);
  std::weak_ptr<int> wval = sval;
};

/**
 * \brief Verifies switch from first to second destroys old value for either cleanup.
 */
TEST_F(EitherCleanup, switch_from_first_to_second_destroys_old_value) {
  detail::either<detail::monostate, sptr_t, std::string> either{
      detail::in_place_index_t<1>{}, std::move(sval)};
  either.emplace(detail::in_place_index_t<2>{}, "Hello");
  EXPECT_TRUE(wval.expired());
}

/**
 * \brief Verifies switch from second to first destroys old value for either cleanup.
 */
TEST_F(EitherCleanup, switch_from_second_to_first_destroys_old_value) {
  detail::either<detail::monostate, std::string, sptr_t> either{
      detail::in_place_index_t<2>{}, std::move(sval)};
  either.emplace(detail::in_place_index_t<1>{}, "Hello");
  EXPECT_TRUE(wval.expired());
}

/**
 * \brief Verifies destructor destroys stored first for either cleanup.
 */
TEST_F(EitherCleanup, destructor_destroys_stored_first) {
  {
    detail::either<detail::monostate, sptr_t, std::string> either{
        detail::in_place_index_t<1>{}, std::move(sval)};
  }
  EXPECT_TRUE(wval.expired());
}

/**
 * \brief Verifies destructor destroys stored second for either cleanup.
 */
TEST_F(EitherCleanup, destructor_destroys_stored_second) {
  {
    detail::either<detail::monostate, std::string, sptr_t> either{
        detail::in_place_index_t<2>{}, std::move(sval)};
  }
  EXPECT_TRUE(wval.expired());
}

// exception safety
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
struct surprize : std::exception {
  const char *what() const noexcept override { return "surprize"; };
};

class throwing {
public:
  throwing() { throw surprize{}; }
};

/**
 * \brief Verifies remains empty on exception from ctor called by emplace for either.
 */
TEST(Either, remains_empty_on_exception_from_ctor_called_by_emplace) {
  detail::either<detail::monostate, throwing> either;
  ASSERT_THROW(either.emplace(detail::in_place_index_t<1>{}), surprize);
  EXPECT_TRUE(either.empty());
}

/**
 * \brief Verifies switches to empty on exception from ctor called by emplace for either.
 */
TEST(Either, switches_to_empty_on_exception_from_ctor_called_by_emplace) {
  detail::either<detail::monostate, int, throwing> either{
      detail::in_place_index_t<1>{}, 42};
  ASSERT_THROW(either.emplace(detail::in_place_index_t<2>{}), surprize);
  EXPECT_TRUE(either.empty());
}
#endif

} // anonymous namespace
} // namespace test
} // namespace portable_concurrency
