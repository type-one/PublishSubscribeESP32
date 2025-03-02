/**
 * @file histogram.hpp
 * @brief A class representing a histogram for counting occurrences of values.
 *
 * This file contains the definition of the histogram class template, which provides
 * functionality for adding values, calculating statistical measures such as average,
 * variance, median, and computing Gaussian probability.
 *
 * @author Laurent Lardinois
 * @date January 2025
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//                                                                             //
// This software is provided 'as-is', without any express or implied           //
// warranty.In no event will the authors be held liable for any damages        //
// arising from the use of this software.                                      //
//                                                                             //
// Permission is granted to anyone to use this software for any purpose,       //
// including commercial applications, and to alter itand redistribute it       //
// freely, subject to the following restrictions :                             //
//                                                                             //
// 1. The origin of this software must not be misrepresented; you must not     //
// claim that you wrote the original software.If you use this software         //
// in a product, an acknowledgment in the product documentation would be       //
// appreciated but is not required.                                            //
// 2. Altered source versions must be plainly marked as such, and must not be  //
// misrepresented as being the original software.                              //
// 3. This notice may not be removed or altered from any source distribution.  //
//-----------------------------------------------------------------------------//

#pragma once

#if !defined(HISTOGRAM_HPP_)
#define HISTOGRAM_HPP_

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "tools/non_copyable.hpp"

#ifndef M_TWO_PI
#define M_TWO_PI (2.0 * 3.14159265358979323846)
#endif

namespace tools
{
    /**
     * @brief A class representing a histogram for counting occurrences of values.
     *
     * @tparam T The type of values stored in the histogram.
     */
    template <typename T>
    class histogram : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        histogram() = default;
        ~histogram() = default;
        struct thread_safe
        {
            static constexpr bool value = false;
        };

        /**
         * @brief Adds a value to the histogram and updates the occurrence count.
         *
         * @param value The value to be added to the histogram.
         */
        void add(const T& value)
        {
            if (0 == m_occurences.count(value)) // first time
            {
                m_occurences.emplace(value, 1);

                if (0 == m_top_occurence)
                {
                    m_top_occurence = 1;
                    m_top_value = value;
                }
            }
            else // found twice at least
            {
                auto found = m_occurences.find(value);
                if (found != m_occurences.end())
                {
                    found->second += 1;

                    if (found->second > m_top_occurence)
                    {
                        m_top_occurence = found->second;
                        m_top_value = value;
                    }
                }
            }

            ++m_total_count;
        }

        /**
         * @brief Returns the top value of the histogram.
         *
         * @return The top value of the histogram.
         */
        constexpr T top() const
        {
            return m_top_value;
        }

        /**
         * @brief Returns the total count of values present in the histogram.
         *
         * @return The total count as an integer.
         */
        [[nodiscard]] constexpr int total_count() const
        {
            return m_total_count;
        }

        /**
         * @brief Returns the highest occurrence count in the histogram.
         *
         * @return The highest occurrence count as an integer.
         */
        [[nodiscard]] constexpr int top_occurence() const
        {
            return m_top_occurence;
        }

        /**
         * @brief Calculates the average value of the histogram.
         *
         * This function computes the average value of the histogram by summing up the
         * products of occurrences and their corresponding values, and then dividing
         * by the total number of occurrences.
         *
         * @return The average value of the histogram as type T. If there are no
         * occurrences, it returns 0.
         */
        T average() const
        {
            double avg = 0.0;
            const auto total = static_cast<double>(m_total_count);

            for (auto itr = m_occurences.cbegin(); itr != m_occurences.cend(); ++itr)
            {
                if (itr->second > 0) // occurence
                {
                    avg += static_cast<double>(itr->second * itr->first);
                }
            }

            return (total > 0.0) ? static_cast<T>(avg / total) : static_cast<T>(0);
        }

        /**
         * @brief Calculates the variance of the data set.
         *
         * This function computes the variance of the data set stored in the histogram.
         *
         * @param average The average value of the data set.
         * @return The variance of the data set.
         */
        T variance(const T& average) const
        {
            double vari = 0.0;
            const auto total = static_cast<double>(m_total_count);

            for (auto itr = m_occurences.cbegin(); itr != m_occurences.cend(); ++itr)
            {
                if (itr->second > 0) // occurence
                {
                    // https://www.calculatorsoup.com/calculators/statistics/standard-deviation-calculator.php
                    const auto dva = static_cast<double>(itr->first) - static_cast<double>(average);
                    vari += itr->second * (dva * dva);
                }
            }

            return (total > 0.0) ? static_cast<T>((vari / total)) : static_cast<T>(0);
        }

        /**
         * @brief Calculates the standard deviation of the data set.
         *
         * This function computes the standard deviation of the data set stored in the histogram.
         *
         * @param variance The variance value of the data set.
         * @return The standard deviation of the data set.
         */
        T standard_deviation(const T& variance) const
        {
            return static_cast<T>(std::sqrt(static_cast<double>(variance)));
        }

        /**
         * @brief Computes the median value of the histogram.
         *
         * This function creates a sorted list of all occurrences and returns the median value.
         * If the histogram is empty, it returns 0.
         *
         * @return The median value of the histogram.
         */
        T median() const
        {
            std::vector<T> to_sort;

            for (auto itr = m_occurences.cbegin(); itr != m_occurences.cend(); ++itr)
            {
                for (int i = 0; i < itr->second; ++i) // occurence
                {
                    to_sort.emplace_back(itr->first);
                }
            }

            if (to_sort.empty())
            {
                return static_cast<T>(0);
            }

            std::sort(to_sort.begin(), to_sort.end());

            // https://www.calculator.net/mean-median-mode-range-calculator.html

            const auto idx = to_sort.size() >> 1;
            T value = static_cast<T>(0);

            if (to_sort.size() & 1U)
            {
                // odd case
                value = static_cast<T>(to_sort[idx]);
            }
            else 
            {
                // even case                
                value = static_cast<T>(0.5 * (to_sort[idx] + to_sort[idx - 1])); // NOLINT math formula
            } 
            
            return value;
        }

        /**
         * @brief Computes the Gaussian probability of a given value.
         *
         * This function calculates the probability of a given value under a Gaussian (normal) distribution
         * characterized by the specified average and variance.
         *
         * @param value The value for which the probability is to be computed.
         * @param average The mean (average) of the Gaussian distribution.
         * @param variance The variance of the Gaussian distribution.
         * @return The probability of the given value under the specified Gaussian distribution.
         */
        double gaussian_probability(const T& value, const T& average, const T& variance) const // NOLINT keep it
        {
            // https://fr.wikipedia.org/wiki/Loi_normale
            double result = 0.0;
            if (variance > static_cast<T>(0))
            {
                const double sigma = std::sqrt(static_cast<double>(variance));
                const double epsilon = static_cast<double>(value - average) / sigma;
                result = std::exp(-0.5 * epsilon * epsilon) / (sigma * std::sqrt(M_TWO_PI)); // NOLINT math formula
            }

            return result;
        }

    private:
        std::unordered_map<T, int> m_occurences;
        int m_total_count = 0;
        int m_top_occurence = 0;
        T m_top_value = static_cast<T>(0);
    };
}

#endif //  HISTOGRAM_HPP_
