// Adapted from Ardoinus tests/pwe_serial_test.h; MIT license, see LICENSE.txt.
#include <GrevirPulseCodec.h>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace {
using namespace setl;
const auto& params = PweWaveformParams1to3<std::uint32_t, 1000, 1000, 100>;
using Bits = PweBitCollector<15>;
using Encoder = PweEncoder<Bits>;
using Decoder = PweDecoder<Bits>;
using Tick = Encoder::TimeType;
using Delay = Encoder::PeriodType;

// Repeatable 0..12 tick edge jitter, without global rand() state.
struct Jitter {
  std::uint32_t state = 1;
  std::uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state % 13;
  }
};

template <typename E, typename D>
void transfer(E& encoder, D& decoder, typename E::TimeType& now,
              const typename E::value_type& value, Jitter* jitter = nullptr) {
  using Period = typename E::PeriodType;
  decoder.signalChanged(now, E::off_level);
  now += Period(1020);
  REQUIRE(encoder.send(value));
  auto output = encoder.poll(now);
  unsigned transitions = 0;
  while (!output.no_data) {
    REQUIRE(++transitions <= 2u * E::size_bits + 2u);
    decoder.signalChanged(now + Period(jitter ? jitter->next() : 0), output.current_output);
    now = output.time_next_poll;
    output = encoder.poll(now);
  }
  REQUIRE(decoder.pollDataReady());
  typename E::value_type result{};
  REQUIRE(decoder.readValue(result));
  REQUIRE(result == value);
  REQUIRE_FALSE(decoder.pollDataReady());
  REQUIRE_FALSE(decoder.hasSignalError());
  decoder.signalChanged(now, E::off_level);
}

template <bool inverted, bool polarity>
void check_polarity() {
  PweEncoder<PweBitCollector<2>, inverted, polarity> encoder(params);
  PweDecoder<PweBitCollector<2>, inverted, polarity> decoder(params);
  Tick now{};
  for (std::uint8_t value = 0; value < 4; ++value) {
    transfer(encoder, decoder, now, value);
  }
  encoder.reset();
  REQUIRE(encoder.send(1));
  now = Tick(100);
  auto first = encoder.poll(now);
  REQUIRE(first.current_output == !inverted);
  REQUIRE((first.time_next_poll - now).get() == (polarity ? 25 : 75));
  auto second = encoder.poll(first.time_next_poll);
  REQUIRE(second.current_output == inverted);
  REQUIRE((second.time_next_poll - now).get() == 100);
}
} // namespace

TEST_CASE("legacy encoder deadlines and stop interval") {
  Encoder encoder(params);
  Tick now(std::numeric_limits<std::uint32_t>::max());
  REQUIRE(encoder.poll(now).no_data);
  REQUIRE(encoder.send(0x5a));
  REQUIRE_FALSE(encoder.send(0));
  auto first = encoder.poll(now);
  for (unsigned bit = 0; bit < 15; ++bit) {
    CAPTURE(bit);
    REQUIRE(first.current_output);
    REQUIRE((first.time_next_poll - now).get() == ((0x5au >> bit) & 1u ? 75 : 25));
    REQUIRE(encoder.poll(first.time_next_poll - Delay(1)).current_output);
    auto second = encoder.poll(first.time_next_poll);
    REQUIRE_FALSE(second.current_output);
    REQUIRE((second.time_next_poll - now).get() == 100);
    REQUIRE_FALSE(encoder.poll(second.time_next_poll - Delay(1)).current_output);
    now = second.time_next_poll;
    first = encoder.poll(now);
  }
  REQUIRE_FALSE(first.current_output);
  REQUIRE_FALSE(first.no_data);
  REQUIRE((first.time_next_poll - now).get() == 2000);
  REQUIRE_FALSE(encoder.poll(first.time_next_poll - Delay(1)).no_data);
  REQUIRE(encoder.poll(first.time_next_poll).no_data);
}

TEST_CASE("all legacy 15-bit values round trip with deterministic edge jitter") {
  Encoder encoder(params);
  Decoder decoder(params);
  Tick now{};
  Jitter jitter;
  for (const auto value : {0x15fu, 0x35au, 0x300u, 0x299u}) {
    transfer(encoder, decoder, now, static_cast<Bits::value_type>(value), &jitter);
  }
  for (std::uint32_t value = 0; value < 32768; ++value) {
    CAPTURE(value);
    transfer(encoder, decoder, now, static_cast<Bits::value_type>(value), &jitter);
  }
}

