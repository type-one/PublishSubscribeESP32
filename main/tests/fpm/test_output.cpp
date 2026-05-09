/**
 * @file test_output.cpp
 * @brief Tests stream output (`operator<<`) formatting for `fpm::fixed`, covering all
 *        format flags, precisions, widths, fill characters, and locales.
 * @author Mike Lankamp
 * @date October 2019
 */

#include "common.hpp"
#include "fpm/ios.hpp"

#include <cfenv>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

using ::testing::Combine;
using ::testing::Values;
using fmtflags = ::std::ios::fmtflags;

using Flags = ::testing::tuple<fmtflags, fmtflags, fmtflags, fmtflags, std::streamsize, int, char, std::locale>;

namespace
{
    class fake_numpunct : public std::numpunct<char>
    {
    public:
        fake_numpunct(char decimal_point, char thousands_sep, std::string grouping)
            : m_decimal_point(decimal_point)
            , m_thousands_sep(thousands_sep)
            , m_grouping(grouping)
        {
        }

    protected:
        char do_decimal_point() const override
        {
            return m_decimal_point;
        }
        char do_thousands_sep() const override
        {
            return m_thousands_sep;
        }
        std::string do_grouping() const override
        {
            return m_grouping;
        }
        std::string do_truename() const override
        {
            return "unused";
        }
        std::string do_falsename() const override
        {
            return "unused";
        }

    private:
        char m_decimal_point;
        char m_thousands_sep;
        std::string m_grouping;
    };

    auto is_output_test_valid(const std::ios_base& stream, double value) -> bool
    {
        const auto floatfield = stream.flags() & std::ios::floatfield;

#if defined(__GLIBCXX__)
        // stdlibc++ seems to have a bug where it applies thousands grouping in hexadecimal mode,
        // and---even worse---applies the grouping through the "0x" prefix. This produces
        // interesting results such as "0,x1.8p+3" instead of "0x1.8p+3" with a grouping
        // of "\002", or "0,x,1.8p+3" with a grouping of "\001".
        const auto locale = stream.getloc();
        const auto& numpunct = std::use_facet<std::numpunct<char>>(locale);
        if (floatfield == (std::ios::fixed | std::ios::scientific) && !numpunct.grouping().empty())
        {
            return false;
        }
#elif defined(_MSC_VER)
        // Microsoft Visual C++ has a few problems:
        // It doesn't properly ignore the specified precision for hexfloat
        if (floatfield == (std::ios::fixed | std::ios::scientific))
        {
            return false;
        }
        // A precision of zero isn't properly handled with a floatfield of "auto" or scientific
        if (stream.precision() == 0 && (floatfield == 0 || floatfield == std::ios::scientific))
        {
            return false;
        }
        // Specifying "showpoint" adds a spurious "0" when the value is 0.
        if (value == 0 && (stream.flags() & std::ios::showpoint) != 0)
        {
            return false;
        }
#endif

        return true;
    }

    auto is_output_test_valid_for_all_values(const std::ios_base& stream) -> bool
    {
        if (!is_output_test_valid(stream, 0.0))
        {
            return false;
        }

#if !defined(_MSC_VER)
        if (!is_output_test_valid(stream, 1.125) || !is_output_test_valid(stream, -1.125)
            || !is_output_test_valid(stream, 0.125) || !is_output_test_valid(stream, -0.125)
            || !is_output_test_valid(stream, 1.0 / 1024.0) || !is_output_test_valid(stream, -1.0 / 1024.0))
        {
            return false;
        }
#endif

        return true;
    }
}

class output : public ::testing::TestWithParam<Flags>
{
protected:
    bool is_test_valid(const std::ios_base& stream, double value) const
    {
        return is_output_test_valid(stream, value);
    }

    void test(double value) const
    {
        using P = fpm::fixed_16_16;

        std::stringstream ss_fixed = create_stream();
        std::stringstream ss_float = create_stream();

        if (!is_test_valid(ss_float, value))
        {
            if (!m_skip_reported)
            {
                m_skip_reported = true;
                GTEST_SKIP() << "Skipping test due to invalid test combination";
            }
            return;
        }

        ss_fixed << P(value);
        ss_float << value;

        // Stream contents should match
        EXPECT_EQ(ss_float.str(), ss_fixed.str()) << "for value: " << value;

        // Stream properties should match afterwards
        EXPECT_EQ(ss_float.flags(), ss_fixed.flags());
        EXPECT_EQ(ss_float.precision(), ss_fixed.precision());
        EXPECT_EQ(ss_float.width(), ss_fixed.width());
        EXPECT_EQ(ss_float.fill(), ss_fixed.fill());
        EXPECT_EQ(ss_float.getloc(), ss_fixed.getloc());
    }

private:
    mutable bool m_skip_reported = false;

