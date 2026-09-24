#pragma once

#include <tuple>
#include "stack.hpp"

namespace rayjin::dev::lua {
  template <typename>
  struct function_signature;

  template <typename R, typename... Args>
  struct function_signature<R(Args...)> {
    using aruments = std::tuple<Args...>;
    constexpr size_t argc = sizeof...(Args);
    using retrn = R;
  };

  template<typename Callable, typename R, typename Tuple, size_t... I>
  int invoke(Callable &&f, lua_State *L, std::index_sequence<I...>) {
    if constexpr (std::is_void_v<R>) {
      std::invoke(f, lua_stack<std::tuple_element_t<I, Tuple>>::get(L, I + 1)...);
      return 0;
    } else {
      auto r = std::invoke(f,lua_stack<std::tuple_element_t<I, Tuple>>::get(L, I + 1)...);

      lua_stack<R>::push(L, r);
      return 1;
    }
  }

  template <auto F>
  lua_CFunction dispatch(lua_State* L) {
    using sign = function_signature<F>;

    return invoke<decltype(F), sign::retrn, signl::arguments>(
      F, L, std::make_index_sequence<sign::size>{}
    );
  }
}