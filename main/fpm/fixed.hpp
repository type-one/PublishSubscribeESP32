#ifndef FPM_FIXED_HPP
#define FPM_FIXED_HPP

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace fpm
{

//! Fixed-point number type
//! \tparam BaseType         the base integer type used to store the fixed-point number. This can be a signed or unsigned type.
//! \tparam IntermediateType the integer type used to store intermediate results during calculations.
//! \tparam FractionBits     the number of bits of the BaseType used to store the fraction
//! \tparam EnableRounding   enable rounding of LSB for multiplication, division, and type conversion
template <typename BaseType, typename IntermediateType, unsigned int FractionBits, bool EnableRounding = true>
class fixed
{
    static constexpr unsigned int k_bits_per_byte = 8U;
    static constexpr unsigned int k_min_integral_bits = 1U;

    static_assert(std::is_integral<BaseType>::value, "BaseType must be an integral type");
    static_assert(FractionBits > 0, "FractionBits must be greater than zero");
    static_assert(
        FractionBits <= sizeof(BaseType) * k_bits_per_byte - k_min_integral_bits,
        "BaseType must at least be able to contain entire fraction, with space for at least one integral bit");
    static_assert(sizeof(IntermediateType) > sizeof(BaseType), "IntermediateType must be larger than BaseType");
    static_assert(
        std::is_signed<IntermediateType>::value == std::is_signed<BaseType>::value,
        "IntermediateType must have same signedness as BaseType");

    // Although this value fits in the BaseType in terms of bits, if there's only one integral bit, this value
    // is incorrect (flips from positive to negative), so we must extend the size to IntermediateType.
    static constexpr IntermediateType FRACTION_MULT = IntermediateType(1) << FractionBits;

    struct raw_construct_tag
    {
    };

    constexpr fixed(BaseType raw_value_value, raw_construct_tag raw_tag) noexcept
        : m_value(raw_value_value)
    {
        (void)raw_tag;
    }

public:
    constexpr fixed() noexcept
        : m_value(0)
    {
    }

    // Converts an integral number to the fixed-point type.
    // Like static_cast, this truncates bits that don't fit.
    template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    constexpr explicit fixed(T input_value) noexcept
        : m_value(static_cast<BaseType>(input_value * FRACTION_MULT))
    {
    }

    // Converts a floating-point number to the fixed-point type.
    // Like static_cast, this truncates bits that don't fit.
    template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    constexpr explicit fixed(T input_value) noexcept
        : m_value(0)
    {
        const auto scaled_value = input_value * static_cast<T>(FRACTION_MULT);
        if constexpr (EnableRounding)
        {
            constexpr T rounding_half = T{0.5};
            const auto rounded_value =
                (input_value >= T{0}) ? (scaled_value + rounding_half) : (scaled_value - rounding_half);
            m_value = static_cast<BaseType>(rounded_value);
        }
        else
        {
            m_value = static_cast<BaseType>(scaled_value);
        }
    }

    // Constructs from another fixed-point type with possibly different underlying representation.
    // Like static_cast, this truncates bits that don't fit.
    template <typename B, typename I, unsigned int F, bool R>
    constexpr explicit fixed(fixed<B, I, F, R> input_value) noexcept
        : m_value(from_fixed_point<F>(input_value.raw_value()).raw_value())
    {
    }

    // Explicit conversion to a floating-point type
    template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    constexpr explicit operator T() const noexcept
    {
        return static_cast<T>(m_value) / FRACTION_MULT;
    }

    // Explicit conversion to an integral type
    template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    constexpr explicit operator T() const noexcept
    {
        return static_cast<T>(m_value / FRACTION_MULT);
    }

    // Returns the raw underlying value of this type.
    // Do not use this unless you know what you're doing.
    [[nodiscard]] constexpr BaseType raw_value() const noexcept
    {
        return m_value;
    }

    //! Constructs a fixed-point number from another fixed-point number.
    //! \tparam NumFractionBits the number of bits used by the fraction in \a value.
    //! \param value the integer fixed-point number
    template <unsigned int NumFractionBits, typename T,
        typename std::enable_if<(NumFractionBits > FractionBits)>::type* = nullptr>
    static constexpr fixed from_fixed_point(T value) noexcept
    {
        // To correctly round the last bit in the result, we need one more bit of information.
        // We do this by multiplying by two before dividing and adding the LSB to the real result.
        if constexpr (EnableRounding)
        {
            return fixed(
                static_cast<BaseType>(
                    value / (T(1) << (NumFractionBits - FractionBits))
                    + (value / (T(1) << (NumFractionBits - FractionBits - 1)) % 2)),
                raw_construct_tag {});
        }
        return fixed(
            static_cast<BaseType>(value / (T(1) << (NumFractionBits - FractionBits))),
            raw_construct_tag {});
    }

    template <unsigned int NumFractionBits, typename T,
        typename std::enable_if<(NumFractionBits <= FractionBits)>::type* = nullptr>
    static constexpr fixed from_fixed_point(T value) noexcept
    {
        return fixed(
            static_cast<BaseType>(value * (T(1) << (FractionBits - NumFractionBits))),
            raw_construct_tag {});
    }

    // Constructs a fixed-point number from its raw underlying value.
    // Do not use this unless you know what you're doing.
    static constexpr fixed from_raw_value(BaseType value) noexcept
    {
        return fixed(value, raw_construct_tag {});
    }

    //! Constructs a fixed-point number from integer part and fraction part.
    //! \tparam NumFraction the fraction in \a fraction_value.
    //! \param integer_value integer part
    //! \param fraction_value fraction value
    template <unsigned long long NumFraction, typename TInt, typename TFraction,
        typename std::enable_if<
            (NumFraction > FRACTION_MULT) && std::is_integral<TInt>::value && std::is_integral<TFraction>::value>::type*
        = nullptr>
    static constexpr fixed from_custom_fraction(TInt integer_value, TFraction fraction_value) noexcept
    {
        const IntermediateType int_part = static_cast<IntermediateType>(integer_value) * (TInt(1) << FractionBits);
        const IntermediateType frac_part =
            static_cast<IntermediateType>(fraction_value) * FRACTION_MULT / static_cast<IntermediateType>(NumFraction);
        const IntermediateType two_frac_part =
            static_cast<IntermediateType>(fraction_value) * FRACTION_MULT * 2 / static_cast<IntermediateType>(NumFraction);

        // To correctly round the last bit in the result, we need one more bit of information.
        // We do this by multiplying by two before dividing and adding the LSB to the real result.
        if constexpr (EnableRounding)
        {
            return fixed(
                static_cast<BaseType>(int_part + frac_part + two_frac_part % 2),
                raw_construct_tag {});
        }
        return fixed(static_cast<BaseType>(int_part + frac_part), raw_construct_tag {});
    }

    template <unsigned long long NumFraction, typename TInt, typename TFraction,
        typename std::enable_if<
            (NumFraction <= FRACTION_MULT) && std::is_integral<TInt>::value && std::is_integral<TFraction>::value>::type*
        = nullptr>
    static constexpr fixed from_custom_fraction(TInt integer_value, TFraction fraction_value) noexcept
    {
        const IntermediateType int_part = static_cast<IntermediateType>(integer_value) * (TInt(1) << FractionBits);
        const IntermediateType frac_part =
            static_cast<IntermediateType>(fraction_value) * FRACTION_MULT / static_cast<IntermediateType>(NumFraction);

        return fixed(static_cast<BaseType>(int_part + frac_part), raw_construct_tag {});
    }

    //
    // Constants
    //
    static constexpr unsigned int k_e_fraction_bits = 61U;
    static constexpr unsigned int k_pi_fraction_bits = 61U;
    static constexpr unsigned int k_half_pi_fraction_bits = 62U;
    static constexpr unsigned int k_two_pi_fraction_bits = 60U;

    static constexpr std::int64_t k_e_scaled = 6267931151224907085LL;
    static constexpr std::int64_t k_pi_scaled = 7244019458077122842LL;

    static constexpr fixed e()
    {
        return from_fixed_point<k_e_fraction_bits>(k_e_scaled);
    }

    static constexpr fixed pi()
    {
        return from_fixed_point<k_pi_fraction_bits>(k_pi_scaled);
    }

    static constexpr fixed half_pi()
    {
        return from_fixed_point<k_half_pi_fraction_bits>(k_pi_scaled);
    }

    static constexpr fixed two_pi()
    {
        return from_fixed_point<k_two_pi_fraction_bits>(k_pi_scaled);
    }

    //
    // Arithmetic member operators
    //

    constexpr fixed operator-() const noexcept
    {
        return fixed::from_raw_value(-m_value);
    }

    fixed& operator+=(const fixed& rhs_value) noexcept
    {
        m_value += rhs_value.m_value;
        return *this;
    }

    template <typename IntType, typename std::enable_if<std::is_integral<IntType>::value>::type* = nullptr>
    fixed& operator+=(IntType rhs_value) noexcept
    {
        m_value += rhs_value * FRACTION_MULT;
        return *this;
    }

    fixed& operator-=(const fixed& rhs_value) noexcept
    {
        m_value -= rhs_value.m_value;
        return *this;
    }

    template <typename IntType, typename std::enable_if<std::is_integral<IntType>::value>::type* = nullptr>
    fixed& operator-=(IntType rhs_value) noexcept
    {
        m_value -= rhs_value * FRACTION_MULT;
        return *this;
    }

    fixed& operator*=(const fixed& rhs_value) noexcept
    {
        if constexpr (EnableRounding)
        {
            // Normal fixed-point multiplication is: lhs * rhs / 2**FractionBits.
            // To correctly round the last bit in the result, we need one more bit of information.
            // We do this by multiplying by two before dividing and adding the LSB to the real result.
            const auto value =
                (static_cast<IntermediateType>(m_value) * rhs_value.m_value) / (FRACTION_MULT / 2);
            m_value = static_cast<BaseType>((value / 2) + (value % 2));
        }
        else
        {
            const auto value = (static_cast<IntermediateType>(m_value) * rhs_value.m_value) / FRACTION_MULT;
            m_value = static_cast<BaseType>(value);
        }
        return *this;
    }

    template <typename IntType, typename std::enable_if<std::is_integral<IntType>::value>::type* = nullptr>
    fixed& operator*=(IntType rhs_value) noexcept
    {
        m_value *= rhs_value;
        return *this;
    }

    fixed& operator/=(const fixed& rhs_value) noexcept
    {
        assert(rhs_value.m_value != 0);
        if constexpr (EnableRounding)
        {
            // Normal fixed-point division is: lhs * 2**FractionBits / rhs.
            // To correctly round the last bit in the result, we need one more bit of information.
            // We do this by multiplying by two before dividing and adding the LSB to the real result.
            const auto value = (static_cast<IntermediateType>(m_value) * FRACTION_MULT * 2) / rhs_value.m_value;
            m_value = static_cast<BaseType>((value / 2) + (value % 2));
        }
        else
        {
            const auto value = (static_cast<IntermediateType>(m_value) * FRACTION_MULT) / rhs_value.m_value;
            m_value = static_cast<BaseType>(value);
        }
        return *this;
    }

    template <typename IntType, typename std::enable_if<std::is_integral<IntType>::value>::type* = nullptr>
    fixed& operator/=(IntType rhs_value) noexcept
    {
        assert(rhs_value != 0);
        m_value /= rhs_value;
        return *this;
    }

private:
    BaseType m_value = 0;
};

//
// Convenience typedefs
//

static constexpr unsigned int k_fraction_bits_16_16 = 16U;
static constexpr unsigned int k_fraction_bits_24_8 = 8U;
static constexpr unsigned int k_fraction_bits_8_24 = 24U;

using fixed_16_16 = fixed<std::int32_t, std::int64_t, k_fraction_bits_16_16>;
using fixed_24_8 = fixed<std::int32_t, std::int64_t, k_fraction_bits_24_8>;
using fixed_8_24 = fixed<std::int32_t, std::int64_t, k_fraction_bits_8_24>;

//
// Addition
//

template <typename B, typename I, unsigned int F, bool R>
constexpr fixed<B, I, F, R> operator+(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) += rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator+(const fixed<B, I, F, R>& lhs_value, T rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) += rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator+(T lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(rhs_value) += lhs_value;
}

//
// Subtraction
//

template <typename B, typename I, unsigned int F, bool R>
constexpr fixed<B, I, F, R> operator-(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) -= rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator-(const fixed<B, I, F, R>& lhs_value, T rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) -= rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator-(T lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) -= rhs_value;
}

