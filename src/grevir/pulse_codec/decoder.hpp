// Extracted from Ardoinus pwe_serial.h; MIT license, see LICENSE.txt.
#pragma once

#include <grevir/pulse_codec/bits.hpp>
#include <grevir/pulse_codec/waveform.hpp>

namespace setl {

/**
 * Decodes a sequence of timed on/off signals and provides a decoded value.
 */
template <
  typename CollectorType,     /// The assembler of bit into data.
  bool inverted = false,      /// The sense of the signal.
  bool long_on_is_1 = false,  /// Legacy polarity: false means long=1; true means short=1.
  typename w_type = xpwe_type,/// The time value type.
  TimeUnit units = PWE_UNIT>  /// Units used.
class PweDecoder {
private:
  enum class State : char {
    NO_SAMPLES,
    SETTLE,
    ON,
    OFF,
    STOP
  };

public:

  using PeriodType = Period<w_type, units>;
  using TimeType = Time<w_type, units>;
  static const std::uint16_t size_bits = CollectorType::size_bits;
  using value_type = typename CollectorType::value_type;

  PweDecoder(const PweWaveformParams<w_type, units>& params)
    : params(params)
  {
    assert(params.valid());
  }

  // Signal has been deemed changed at a specific time.
  void signalChanged(const TimeType& time, bool in_level) {
    bool level = in_level != inverted;
    const bool previous_level = last_level;
    last_level = level;

    switch (state) {
      case State::NO_SAMPLES: {
        transition(State::STOP, time);
        break;
      }
      case State::STOP: {
        // A falling edge starts the idle interval; high time cannot satisfy it.
        if (previous_level) {
          time_state_entered = time;
        }
        // We need the level to stay off for a minimum time.
        if ((time - time_state_entered) < params.stop_period) {
          if (level) {
            // Ignore data being sent until after a full stop period has passed.
            time_state_entered = time;  // transition to self.
          }
          break;
        }
        transition(State::SETTLE, time);

      }
      [[fallthrough]];
      case State::SETTLE: {
        if (level) {
          // start of first bit.
          transition(State::ON, time);
          if (bit_number == size_bits) {
            overrun = true;
          }
          bit_number = 0;
          collector.writeValue(value_type{});
        }
        break;
      }

      case State::ON: {
        auto period = time - time_state_entered;
        if (period > params.bit_max_period || (!level && period.get() == 0)) {
          rejectSignal(time);
        } else if (!level) {
          auto is_long = period >= params.bit_half_period;
          collector.saveBit(long_on_is_1 != is_long, bit_number);
          bit_number++;
          if (bit_number < size_bits) {
            state = State::OFF;
          } else {
            transition(State::STOP, time);
          }
        }
        break;
      }

      case State::OFF: {
        auto period = time - time_state_entered;

        // Period for the bit has to be within margin.
        if (period > params.bit_max_period) {
          rejectSignal(time);
        } else if (level) {
          if (period < params.bit_min_period) {
            rejectSignal(time);
          } else if (bit_number >= size_bits) {
            transition(State::STOP, time);
          } else {
            transition(State::ON, time);
          }
        }
        break;
      }
    }
  }

  bool pollDataReady() const {
    return bit_number == size_bits && (state == State::STOP || state == State::SETTLE);
  }

  /// Advisory wait estimate, not an edge deadline; zero when ready, saturated on overflow.
  PeriodType nextPollWaitTime(const TimeType& now) const {
    using pulse_codec_detail::add;
    using pulse_codec_detail::multiply;
    if (pollDataReady()) {
      return PeriodType(0);
    }
    switch (state) {
      case State::SETTLE: {
        return bitsDecodePeriod();
      }
      case State::STOP: {
        const auto elapsed = (now - time_state_entered).get();
        const auto remaining = elapsed < params.stop_period.get()
          ? static_cast<w_type>(params.stop_period.get() - elapsed) : w_type(0);
        return PeriodType(add(bitsDecodePeriod().get(), remaining));
      }
      case State::ON:
      case State::OFF: {
        const auto remaining_bits = static_cast<std::uint16_t>(size_bits - bit_number);
        return PeriodType(multiply(add(params.bit_half_period.get(), params.bit_on_period.get()),
                                   remaining_bits));
      }
      case State::NO_SAMPLES: {
        return PeriodType(add(bitsDecodePeriod().get(), params.stop_period.get()));
      }
    }
    return PeriodType(0);
  }

  /** Nominal full-frame duration, saturated at the period representation's maximum. */
  PeriodType bitsDecodePeriod() const {
    return PeriodType(pulse_codec_detail::multiply(
      pulse_codec_detail::add(params.bit_off_period.get(), params.bit_on_period.get()), size_bits));
  }

  /** Returns true if a value is available and places the result in the given parameter. */
  bool readValue(value_type& result) {
    if (!pollDataReady()) {
      return false;
    }
    collector.readValue(result);
    bit_number = 0;
    return true;
  }

  bool hasSignalError() {
    return signal_error;
  }

  void clearSignalError() {
    signal_error = false;
  }

  bool isOverrun() {
    return overrun;
  }

  void clearOverrun() {
    overrun = false;
  }

  /** Resets all state. */
  void reset() {
    state = State::NO_SAMPLES;
    time_state_entered = TimeType();
    bit_number = 0;
    collector.writeValue(value_type{});
    last_level = false;
    clearOverrun();
    clearSignalError();
  }

  std::uint16_t get_bit_number() {
    return bit_number;
  }

private:

  void rejectSignal(const TimeType& time) {
    transition(State::STOP, time);
    signal_error = true;
    bit_number = 0;
    collector.writeValue(value_type{});
  }

  void transition(State nextState, const TimeType& time) {
    state = nextState;
    time_state_entered = time;
  }

  // The encoder parameters used for this.
  const PweWaveformParams<w_type, units>& params;

  State state = State::NO_SAMPLES;
  std::uint16_t bit_number = 0;
  bool overrun = false;
  bool signal_error = false;
  bool last_level = false;
  CollectorType collector;
  TimeType time_state_entered;
};

} // namespace setl
