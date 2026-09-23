// Extracted from Ardoinus pwe_serial.h; MIT license, see LICENSE.txt.
#pragma once

#include <grevir/time/time.hpp>
#include <grevir/base/compat/cstdint.hpp>
#include <grevir/base/compat/cassert.hpp>
#include <grevir/base/compat/limits.hpp>
#include <grevir/base/compat/type_traits.hpp>

namespace setl {

using xpwe_type = std::uint32_t;
inline constexpr TimeUnit PWE_UNIT = TimeUnit::MICROS;

/**
 * Parameters that describe the waveform.
 */
template <typename w_type, TimeUnit units = PWE_UNIT>
struct PweWaveformParams {
  static_assert(std::is_unsigned_v<w_type> && !std::is_same_v<w_type, bool>,
                "Pulse clocks require an unsigned integral representation");
  using PeriodType = Period<w_type, units>;
  PeriodType settle_period;
  PeriodType stop_period;
  PeriodType bit_on_period;
  PeriodType bit_off_period;
  PeriodType bit_half_period;
  PeriodType bit_min_period;
  PeriodType bit_max_period;

  // Unsigned modular deadlines are unambiguous only within half a clock cycle.
  static constexpr w_type clock_half_range =
    w_type(1) << (std::numeric_limits<w_type>::digits - 1);

  bool valid() const {
    const auto short_bit = bit_on_period.get();
    const auto long_bit = bit_off_period.get();
    const auto max_bit = bit_max_period.get();
    return short_bit > 0 && short_bit < bit_half_period.get()
      && bit_half_period.get() <= long_bit && long_bit <= max_bit
      && short_bit <= max_bit - long_bit
      && bit_min_period.get() > 0
      && bit_min_period.get() <= short_bit + long_bit
      && max_bit < clock_half_range && stop_period.get() >= max_bit
      && stop_period.get() < clock_half_range
      && settle_period.get() < clock_half_range - stop_period.get();
  }
};

namespace pulse_codec_detail {
// Saturating estimates avoid wrapping a long frame's wait hint to a short delay.
template <typename T>
T add(T lhs, T rhs) {
  const auto max = std::numeric_limits<T>::max();
  return rhs > max - lhs ? max : static_cast<T>(lhs + rhs);
}

template <typename T>
T multiply(T value, std::uint16_t count) {
  const auto max = std::numeric_limits<T>::max();
  if (value != 0 && count > max / value) {
    return max;
  }
  return static_cast<T>(value * count);
}
} // namespace pulse_codec_detail

/** Create 1:3 short/long periods with integer rounding toward zero.
 * Both quarter periods are rounded separately, preserving the legacy wire timing.
 * For exact 1:3 timing use a bit period divisible by four.
 */
template <
  typename w_type,
  w_type settle_period,
  w_type stop_period,
  w_type bit_period,
  TimeUnit units = PWE_UNIT>
inline constexpr PweWaveformParams<w_type, units> PweWaveformParams1to3 = [] {
  using Params = PweWaveformParams<w_type, units>;
  using P = typename Params::PeriodType;
  static_assert(bit_period >= 4, "A bit must contain at least four clock ticks");
  static_assert(bit_period / 5 <= std::numeric_limits<w_type>::max() - bit_period,
                "The bit tolerance must fit the clock representation");
  constexpr auto max_bit = static_cast<w_type>(bit_period + bit_period / 5);
  static_assert(max_bit < Params::clock_half_range,
                "Bit intervals must be less than half a clock cycle");
  static_assert(stop_period >= max_bit && stop_period < Params::clock_half_range,
                "The stop interval must delimit bits and fit half a clock cycle");
  static_assert(settle_period < Params::clock_half_range - stop_period,
                "Stop plus settle must be less than half a clock cycle");
  return Params{
    P(settle_period), P(stop_period),
    P(static_cast<w_type>(bit_period / 4)),
    P(static_cast<w_type>((bit_period / 4) * 3 + ((bit_period % 4) * 3) / 4)),
    P(static_cast<w_type>(bit_period / 2)),
    P(static_cast<w_type>((bit_period / 5) * 4 + ((bit_period % 5) * 4) / 5)),
    P(max_bit)};
}();

} // namespace setl
