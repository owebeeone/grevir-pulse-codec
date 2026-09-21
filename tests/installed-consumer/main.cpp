#include <GrevirPulseCodec.h>

int main() {
  using Bits = setl::PweBitCollectorArray<std::uint8_t, 16>;
  const auto& waveform = setl::PweWaveformParams1to3<std::uint32_t, 1000, 1000, 100>;
  setl::PweEncoder<Bits> encoder(waveform);
  setl::PweDecoder<Bits> decoder(waveform);
  using Tick = decltype(encoder)::TimeType;
  const Bits::value_type input{0xa5, 0x81};
  decoder.signalChanged(Tick(0), false);
  if (!encoder.send(input)) {
    return 1;
  }
  Tick now(1000);
  auto output = encoder.poll(now);
  unsigned steps = 0;
  while (!output.no_data) {
    if (++steps > 34) {
      return 2;
    }
    decoder.signalChanged(now, output.current_output);
    now = output.time_next_poll;
    output = encoder.poll(now);
  }
  Bits::value_type result{};
  if (!decoder.readValue(result) || result != input || decoder.hasSignalError()) {
    return 3;
  }
  return 0;
}
