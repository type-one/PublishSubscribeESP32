/**
 * @file math.hpp
 * @brief Mathematical functions for `fpm::fixed`: classification, rounding, exponential,
 *        logarithm, power, and trigonometric functions.
 * @author Mike Lankamp
 * @date May 2019
 */

#ifndef FPM_MATH_HPP
#define FPM_MATH_HPP

#include "fixed.hpp"
#include <cmath>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace fpm
{

    //
    // Helper functions
    //
    namespace detail
    {

        static constexpr int k_bits_per_byte = 8;

        // Returns the index of the most-signifcant set bit
        /** @brief Returns the bit position of the highest set bit in `value`. */
        inline long find_highest_bit(unsigned long long value) noexcept
        {
            assert(value != 0);
#if defined(_MSC_VER)
            unsigned long index;
#if defined(_WIN64)
            _BitScanReverse64(&index, value);
#else
            if (_BitScanReverse(&index, static_cast<unsigned long>(value >> 32)) != 0)
            {
                index += 32;
            }
            else
            {
                _BitScanReverse(&index, static_cast<unsigned long>(value & 0xfffffffflu));
            }
#endif
            return index;
#elif defined(__GNUC__) || defined(__clang__)
            const auto bit_width = static_cast<long>(sizeof(value)) * k_bits_per_byte;
            return bit_width - 1L - static_cast<long>(__builtin_clzll(value));
#else
#error "your platform does not support find_highest_bit()"
#endif
        }

    }

    //
    // Classification methods
    //

    /** @brief Returns the floating-point classification of a fixed-point value (FP_ZERO or FP_NORMAL). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr int fpclassify(fixed<B, I, F, R> input_value) noexcept
    {
        return (input_value.raw_value() == 0) ? FP_ZERO : FP_NORMAL;
    }

    /** @brief Returns `true`; fixed-point numbers are always finite. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isfinite(fixed<B, I, F, R> input_value) noexcept
    {
        (void)input_value;
        return true;
    }

    /** @brief Returns `false`; fixed-point numbers are never infinite. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isinf(fixed<B, I, F, R> input_value) noexcept
    {
        (void)input_value;
        return false;
    }

    /** @brief Returns `false`; fixed-point numbers are never NaN. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isnan(fixed<B, I, F, R> input_value) noexcept
    {
        (void)input_value;
        return false;
    }

    /** @brief Returns `true` when the value is non-zero (fixed-point has no subnormals). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isnormal(fixed<B, I, F, R> input_value) noexcept
    {
        return input_value.raw_value() != 0;
    }

    /** @brief Returns `true` when the value is negative. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool signbit(fixed<B, I, F, R> input_value) noexcept
    {
        return input_value.raw_value() < 0;
    }

    /** @brief Returns `true` when `x > y` (always ordered for fixed-point). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isgreater(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return lhs_value > rhs_value;
    }

    /** @brief Returns `true` when `x >= y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isgreaterequal(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return lhs_value >= rhs_value;
    }

    /** @brief Returns `true` when `x < y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isless(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return lhs_value < rhs_value;
    }

    /** @brief Returns `true` when `x <= y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool islessequal(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return lhs_value <= rhs_value;
    }

    /** @brief Returns `true` when `x < y` or `x > y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool islessgreater(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return lhs_value != rhs_value;
    }

    /** @brief Returns `false`; fixed-point comparisons are always ordered. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr bool isunordered(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        (void)lhs_value;
        (void)rhs_value;
        return false;
    }

    //
    // Nearest integer operations
    //
    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> ceil(fixed<B, I, F, R> input_value) noexcept
    {
        constexpr auto FRAC = B(1) << F;
        auto value = input_value.raw_value();
        if (value > 0)
        {
            value += FRAC - 1;
        }
        return fixed<B, I, F, R>::from_raw_value(value / FRAC * FRAC);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> floor(fixed<B, I, F, R> input_value) noexcept
    {
        constexpr auto FRAC = B(1) << F;
        auto value = input_value.raw_value();
        if (value < 0)
        {
            value -= FRAC - 1;
        }
        return fixed<B, I, F, R>::from_raw_value(value / FRAC * FRAC);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> trunc(fixed<B, I, F, R> input_value) noexcept
    {
        constexpr auto FRAC = B(1) << F;
        return fixed<B, I, F, R>::from_raw_value(input_value.raw_value() / FRAC * FRAC);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> round(fixed<B, I, F, R> input_value) noexcept
    {
        constexpr auto FRAC = B(1) << F;
        auto value = input_value.raw_value() / (FRAC / 2);
        return fixed<B, I, F, R>::from_raw_value(((value / 2) + (value % 2)) * FRAC);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> nearbyint(fixed<B, I, F, R> input_value) noexcept
    {
        // Rounding mode is assumed to be FE_TONEAREST
        constexpr auto FRAC = B(1) << F;
        auto value = input_value.raw_value();
        const bool is_half = std::abs(value % FRAC) == FRAC / 2;
        value /= FRAC / 2;
        value = (value / 2) + (value % 2);
        value -= (value % 2) * is_half;
        return fixed<B, I, F, R>::from_raw_value(value * FRAC);
    }

    /** @brief Rounds to the nearest integer using tie-to-even (banker's rounding). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> rint(fixed<B, I, F, R> input_value) noexcept
    {
        // Rounding mode is assumed to be FE_TONEAREST
        return nearbyint(input_value);
    }

    //
    // Mathematical functions
    //
    /** @brief Returns the absolute value. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> abs(fixed<B, I, F, R> input_value) noexcept
    {
        return (input_value >= fixed<B, I, F, R> { 0 }) ? input_value : -input_value;
    }

    /** @brief Returns the floating-point remainder of `x / y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> fmod(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return assert(rhs_value.raw_value() != 0),
               fixed<B, I, F, R>::from_raw_value(lhs_value.raw_value() % rhs_value.raw_value());
    }

    /** @brief Returns the IEEE remainder of `x / y` (tie rounds to even). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> remainder(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        return assert(rhs_value.raw_value() != 0), lhs_value - nearbyint(lhs_value / rhs_value) * rhs_value;
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> remquo(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value, int* quotient_out) noexcept
    {
        assert(rhs_value.raw_value() != 0);
        assert(quotient_out != nullptr);
        *quotient_out = lhs_value.raw_value() / rhs_value.raw_value();
        return fixed<B, I, F, R>::from_raw_value(lhs_value.raw_value() % rhs_value.raw_value());
    }

    //
    // Manipulation functions
    //

    template <typename B, typename I, unsigned int F, bool R, typename C, typename J, unsigned int G, bool S>
    constexpr fixed<B, I, F, R> copysign(fixed<B, I, F, R> lhs_value, fixed<C, J, G, S> rhs_value) noexcept
    {
        return lhs_value = abs(lhs_value), (rhs_value >= fixed<C, J, G, S> { 0 }) ? lhs_value : -lhs_value;
    }

    /** @brief Returns the next representable value from `x` toward `y`. */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> nextafter(fixed<B, I, F, R> from_value, fixed<B, I, F, R> to_value) noexcept
    {
        if (from_value == to_value)
        {
            return to_value;
        }

        if (to_value > from_value)
        {
            return fixed<B, I, F, R>::from_raw_value(from_value.raw_value() + 1);
        }

        return fixed<B, I, F, R>::from_raw_value(from_value.raw_value() - 1);
    }

    /** @brief Returns the next representable value from `x` toward `y` (identical to `nextafter` for fixed-point). */
    template <typename B, typename I, unsigned int F, bool R>
    constexpr fixed<B, I, F, R> nexttoward(fixed<B, I, F, R> from_value, fixed<B, I, F, R> to_value) noexcept
    {
        return nextafter(from_value, to_value);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> modf(fixed<B, I, F, R> input_value, fixed<B, I, F, R>* integer_part_out) noexcept
    {
        const auto raw = input_value.raw_value();
        constexpr auto FRAC = B { 1 } << F;
        *integer_part_out = fixed<B, I, F, R>::from_raw_value(raw / FRAC * FRAC);
        return fixed<B, I, F, R>::from_raw_value(raw % FRAC);
    }


    //
    // Power functions
    //

    template <typename B, typename I, unsigned int F, bool R, typename T,
        typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    fixed<B, I, F, R> pow(fixed<B, I, F, R> base, T exp) noexcept
    {
        using Fixed = fixed<B, I, F, R>;

        if (base == Fixed(0))
        {
            assert(exp > 0);
            return Fixed(0);
        }

        Fixed result { 1 };
        if (exp < 0)
        {
            for (Fixed intermediate = base; exp != 0; exp /= 2, intermediate *= intermediate)
            {
                if ((exp % 2) != 0)
                {
                    result /= intermediate;
                }
            }
        }
        else
        {
            for (Fixed intermediate = base; exp != 0; exp /= 2, intermediate *= intermediate)
            {
                if ((exp % 2) != 0)
                {
                    result *= intermediate;
                }
            }
        }
        return result;
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> pow(fixed<B, I, F, R> base, fixed<B, I, F, R> exp) noexcept
    {
        using Fixed = fixed<B, I, F, R>;

        if (base == Fixed(0))
        {
            assert(exp > Fixed(0));
            return Fixed(0);
        }

        if (exp < Fixed(0))
        {
            return 1 / pow(base, -exp);
        }

        constexpr auto FRAC = B(1) << F;
        if (exp.raw_value() % FRAC == 0)
        {
            // Non-fractional exponents are easier to calculate
            return pow(base, exp.raw_value() / FRAC);
        }

        // For negative bases we do not support fractional exponents.
        // Technically fractions with odd denominators could work,
        // but that's too much work to figure out.
        assert(base > Fixed(0));
        return exp2(log2(base) * exp);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> exp(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        if (input_value < Fixed(0))
        {
            return 1 / exp(-input_value);
        }
        constexpr auto FRAC = B(1) << F;
        const B input_integer = input_value.raw_value() / FRAC;
        input_value -= input_integer;
        assert(input_value >= Fixed(0) && input_value < Fixed(1));

        constexpr auto coeff_a = Fixed::template from_fixed_point<63>(128239257017632854LL);  // 1.3903728105644451e-2
        constexpr auto coeff_b = Fixed::template from_fixed_point<63>(320978614890280666LL);  // 3.4800571158543038e-2
        constexpr auto coeff_c = Fixed::template from_fixed_point<63>(1571680799599592947LL); // 1.7040197373796334e-1
        constexpr auto coeff_d = Fixed::template from_fixed_point<63>(4603349000587966862LL); // 4.9909609871464493e-1
        constexpr auto coeff_e = Fixed::template from_fixed_point<62>(4612052447974689712LL); // 1.0000794567422495
        constexpr auto coeff_f = Fixed::template from_fixed_point<63>(9223361618412247875LL); // 9.9999887043019773e-1
        return pow(Fixed::e(), input_integer)
            * (((((coeff_a * input_value + coeff_b) * input_value + coeff_c) * input_value + coeff_d) * input_value
                   + coeff_e)
                    * input_value
                + coeff_f);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> exp2(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        if (input_value < Fixed(0))
        {
            return 1 / exp2(-input_value);
        }
        constexpr auto FRAC = B(1) << F;
        const B input_integer = input_value.raw_value() / FRAC;
        input_value -= input_integer;
        assert(input_value >= Fixed(0) && input_value < Fixed(1));

        constexpr auto coeff_a = Fixed::template from_fixed_point<63>(17491766697771214LL);   // 1.8964611454333148e-3
        constexpr auto coeff_b = Fixed::template from_fixed_point<63>(82483038782406547LL);   // 8.9428289841091295e-3
        constexpr auto coeff_c = Fixed::template from_fixed_point<63>(515275173969157690LL);  // 5.5866246304520701e-2
        constexpr auto coeff_d = Fixed::template from_fixed_point<63>(2214897896212987987LL); // 2.4013971109076949e-1
        constexpr auto coeff_e = Fixed::template from_fixed_point<63>(6393224161192452326LL); // 6.9315475247516736e-1
        constexpr auto coeff_f = Fixed::template from_fixed_point<63>(9223371050976163566LL); // 9.9999989311082668e-1
        return Fixed(1 << input_integer)
            * (((((coeff_a * input_value + coeff_b) * input_value + coeff_c) * input_value + coeff_d) * input_value
                   + coeff_e)
                    * input_value
                + coeff_f);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> expm1(fixed<B, I, F, R> input_value) noexcept
    {
        return exp(input_value) - 1;
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> log2(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        assert(input_value > Fixed(0));

        // Normalize input to the [1:2] domain
        B value = input_value.raw_value();
        const long highest = detail::find_highest_bit(value);
        const long fraction_bits = static_cast<long>(F);
        if (highest >= fraction_bits)
        {
            value >>= static_cast<unsigned int>(highest - fraction_bits);
        }
        else
        {
            value <<= static_cast<unsigned int>(fraction_bits - highest);
        }
        input_value = Fixed::from_raw_value(value);
        assert(input_value >= Fixed(1) && input_value < Fixed(2));

        constexpr auto coeff_a = Fixed::template from_fixed_point<63>(413886001457275979LL);   // 4.4873610194131727e-2
        constexpr auto coeff_b = Fixed::template from_fixed_point<63>(-3842121857793256941LL); // -4.1656368651734915e-1
        constexpr auto coeff_c = Fixed::template from_fixed_point<62>(7522345947206307744LL);  // 1.6311487636297217
        constexpr auto coeff_d = Fixed::template from_fixed_point<61>(-8187571043052183818LL); // -3.5507929249026341
        constexpr auto coeff_e = Fixed::template from_fixed_point<60>(5870342889289496598LL);  // 5.0917108110420042
        constexpr auto coeff_f = Fixed::template from_fixed_point<61>(-6457199832668582866LL); // -2.8003640347009253
        return Fixed(highest - F)
            + (((((coeff_a * input_value + coeff_b) * input_value + coeff_c) * input_value + coeff_d) * input_value
                   + coeff_e)
                    * input_value
                + coeff_f);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> log(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        return log2(input_value) / log2(Fixed::e());
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> log10(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        static constexpr int decimal_base = 10;
        return log2(input_value) / log2(Fixed(decimal_base));
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> log1p(fixed<B, I, F, R> input_value) noexcept
    {
        return log(1 + input_value);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> cbrt(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;

        if (input_value == Fixed(0))
        {
            return input_value;
        }
        if (input_value < Fixed(0))
        {
            return -cbrt(-input_value);
        }
        assert(input_value >= Fixed(0));

        // Finding the cube root of an integer, taken from Hacker's Delight,
        // based on the square root algorithm.

        // We start at the greatest power of eight that's less than the argument.
        int offset = ((detail::find_highest_bit(input_value.raw_value()) + 2 * F) / 3 * 3);
        I numerator = I { input_value.raw_value() };
        I result = 0;

        const auto do_round = [&]
        {
            for (; offset >= 0; offset -= 3)
            {
                result += result;
                const I trial_value = (3 * result * (result + 1) + 1) << offset;
                if (numerator >= trial_value)
                {
                    numerator -= trial_value;
                    result++;
                }
            }
        };

        // We should shift by 2*F (since there are two multiplications), but that
        // could overflow even the intermediate type, so we have to split the
        // algorithm up in two rounds of F bits each. Each round will deplete
        // 'num' digit by digit, so after a round we can shift it again.
        numerator <<= F;
        offset -= F;
        do_round();

        numerator <<= F;
        offset += F;
        do_round();

        return Fixed::from_raw_value(static_cast<B>(result));
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> sqrt(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;

        assert(input_value >= Fixed(0));
        if (input_value == Fixed(0))
        {
            return input_value;
        }

        // Finding the square root of an integer in base-2, from:
        // https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_.28base_2.29

        // Shift by F first because it's fixed-point.
        I numerator = I { input_value.raw_value() } << F;
        I result = 0;

        // "bit" starts at the greatest power of four that's less than the argument.
        for (I bit = I { 1 } << ((detail::find_highest_bit(input_value.raw_value()) + F) / 2 * 2); bit != 0; bit >>= 2)
        {
            const I trial_value = result + bit;
            result >>= 1;
            if (numerator >= trial_value)
            {
                numerator -= trial_value;
                result += bit;
            }
        }

        // Round the last digit up if necessary
        if (numerator > result)
        {
            result++;
        }

        return Fixed::from_raw_value(static_cast<B>(result));
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> hypot(fixed<B, I, F, R> lhs_value, fixed<B, I, F, R> rhs_value) noexcept
    {
        assert(lhs_value != 0 || rhs_value != 0);
        return sqrt(lhs_value * lhs_value + rhs_value * rhs_value);
    }

    //
    // Trigonometry functions
    //

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> sin(fixed<B, I, F, R> input_value) noexcept
    {
        // This sine uses a fifth-order curve-fitting approximation originally
        // described by Jasper Vijn on coranac.com which has a worst-case
        // relative error of 0.07% (over [-pi:pi]).
        using Fixed = fixed<B, I, F, R>;

        // Turn x from [0..2*PI] domain into [0..4] domain
        input_value = fmod(input_value, Fixed::two_pi());
        input_value = input_value / Fixed::half_pi();

        // Take x modulo one rotation, so [-4..+4].
        if (input_value < Fixed(0))
        {
            input_value += Fixed(4);
        }

        int sign = +1;
        if (input_value > Fixed(2))
        {
            // Reduce domain to [0..2].
            sign = -1;
            input_value -= Fixed(2);
        }

        if (input_value > Fixed(1))
        {
            // Reduce domain to [0..1].
            input_value = Fixed(2) - input_value;
        }

        const Fixed input_squared = input_value * input_value;
        static constexpr int five_constant = 5;
        static constexpr int three_constant = 3;
        static constexpr int two_constant = 2;
        return sign * input_value
            * (Fixed::pi()
                - input_squared * (Fixed::two_pi() - five_constant - input_squared * (Fixed::pi() - three_constant)))
            / two_constant;
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> cos(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        if (input_value > Fixed(0))
        {
            // Prevent an overflow due to the addition of pi/2.
            return sin(input_value - (Fixed::two_pi() - Fixed::half_pi()));
        }
        return sin(Fixed::half_pi() + input_value);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> tan(fixed<B, I, F, R> input_value) noexcept
    {
        const auto cosine_value = cos(input_value);

        // Tangent goes to infinity at 90 and -90 degrees.
        // We can't represent that with fixed-point maths.
        assert(abs(cosine_value).raw_value() > 1);

        return sin(input_value) / cosine_value;
    }

    namespace detail
    {

        // Calculates atan(x) assuming that x is in the range [0,1]
        template <typename B, typename I, unsigned int F, bool R>
        fixed<B, I, F, R> atan_sanitized(fixed<B, I, F, R> input_value) noexcept
        {
            using Fixed = fixed<B, I, F, R>;
            assert(input_value >= Fixed(0) && input_value <= Fixed(1));

            constexpr auto coeff_a = Fixed::template from_fixed_point<63>(716203666280654660LL);   // 0.0776509570923569
            constexpr auto coeff_b = Fixed::template from_fixed_point<63>(-2651115102768076601LL); // -0.287434475393028
            constexpr auto coeff_c = Fixed::template from_fixed_point<63>(9178930894564541004LL);  // 0.995181681698119

            const auto input_squared = input_value * input_value;
            return ((coeff_a * input_squared + coeff_b) * input_squared + coeff_c) * input_value;
        }

        // Calculate atan(y / x), assuming x != 0.
        //
        // If x is very, very small, y/x can easily overflow the fixed-point range.
        // If q = y/x and q > 1, atan(q) would calculate atan(1/q) as intermediate step
        // anyway. We can shortcut that here and avoid the loss of information, thus
        // improving the accuracy of atan(y/x) for very small x.
        template <typename B, typename I, unsigned int F, bool R>
        fixed<B, I, F, R> atan_div(fixed<B, I, F, R> y_value, fixed<B, I, F, R> x_value) noexcept
        {
            using Fixed = fixed<B, I, F, R>;
            assert(x_value != Fixed(0));

            // Make sure y and x are positive.
            // If y / x is negative (when y or x, but not both, are negative), negate the result to
            // keep the correct outcome.
            if (y_value < Fixed(0))
            {
                if (x_value < Fixed(0))
                {
                    return atan_div(-y_value, -x_value);
                }
                return -atan_div(-y_value, x_value);
            }
            if (x_value < Fixed(0))
            {
                return -atan_div(y_value, -x_value);
            }
            assert(y_value >= Fixed(0));
            assert(x_value > Fixed(0));

            if (y_value > x_value)
            {
                return Fixed::half_pi() - detail::atan_sanitized(x_value / y_value);
            }
            return detail::atan_sanitized(y_value / x_value);
        }

    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> atan(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        if (input_value < Fixed(0))
        {
            return -atan(-input_value);
        }

        if (input_value > Fixed(1))
        {
            return Fixed::half_pi() - detail::atan_sanitized(Fixed(1) / input_value);
        }

        return detail::atan_sanitized(input_value);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> asin(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        assert(input_value >= Fixed(-1) && input_value <= Fixed(+1));

        const auto y_squared = Fixed(1) - input_value * input_value;
        if (y_squared == Fixed(0))
        {
            return copysign(Fixed::half_pi(), input_value);
        }
        return detail::atan_div(input_value, sqrt(y_squared));
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> acos(fixed<B, I, F, R> input_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        assert(input_value >= Fixed(-1) && input_value <= Fixed(+1));

        if (input_value == Fixed(-1))
        {
            return Fixed::pi();
        }
        const auto y_squared = Fixed(1) - input_value * input_value;
        return Fixed(2) * detail::atan_div(sqrt(y_squared), Fixed(1) + input_value);
    }

    template <typename B, typename I, unsigned int F, bool R>
    fixed<B, I, F, R> atan2(fixed<B, I, F, R> y_value, fixed<B, I, F, R> x_value) noexcept
    {
        using Fixed = fixed<B, I, F, R>;
        if (x_value == Fixed(0))
        {
            assert(y_value != Fixed(0));
            return (y_value > Fixed(0)) ? Fixed::half_pi() : -Fixed::half_pi();
        }

        auto ret = detail::atan_div(y_value, x_value);

        if (x_value < Fixed(0))
        {
            return (y_value >= Fixed(0)) ? ret + Fixed::pi() : ret - Fixed::pi();
        }
        return ret;
    }

}

#endif
