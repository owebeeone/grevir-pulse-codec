// Extracted from Ardoinus pwe_serial.h; MIT license, see LICENSE.txt.
#pragma once

#include <grevir/pulse_codec/bits.hpp>
#include <grevir/pulse_codec/waveform.hpp>

namespace setl {

template <
  typename CollectorType,     /// The assembler of bit into data.
  bool inverted = false,      /// The sense of the signal.
  bool long_on_is_1 = false,  /// Legacy polarity: false means long=1; true means short=1.
  typename w_type = xpwe_type,/// The time value type.
  TimeUnit units = PWE_UNIT>  /// Units used.
class PweEncoder {
private:
  enum class State : char {
    NO_DATA,
    HAVE_DATA,
    BIT_ON_FIRST,
    BIT_ON_SECOND,
    BIT_OFF_FIRST,
    BIT_OFF_SECOND,
    STOP
  };
public:

  using PeriodType = Period<w_type, units>;
  using TimeType = Time<w_type, units>;
  static const std::uint16_t size_bits = CollectorType::size_bits;
  using value_type = typename CollectorType::value_type;
  enum {
    on_level = true != inverted,
    off_level = true == inverted,
  };

  // The result of a poll.
  struct PollResult {
    std::uint8_t current_output;  // The output needed.
    std::uint8_t no_data;  // indicates that no future poll needed until next send.
    TimeType time_next_poll;
  };

  PweEncoder(const PweWaveformParams<w_type, units>& params)
    : params(params)
  {
    assert(params.valid());
  }

  /** Sets the data to send */
  bool send(const value_type& value) {

    // If we're using the last data then no, we can't send this new data now.
    if ((state != State::NO_DATA)
      && ((state != State::STOP) || (bit_number == 0))) {
      return false;
    }

    collector.writeValue(value);
    bit_number = 0;
    if (state == State::NO_DATA) {
      state = State::HAVE_DATA;
    }
    return true;
  }


  /** The time on the next poll event. */
  PollResult poll(const TimeType& time) {
    switch (state) {
      default:
      case State::NO_DATA: {
        return PollResult{ off_level, true, TimeType{ 0 } };
      }
      case State::HAVE_DATA: {
        transitionBit(time);
        return PollResult{ on_level, false, next_transition_time };
      }
      case State::BIT_ON_FIRST: {
        if (hasPassedMidBitPoint(time)) {
          state = State::BIT_ON_SECOND;
          next_transition_time += statePeriod(State::BIT_ON_SECOND);
          return PollResult{ off_level, false, next_transition_time };
        }
        return PollResult{ on_level, false, next_transition_time };
      }
      case State::BIT_OFF_FIRST: {
        if (hasPassedMidBitPoint(time)) {
          state = State::BIT_OFF_SECOND;
          next_transition_time += statePeriod(State::BIT_OFF_SECOND);
          return PollResult{ off_level, false, next_transition_time };
        }
        return PollResult{ on_level, false, next_transition_time };
      }
      case State::BIT_OFF_SECOND:
      case State::BIT_ON_SECOND: {
        if (hasPassedMidBitPoint(time)) {
          if (bit_number < size_bits) {
            transitionBit(next_transition_time);
            return PollResult{ on_level, false, next_transition_time };
          } else {
            state = State::STOP;
            next_transition_time += statePeriod(State::STOP);
            return PollResult{ off_level, false, next_transition_time };
          }
        }
        return PollResult{ off_level, false, next_transition_time };
      }
      case State::STOP: {
        if (hasPassedMidBitPoint(time)) {
          if (bit_number > 0) {
            state = State::NO_DATA;
            return PollResult{ off_level, true, TimeType(0) };
          } else {
            transitionBit(time);
            return PollResult{ on_level, false, next_transition_time };
          }
        }
        return PollResult{ off_level, false, next_transition_time };
      }
    }
  }

  /** Resets all state. */
  void reset() {
    state = State::NO_DATA;
    next_transition_time = TimeType();
    bit_number = 0;
  }

private:
  PeriodType statePeriod(State for_state) {
    switch (for_state) {
      default:
      case State::NO_DATA: {
        return PeriodType{ 0 };
      }
      case State::HAVE_DATA: {
        return PeriodType{ 0 };
      }
      case State::BIT_ON_FIRST: {
        return params.bit_on_period;
      }
      case State::BIT_ON_SECOND: {
        return params.bit_off_period;
      }
      case State::BIT_OFF_FIRST: {
        return params.bit_off_period;
      }
      case State::BIT_OFF_SECOND: {
        return params.bit_on_period;
      }
      case State::STOP: {
        return params.stop_period + params.settle_period;
      }
    }
  }

  void transitionBit(const TimeType& time) {
    auto bit_is_short = collector.getBit(bit_number) == long_on_is_1;
    if (bit_is_short) {
      state = State::BIT_ON_FIRST;
      next_transition_time = time + params.bit_on_period;
    } else {
      next_transition_time = time + params.bit_off_period;
      state = State::BIT_OFF_FIRST;
    }
    ++bit_number;
  }

  bool hasPassedMidBitPoint(const TimeType& time) {
    const auto mid_value = PeriodType(PweWaveformParams<w_type, units>::clock_half_range);
    return (time - next_transition_time) < mid_value;
  }

  State state = State::NO_DATA;
  TimeType next_transition_time;
  std::uint16_t bit_number = 0;

  CollectorType collector;

  // The encoder parameters used for this.
  const PweWaveformParams<w_type, units>& params;
};

} // namespace setl