TEST_CASE("both signal senses and both bit polarities preserve bit duration") {
  check_polarity<false, false>();
  check_polarity<false, true>();
  check_polarity<true, false>();
  check_polarity<true, true>();
}

TEST_CASE("scalar bits support replacement and the highest 32-bit position") {
  PweBitCollector<32> bits;
  bits.saveBit(true, 31);
  REQUIRE(bits.getBit(31));
  bits.saveBit(false, 31);
  REQUIRE_FALSE(bits.getBit(31));
  bits.writeValue(0x80000001u);
  std::uint32_t value = 0;
  bits.readValue(value);
  REQUIRE(value == 0x80000001u);
  bits.readValue(value);
  REQUIRE(value == 0);
}

TEST_CASE("array bits cross byte and word boundaries and reset after reading") {
  PweBitCollectorArray<char, 15> bytes;
  STATIC_REQUIRE(sizeof(decltype(bytes)::value_type) == 2);
  for (unsigned bit = 0; bit < 15; ++bit) {
    bytes.saveBit(true, bit);
    REQUIRE(bytes.getBit(bit));
    bytes.saveBit(false, bit);
    REQUIRE_FALSE(bytes.getBit(bit));
  }
  bytes.writeValue({static_cast<char>(0x81), 0x40});
  decltype(bytes)::value_type value{};
  bytes.readValue(value);
  REQUIRE(static_cast<unsigned char>(value[0]) == 0x81);
  REQUIRE(value[1] == 0x40);
  bytes.readValue(value);
  REQUIRE(value == decltype(value){});
  PweBitCollectorArray<std::uint16_t, 33> words;
  STATIC_REQUIRE(sizeof(decltype(words)::value_type) == 6);
  words.saveBit(true, 15);
  words.saveBit(true, 16);
  words.saveBit(true, 32);
  decltype(words)::value_type word_value{};
  words.readValue(word_value);
  REQUIRE(word_value == decltype(word_value){0x8000, 1, 1});
  PweEncoder<decltype(words)> encoder(params);
  PweDecoder<decltype(words)> decoder(params);
  Tick now{};
  transfer(encoder, decoder, now, word_value);
}

TEST_CASE("decoder reset and unread frame replacement discard stale bits") {
  Decoder decoder(params);
  const auto edge = [&](std::uint32_t time, bool level) {
    decoder.signalChanged(Tick(time), level);
  };
  edge(0, false);
  edge(1000, true);
  edge(1075, false); // A one in an incomplete frame.
  decoder.reset();
  Encoder encoder(params);
  Tick now(2000);
  transfer(encoder, decoder, now, Bits::value_type(0));
  REQUIRE_FALSE(decoder.isOverrun());

  decoder.reset();
  edge(0, false);
  for (unsigned bit = 0; bit < 15; ++bit) {
    edge(1000 + 100 * bit, true);
    edge(1075 + 100 * bit, false);
  }
  REQUIRE(decoder.pollDataReady());
  now = Tick(5000);
  transfer(encoder, decoder, now, Bits::value_type(0));
  REQUIRE(decoder.isOverrun());
  decoder.clearOverrun();
  REQUIRE_FALSE(decoder.isOverrun());
}

TEST_CASE("decoder recovers after an invalid inter-bit interval") {
  Decoder decoder(params);
  decoder.signalChanged(Tick(0), false);
  decoder.signalChanged(Tick(1000), true);
  decoder.signalChanged(Tick(1075), false);
  decoder.signalChanged(Tick(1400), true);
  REQUIRE(decoder.hasSignalError());
  REQUIRE_FALSE(decoder.pollDataReady());
  decoder.clearSignalError();
  Encoder encoder(params);
  Tick now(2000);
  transfer(encoder, decoder, now, Bits::value_type(0));
  REQUIRE_FALSE(decoder.isOverrun());
}