//
// Multiplication
//

template <typename B, typename I, unsigned int F, bool R>
constexpr fixed<B, I, F, R> operator*(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) *= rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator*(const fixed<B, I, F, R>& lhs_value, T rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) *= rhs_value;
}

template <typename B, typename I, unsigned int F, bool R, typename T,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator*(T lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(rhs_value) *= lhs_value;
}

//
// Division
//

template <typename B, typename I, unsigned int F, bool R>
constexpr fixed<B, I, F, R> operator/(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) /= rhs_value;
}

template <typename B, typename I, unsigned int F, typename T, bool R,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator/(const fixed<B, I, F, R>& lhs_value, T rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) /= rhs_value;
}

template <typename B, typename I, unsigned int F, typename T, bool R,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr fixed<B, I, F, R> operator/(T lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return fixed<B, I, F, R>(lhs_value) /= rhs_value;
}

//
// Comparison operators
//

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator==(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() == rhs_value.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator!=(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() != rhs_value.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator<(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() < rhs_value.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator>(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() > rhs_value.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator<=(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() <= rhs_value.raw_value();
}

template <typename B, typename I, unsigned int F, bool R>
constexpr bool operator>=(const fixed<B, I, F, R>& lhs_value, const fixed<B, I, F, R>& rhs_value) noexcept
{
    return lhs_value.raw_value() >= rhs_value.raw_value();
}

namespace detail
{
using decimal_digits_temp_type = long long;
static constexpr decimal_digits_temp_type k_log10_2_scaled_8_24 = 5050445LL;
static constexpr int k_scale_shift_bits = 24;
static constexpr int k_rounding_bias = 1;

// Number of base-10 digits required to fully represent a number of bits
static constexpr int max_digits10(int bit_count)
{
    // 8.24 fixed-point equivalent of (int)ceil(bits * std::log10(2));
    return static_cast<int>(
    (decimal_digits_temp_type {bit_count} * k_log10_2_scaled_8_24
     + (decimal_digits_temp_type {1} << k_scale_shift_bits) - k_rounding_bias)
        >> k_scale_shift_bits);
}

// Number of base-10 digits that can be fully represented by a number of bits
static constexpr int digits10(int bit_count)
{
    // 8.24 fixed-point equivalent of (int)(bits * std::log10(2));
    return static_cast<int>((decimal_digits_temp_type {bit_count} * k_log10_2_scaled_8_24) >> k_scale_shift_bits);
}

} // namespace detail
} // namespace fpm

// Specializations for customization points
namespace std
{

template <typename B, typename I, unsigned int F, bool R>
struct hash<fpm::fixed<B, I, F, R>>
{
    using argument_type = fpm::fixed<B, I, F, R>;
    using result_type = std::size_t;

    result_type operator()(argument_type arg) const noexcept(noexcept(std::declval<std::hash<B>>()(arg.raw_value())))
    {
        return m_hash(arg.raw_value());
    }

private:
    std::hash<B> m_hash;
};

template <typename B, typename I, unsigned int F, bool R>
struct numeric_limits<fpm::fixed<B, I, F, R>>
{
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = std::numeric_limits<B>::is_signed;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr std::float_round_style round_style = std::round_to_nearest;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = std::numeric_limits<B>::is_modulo;
    static constexpr int digits = std::numeric_limits<B>::digits;

    // Any number with `digits10` significant base-10 digits (that fits in
    // the range of the type) is guaranteed to be convertible from text and
    // back without change. Worst case, this is 0.000...001, so we can only
    // guarantee this case. Nothing more.
    static constexpr int digits10 = 1;

    // This is equal to max_digits10 for the integer and fractional part together.
    static constexpr int max_digits10 =
        fpm::detail::max_digits10(std::numeric_limits<B>::digits - F) + fpm::detail::max_digits10(F);

    static constexpr int radix = 2;
    static constexpr int min_exponent = 1 - F;
    static constexpr int min_exponent10 = -fpm::detail::digits10(F);
    static constexpr int max_exponent = std::numeric_limits<B>::digits - F;
    static constexpr int max_exponent10 = fpm::detail::digits10(std::numeric_limits<B>::digits - F);
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
    static constexpr int round_error_divisor = 2;

    static constexpr fpm::fixed<B, I, F, R> lowest() noexcept
    {
        return fpm::fixed<B, I, F, R>::from_raw_value(std::numeric_limits<B>::lowest());
    }

    static constexpr fpm::fixed<B, I, F, R> min() noexcept
    {
        return lowest();
    }

    static constexpr fpm::fixed<B, I, F, R> max() noexcept
    {
        return fpm::fixed<B, I, F, R>::from_raw_value(std::numeric_limits<B>::max());
    }

    static constexpr fpm::fixed<B, I, F, R> epsilon() noexcept
    {
        return fpm::fixed<B, I, F, R>::from_raw_value(1);
    }

    static constexpr fpm::fixed<B, I, F, R> round_error() noexcept
    {
        return fpm::fixed<B, I, F, R>(1) / round_error_divisor;
    }

    static constexpr fpm::fixed<B, I, F, R> denorm_min() noexcept
    {
        return min();
    }
};

} // namespace std

namespace fpm
{
template <typename T>
struct is_fixed : std::false_type
{
};

template <typename BaseType, typename IntermediateType, unsigned int FractionBits, bool EnableRounding>
struct is_fixed<fixed<BaseType, IntermediateType, FractionBits, EnableRounding>> : std::true_type
{
};

#if __cplusplus >= 201703L
template <typename T>
inline constexpr bool is_fixed_v = is_fixed<T>::value;
#endif
}

#endif
