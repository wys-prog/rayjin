#pragma once

#include <cstdint>
#include <limits>
#include <climits>
#include <type_traits>
#include <algorithm>

#include "jincore.hpp"

namespace jin::sized {
  template <std::size_t N>
  struct integral_storage;

  template <>
  struct integral_storage<1> { using type = std::int8_t; };

  template <>
  struct integral_storage<2> { using type = std::int16_t; };

  template <>
  struct integral_storage<4> { using type = std::int32_t; };

  template <>
  struct integral_storage<8> { using type = std::int64_t; };

  #ifdef __SIZEOF_INT128__
  template <>
  struct integral_storage<16> { using type = __int128_t; };
  #endif

  template <typename T>
  using integral_storage_t = typename std::make_unsigned_t<typename integral_storage<sizeof(T)>::type>;

  using float64 = std::conditional_t<sizeof(float) == sizeof(int64_t), float, std::conditional_t<
    sizeof(double) == sizeof(int64_t), double, 
    std::conditional_t<sizeof(long double) == sizeof(int64_t), long double, long double>
  >>;

  using float32 = std::conditional_t<sizeof(float) == sizeof(int32_t), float, std::conditional_t<
    sizeof(double) == sizeof(int32_t), double, 
    std::conditional_t<sizeof(long double) == sizeof(int32_t), long double, long double>
  >>;

  #if __STDCPP_FLOAT128_T__ == 1
  inline constexpr static auto has_float128 = true;
  using float128 = _Float128;
  #else
  inline constexpr static auto has_float128 = false;
  using float128 = void;
  #endif

  using largefloat_default_fallback = std::conditional_t<(sizeof(float64) > sizeof(long double)), long double, float64>;
  using largefloat = std::conditional_t<has_float128, float128, largefloat_default_fallback>;
  
  #ifdef __SIZEOF_INT128__
  using i128 = __int128_t;
  using ui128 = std::make_unsigned_t<__int128_t>;
  inline constexpr static auto has_i128 = true;
  #else
  using i128 = void;
  using ui128 = void;
  inline constexpr static auto has_i128 = false;
  #endif
  
  using largeint = std::conditional_t<has_i128, i128, long long int>;

  namespace arches {
    using vclassic = rt::v_varch<16, uint16_t, float32, integral_storage_t<float32>>;
    using vnormal  = rt::v_varch<32, uint64_t, float64, integral_storage_t<float64>>;

    #ifndef RAYJIN_DISABLE_VLONG_ARCH
    using vlong    = rt::v_varch<32, largeint, largefloat, integral_storage_t<largefloat>>;
    #endif
  }
}