TEST_CASE("queued frames respect the stop interval across an 8-bit clock wrap") {
  const auto& narrow = PweWaveformParams1to3<std::uint8_t, 4, 20, 8>;
  using E = PweEncoder<PweBitCollector<2>, false, false, std::uint8_t>;
  using D = PweDecoder<PweBitCollector<2>, false, false, std::uint8_t>;
  E encoder(narrow);
  D decoder(narrow);
  E::TimeType now(250);
  decoder.signalChanged(E::TimeType(230), false);
  REQUIRE(encoder.send(1));
  for (unsigned frame = 0; frame < 2; ++frame) {
    auto output = encoder.poll(now);
    for (unsigned bit = 0; bit < 2; ++bit) {
      REQUIRE(output.current_output);
      decoder.signalChanged(now, output.current_output);
      REQUIRE(encoder.poll(output.time_next_poll - E::PeriodType(1)).current_output);
      now = output.time_next_poll;
      output = encoder.poll(now);
      REQUIRE_FALSE(output.current_output);
      decoder.signalChanged(now, output.current_output);
      now = output.time_next_poll;
      output = encoder.poll(now);
    }
    REQUIRE(decoder.pollDataReady());
    REQUIRE(decoder.nextPollWaitTime(now).get() == 0);
    E::value_type value{};
    REQUIRE(decoder.readValue(value));
    REQUIRE(value == (frame == 0 ? 1 : 2));
    REQUIRE_FALSE(output.no_data);
    REQUIRE_FALSE(output.current_output);
    REQUIRE((output.time_next_poll - now).get() == 24);
    if (frame == 0) {
      REQUIRE(encoder.send(2));
      REQUIRE_FALSE(encoder.send(0));
      REQUIRE_FALSE(encoder.poll(output.time_next_poll - E::PeriodType(1)).current_output);
    }
    now = output.time_next_poll;
  }
  REQUIRE(encoder.poll(now).no_data);
  REQUIRE_FALSE(decoder.hasSignalError());
}

TEST_CASE("decoder requires a complete idle interval and rejects malformed final pulses") {
  using D = PweDecoder<PweBitCollector<1>>;
  D decoder(params);
  decoder.signalChanged(Tick(0), true);
  decoder.signalChanged(Tick(2000), false);
  decoder.signalChanged(Tick(2500), true); // Only 500 ticks of idle: not a bit.
  decoder.signalChanged(Tick(2575), false);
  REQUIRE_FALSE(decoder.pollDataReady());
  decoder.signalChanged(Tick(4000), true);
  decoder.signalChanged(Tick(4200), false); // Final pulse is too long.
  REQUIRE(decoder.hasSignalError());
  REQUIRE_FALSE(decoder.pollDataReady());
  decoder.clearSignalError();
  decoder.signalChanged(Tick(6000), true);
  decoder.signalChanged(Tick(6000), false); // Zero width is also invalid.
  REQUIRE(decoder.hasSignalError());
  REQUIRE_FALSE(decoder.pollDataReady());
}

TEST_CASE("integer waveform ratios preserve rounding without overflowing intermediates") {
  const auto& odd = PweWaveformParams1to3<std::uint16_t, 1, 100, 7>;
  REQUIRE(odd.valid());
  REQUIRE(odd.bit_on_period.get() == 1);
  REQUIRE(odd.bit_off_period.get() == 5);
  REQUIRE(odd.bit_half_period.get() == 3);
  REQUIRE(odd.bit_min_period.get() == 5);
  REQUIRE(odd.bit_max_period.get() == 8);
  const auto& wide = PweWaveformParams1to3<std::uint32_t, 0, 2147483647u, 1789569706u>;
  REQUIRE(wide.valid());
  REQUIRE(wide.bit_on_period.get() == 447392426u);
  REQUIRE(wide.bit_off_period.get() == 1342177279u);
  REQUIRE(wide.bit_min_period.get() == 1431655764u);
  REQUIRE(wide.bit_max_period.get() == 2147483647u);
  auto invalid = odd;
  invalid.bit_half_period = decltype(odd.bit_half_period)(0);
  REQUIRE_FALSE(invalid.valid());
}

TEST_CASE("long frame wait estimates saturate in the selected representation") {
  const auto& narrow = PweWaveformParams1to3<std::uint16_t, 1000, 24000, 20000>;
  PweDecoder<PweBitCollector<4>, false, false, std::uint16_t> decoder(narrow);
  using T = decltype(decoder)::TimeType;
  REQUIRE(decoder.bitsDecodePeriod().get() == 65535);
  REQUIRE(decoder.nextPollWaitTime(T(0)).get() == 65535);
  decoder.signalChanged(T(0), false);
  REQUIRE(decoder.nextPollWaitTime(T(0)).get() == 65535);
  decoder.signalChanged(T(24000), true);
  REQUIRE(decoder.nextPollWaitTime(T(24000)).get() == 60000);
}
