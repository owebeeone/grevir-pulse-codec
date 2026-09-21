// Extracted from Ardoinus pwe_serial.h; MIT license, see LICENSE.txt.
#pragma once

#include <grevir/base/type_for_size.hpp>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace setl {

/**
 * Holder for bit data.
 */
template <std::uint16_t w_size_bits>
class PweBitCollector {
  static_assert(w_size_bits > 0, "Cannot have 0 bits");
  static_assert(w_size_bits <= 32, "Use PweBitCollectorArray for more than 32 bits");

public:
  static const std::uint16_t size_bits = w_size_bits;
  using value_type = typename setl::TypeForMaxBits<size_bits>::selected::type_unsigned;

  /**
   * Returns the current value and resets the value.
   */
  void readValue(value_type& result) {
    result = working_value;
    working_value = value_type{};
  }

  void saveBit(bool bit_value, std::uint32_t bit_address) {
    assert(bit_address < size_bits);
    const auto bit = static_cast<value_type>(value_type(1u) << bit_address);
    working_value = static_cast<value_type>((working_value & ~bit) | (bit_value ? bit : 0));
  }

  void writeValue(const value_type& new_value) {
    working_value = new_value;
  }

  bool getBit(std::uint32_t bit_address) const {
    assert(bit_address < size_bits);
    auto bit = value_type(1u) << bit_address;
    return working_value & bit;
  }

private:
  value_type working_value = 0;
};

/**
 * Array version.
 */
template <typename T, std::uint16_t w_size_bits>
class PweBitCollectorArray {
  static_assert(w_size_bits > 0, "Cannot have 0 bits");
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                "Array elements must be non-bool integral types");
  using unsigned_type = std::make_unsigned_t<T>;
  static constexpr unsigned bits_per_element = std::numeric_limits<unsigned_type>::digits;

public:
  using value_type = std::array<T, (w_size_bits - 1) / bits_per_element + 1>;
  static const std::uint16_t size_bits = w_size_bits;

  void readValue(value_type& result) {
    result = working_value;
    working_value = value_type{};
  }

  void saveBit(bool bit_value, std::uint32_t bit_address) {
    assert(bit_address < size_bits);
    const auto index = bit_address / bits_per_element;
    const auto bit = static_cast<unsigned_type>(unsigned_type(1u) << (bit_address % bits_per_element));
    const auto value = static_cast<unsigned_type>(working_value[index]);
    working_value[index] = static_cast<T>((value & ~bit) | (bit_value ? bit : 0));
  }

  void writeValue(const value_type& new_value) {
    working_value = new_value;
  }

  bool getBit(std::uint32_t bit_address) const {
    assert(bit_address < size_bits);
    const auto index = bit_address / bits_per_element;
    const auto bit = static_cast<unsigned_type>(unsigned_type(1u) << (bit_address % bits_per_element));
    return static_cast<unsigned_type>(working_value[index]) & bit;
  }

private:
  value_type working_value{};
};

} // namespace setl