    std::stringstream create_stream() const
    {
        std::stringstream ss;
        ss.setf(std::get<0>(GetParam()) | std::get<1>(GetParam()) | std::get<2>(GetParam()) | std::get<3>(GetParam()));
        ss.precision(std::get<4>(GetParam()));
        ss.width(std::get<5>(GetParam()));
        ss.fill(std::get<6>(GetParam()));
        ss.imbue(std::get<7>(GetParam()));
        return ss;
    }
};

/**
 * @brief Verifies formatting of small fixed-point values across output flag combinations.
 */
TEST_P(output, small_numbers)
{
    test(0.0);

#if !defined(_MSC_VER)
    // Microsoft Visual C++ isn't rounding these correctly. See
    // the rounding tests for more information.
    test(1.125);
    test(-1.125);

    test(0.125);
    test(-0.125);

    test(1.0 / 1024.0);
    test(-1.0 / 1024.0);
#endif
}

/**
 * @brief Verifies formatting of large fixed-point values across output flag combinations.
 */
TEST_P(output, large_numbers)
{
    test(16782.0 + 1.0 / 1024.0);
    test(-16782.0 - 1.0 / 1024.0);

    test(100);
    test(1000);
    test(10000);
}

/**
 * @brief Verifies formatting of integral fixed-point values across output flag combinations.
 */
TEST_P(output, integers)
{
    test(0);
    test(1);
    test(-1);
    test(2);
    test(-2);
    test(8);
    test(-8);
    test(10);
    test(-10);
    test(16);
    test(-16);
    test(1024);
    test(-1024);
}

static const std::locale s_fake_locale(std::locale(""), new fake_numpunct(',', '.', "\001\002"));

