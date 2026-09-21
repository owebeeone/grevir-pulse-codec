# Grevir Pulse Codec

Fixed-storage pulse-width serial encoding and decoding, extracted from Ardoinus
`pwe_serial.h`. The public types retain their `setl` names. Bit storage, waveform
parameters, encoding and decoding have separate headers under
`grevir/pulse_codec/`; `GrevirPulseCodec.h` includes all four.

The package depends only on Grevir Base and Time. It has no GPIO, Arduino, timer
allocation, interrupt or packet-framing dependency. It allocates no dynamic storage:
scalar collectors hold 1–32 bits; array collectors hold 1–65,535 bits in a fixed
`std::array`. Array positions count bits within each element, least significant bit
first, then advance to the next element. Non-bool integral elements, including
legacy `char`, are supported. Bits outside the declared payload are ignored when
encoding and zero after decoding.

## Installed use

```cmake
find_package(grevir-pulse-codec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE grevir::pulse_codec)
```

The target supplies C++23, the standard-library mode and its dependencies. Normal
production builds and installed consumers do not require Catch2 or Test Support.
Arduino library layout is supplied, but Arduino/target builds are not validated.

```cpp
#include <GrevirPulseCodec.h>

using Bits = setl::PweBitCollector<15>;
const auto& waveform =
  setl::PweWaveformParams1to3<std::uint32_t, 1000, 1000, 100>;
setl::PweEncoder<Bits> encoder(waveform);
setl::PweDecoder<Bits> decoder(waveform);
```

See [the installed consumer](tests/installed-consumer/main.cpp) for a complete
host loopback using a two-byte payload. A physical integration must provide edge
timestamps and apply encoder output levels; that belongs to the later Pulse IO
extraction.

## Timing and ownership contract

- The waveform object must outlive the encoder/decoder and remain unchanged while
  they use it; they retain a reference. Manually assembled parameters must satisfy
  `valid()`, asserted at construction. Collector bit addresses must be in range.
- Clocks use unsigned integral tick values, defaulting to uint32 microseconds.
  Each bit's maximum interval and the combined stop/settle interval must be less
  than half the clock range. Service deadlines/observations within that range so
  modular wraparound is unambiguous. Floating-point clocks are not supported.
- The 1:3 factory uses integer arithmetic. Short, long and tolerance intervals
  round down independently: bit/4, 3*bit/4, 4*bit/5 and 6*bit/5. No overflowing
  multiply or wider intermediate is needed. Choose a bit period divisible by four
  for exact nominal bit duration. Invalid factory intervals fail compilation.
- `send()` copies a payload if idle, or queues one payload during the stop period;
  it returns false when that slot is occupied. `poll()` reports the required level
  and next deadline. Apply changes on time; the encoder advances one transition
  per call and cannot repair a physical waveform after missed deadlines.
- Seed the decoder with the idle level, then allow at least a stop interval before
  the first active edge. Feed subsequent edges in timestamp order. Repeated level
  samples are permitted. `pollDataReady()`/`readValue()` expose one complete value;
  a new frame replaces an unread value and latches `isOverrun()`.
- Default wire polarity is unchanged: a long active pulse means one. Despite its
  inherited name, the template flag `long_on_is_1=true` selects short active pulses
  for one. `inverted` changes the electrical sense independently. Both choices now
  retain the same total bit duration.
- Decoding classifies active widths at `bit_half_period` and checks inter-bit
  periods against min/max. Zero-width and overlong active pulses are rejected,
  including the last bit. As in Ardoinus, a value is available at the last active
  pulse's falling edge; its trailing idle duration is not part of frame acceptance.
  This is not a checksum or comprehensive noise filter.
- Errors discard partial bits and wait for idle before resuming. Reset clears all
  buffered bits and flags. `nextPollWaitTime()` is an advisory estimate, not a
  guaranteed earliest-arrival bound or an edge-service deadline. It returns zero
  for ready data; unrepresentable estimates saturate instead of wrapping.

## Extraction corrections and evidence

Inherited defects corrected here: array element count/bit indexing and invalid
array reset assignment; false-bit replacement; stale partial/unread frame bits;
alternate-polarity second-half timing; counting active input toward startup idle;
accepting zero/overlong final pulses; the modular deadline boundary; overflowing
wait estimates; and incidental floating-point arithmetic in integer waveform
construction. The old array read failed compilation. After only collector fixes,
three regression cases failed in the retained state machines, then passed with
the timing/recovery changes.

Eleven native Catch2 cases pass, including the original exhaustive 32,768-value
15-bit sweep with repeatable 0–12 tick jitter, independent deadline expectations,
all four sense/polarity combinations, array boundaries, recovery, unread data,
queued sends, 8-/32-bit clock wraparound and 16-/32-bit arithmetic boundaries.
All five public headers compile independently; one valid and seven expected-rejection
compiler probes pass. Raw-token checking confirms braced control bodies in all eight
production/test C++ files. Isolated production/install/consumer
checks and the same eleven cases under address/undefined sanitizers pass.
Source review covers the shared policy and conventional AVR 16-bit integer
promotions; this is not target code-size or cycle evidence. AVR compiler and
hardware validation remain on hold.

Standalone host validation can use installed dependencies:

```sh
cmake -S . -B build/host -DCMAKE_PREFIX_PATH=/path/to/grevir/install \
  -DGREVIR_BUILD_HOST_TESTS=ON -DGREVIR_BUILD_COMPILE_CHECKS=ON \
  -DGREVIR_CATCH2_SOURCE_DIR=/path/to/Catch2-3.8.1
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Packet Codec and Pulse IO remain separate planned packages.
