#include <GrevirPulseCodec.h>

void compile_collectors() {
  setl::PweBitCollectorArray<char, 15> collector;
  decltype(collector)::value_type result{};
  collector.saveBit(true, 14);
  collector.readValue(result);
}

static_assert(sizeof(setl::PweBitCollectorArray<std::uint8_t, 65535>::value_type) == 8192);
static_assert(sizeof(setl::PweBitCollectorArray<std::uint16_t, 65535>::value_type) == 8192);
static_assert(sizeof(setl::PweBitCollector<1>::value_type) == 1);
static_assert(sizeof(setl::PweBitCollector<16>::value_type) == 2);
static_assert(sizeof(setl::PweBitCollector<32>::value_type) == 4);

template class setl::PweEncoder<setl::PweBitCollector<1>, false, false, std::uint8_t>;
template class setl::PweDecoder<setl::PweBitCollector<1>, false, false, std::uint8_t>;
template class setl::PweEncoder<setl::PweBitCollector<32>, true, true, std::uint16_t>;
template class setl::PweDecoder<setl::PweBitCollector<32>, true, true, std::uint16_t>;
template class setl::PweBitCollectorArray<std::uint32_t, 64>;