namespace
{
    auto make_output_flags_params() -> std::vector<Flags>
    {
        std::vector<Flags> params;

        const auto adjust_flags_values = { fmtflags(0), std::ios::left, std::ios::right, std::ios::internal };
        const auto floatfield_values
            = { fmtflags(0), std::ios::scientific, std::ios::fixed, std::ios::scientific | std::ios::fixed };
        const auto showpoint_values = { fmtflags(0), std::ios::showpoint };
        const auto uppercase_values = { fmtflags(0), std::ios::uppercase };
        const auto precision_values
            = { std::streamsize(0), std::streamsize(1), std::streamsize(5), std::streamsize(29), std::streamsize(128) };
        const auto width_values = { 0, 1, 10, 2000 };
        const auto fill_values = { ' ', '*', '0' };
        const auto locale_values = { std::locale("C"), std::locale(""), s_fake_locale };

        for (const auto adjust_flags : adjust_flags_values)
        {
            for (const auto floatfield : floatfield_values)
            {
                for (const auto showpoint : showpoint_values)
                {
                    for (const auto uppercase : uppercase_values)
                    {
                        for (const auto precision : precision_values)
                        {
                            for (const auto width : width_values)
                            {
                                for (const auto fill : fill_values)
                                {
                                    for (const auto& locale : locale_values)
                                    {
                                        std::stringstream stream;
                                        stream.setf(adjust_flags | floatfield | showpoint | uppercase);
                                        stream.precision(precision);
                                        stream.width(width);
                                        stream.fill(fill);
                                        stream.imbue(locale);

                                        if (!is_output_test_valid_for_all_values(stream))
                                        {
                                            continue;
                                        }

                                        params.emplace_back(adjust_flags, floatfield, showpoint, uppercase, precision,
                                            width, fill, locale);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return params;
    }
}

INSTANTIATE_TEST_SUITE_P(output_flags, output, ::testing::ValuesIn(make_output_flags_params()));

using GroupingFlags = ::testing::tuple<fmtflags, std::string>;

class output_grouping : public ::testing::TestWithParam<GroupingFlags>
{
protected:
    void test(double value)
    {
        using P = fpm::fixed_24_8;
        using std::get;

        auto flags = get<0>(GetParam());
        auto grouping = get<1>(GetParam());

#if defined(_MSC_VER)
        // Microsoft Visual C++ does not respect a grouping size < 0 (e.g. "\x200").
        // It it supposed to represent "infinite group" but instead it reuses the last group size
        if (grouping.find('\200') != std::string::npos)
        {
            GTEST_SKIP();
            return;
        }
#endif

        std::locale locale(std::locale("C"), new fake_numpunct('.', ',', grouping));

        std::stringstream ss_fixed, ss_float;
        ss_fixed.imbue(locale);
        ss_fixed.setf(flags);
        ss_float.imbue(locale);
        ss_float.setf(flags);
        ss_fixed << P(value);
        ss_float << value;

        EXPECT_EQ(ss_float.str(), ss_fixed.str()) << "for value: " << value;
    }
};

/**
 * @brief Verifies locale-specific digit grouping in formatted output.
 */
TEST_P(output_grouping, grouping)
{
    test(1);
    test(12);
    test(123);
    test(1234);
    test(12345);
    test(123456);
    test(1234567);
}

// Do not test hexfloat (fixed | scientific) due to stdlibc++ bug (see above)
INSTANTIATE_TEST_SUITE_P(output_grouping, output_grouping,
    Combine(Values(0, std::ios::scientific, std::ios::fixed),
        Values("\003", "\001\001", "\002\001\000", "\002\001\177", "\002\001\200")));

class output_rounding : public ::testing::Test
{
protected:
    void test(double value, int precision, std::ios::fmtflags flags = std::ios::fixed)
    {
        using P = fpm::fixed_16_16;

#if defined(_MSC_VER)
        // Microsoft Visual C++ has a bug where the floating point rounding mode (fesetround) of FE_TONEAREST
        // is not respected in printing functions, instead rounding "away from zero".
        GTEST_SKIP();
        return;
#endif

        std::stringstream ss_fixed, ss_float;
        ss_fixed << std::setiosflags(flags) << std::setprecision(precision) << P(value);
        ss_float << std::setiosflags(flags) << std::setprecision(precision) << value;

        EXPECT_EQ(ss_float.str(), ss_fixed.str()) << "for value: " << value;
    }
};

/**
 * @brief Verifies decimal increment rounding during output formatting.
 */
TEST_F(output_rounding, increment_rounding)
{
    test(9.9, 0);
    test(-9.9, 0);

    test(9.99, 1);
    test(-9.99, 1);

    test(9.9995, 3);
    test(-9.9995, 3);
}

/**
 * @brief Verifies tie-breaking rounding behavior during output.
 */
TEST_F(output_rounding, tie_rounding)
{
    test(0.5, 0);
    test(-0.5, 0);

    test(0.5, 1);
    test(-0.5, 1);

    test(0.25, 1);
    test(-0.25, 1);

    test(0.125, 2);
    test(-0.125, 2);

    test(0.0009765625, 6);
    test(-0.0009765625, 6);

    test(0.0009765625, 7);
    test(-0.0009765625, 7);
}

/**
 * @brief Verifies scientific-format rounding behavior.
 */
TEST_F(output_rounding, scientific_rounding)
{
    test(0.25, 0, std::ios::scientific);
    test(-0.25, 0, std::ios::scientific);

    test(0.5, 0, std::ios::scientific);
    test(-0.5, 0, std::ios::scientific);

    test(0.75, 0, std::ios::scientific);
    test(-0.75, 0, std::ios::scientific);

    test(0.015625, 0, std::ios::scientific);
    test(0.015625, 1, std::ios::scientific);

    test(0.0625, 0, std::ios::scientific);
    test(0.0625, 1, std::ios::scientific);

    test(0.09375, 0, std::ios::scientific);
    test(0.09375, 1, std::ios::scientific);
}

class output_specific : public ::testing::Test
{
protected:
    template <typename Fixed>
    void test(const char* expected, Fixed value, int precision, std::ios::fmtflags flags = std::ios::fixed)
    {
        std::stringstream ss;
        ss << std::setiosflags(flags) << std::setprecision(precision) << value;
        EXPECT_EQ(expected, ss.str());
    }
};

/**
 * @brief Verifies output formatting near representable type limits.
 */
TEST_F(output_specific, type_limit)
{
    using F4 = fpm::fixed<std::int8_t, std::int16_t, 4>;
    using F16 = fpm::fixed_16_16;

    test("-32768.000", F16::from_raw_value(-2147483647 - 1), 3, std::ios::fixed);
    test("-3.277e+04", F16::from_raw_value(-2147483647 - 1), 3, std::ios::scientific);
    test("32768.000", F16::from_raw_value(2147483647), 3, std::ios::fixed);
    test("3.277e+04", F16::from_raw_value(2147483647), 3, std::ios::scientific);

    test("-8.000", F4::from_raw_value(-127 - 1), 3, std::ios::fixed);
    test("-8.000e+00", F4::from_raw_value(-127 - 1), 3, std::ios::scientific);
    test("7.938", F4::from_raw_value(127), 3, std::ios::fixed);
    test("7.938e+00", F4::from_raw_value(127), 3, std::ios::scientific);
}
