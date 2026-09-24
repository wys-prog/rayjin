#pragma once

#include <string>
#include <exception>
#include <type_traits>
#include <lua/lua.hpp>

#include "../except.hpp"

namespace rayjin::dev::lua {
  template <typename T, typename=void>
  struct lua_stack;

  template <typename T>
  struct lua_stack<T, std::enable_if<
    std::is_arithmetic_v<T> && !std::is_same_v<T, bool>
  >> {
    static void push(lua_State* L, const T& v) {
      lua_pushinteger(L, (lua_Integer)v);
    }

    static T get(lua_State* L) {
      return luaL_checkinteger(L, 1);
    }
  };

  template <typename T>
  struct lua_stack<T, std::enable_if<std::is_floating_point_v<T>>> {
    static void push(lua_State* L, const T& v) {
      lua_pushinteger(L, (lua_Number)v);
    }

    static T get(lua_State* L) {
      return luaL_checknumber(L, 1);
    }
  };

  struct lua_stack<bool> {
    static void push(lua_State* L, const bool &v) {
      lua_pushboolean(L, v);
    }

    static bool get(lua_State* L) {
      return luaL_checkinteger(L, 1);
    }
  };

  struct lua_stack<const char*> {
    static void push(lua_State* L, const char* s) {
      lua_pushstring(L, s);
    }

    static const char* get(lua_State* L) {
      return luaL_checkstring(L, 1);
    }
  };

  struct lua_stack<std::string> {
    static void push(lua_State* L, const std::string &s) {
      lua_pushstring(L, s.c_str());
    }

    static std::string get(lua_State* L) {
      return luaL_checkstring(L, 1);
    }
  };

  struct lua_stack<void*> {
    static void push(lua_State* L, void* p) {
      lua_pushlightuserdata(L, p);
    }

    static void* get(lua_State* L) {
      if (lua_islightuserdata(L, 1)) {
        return lua_touserdata(L, 1);
      } else {
        luaL_error(L, "%s: expected userdatum at arg #1, but got %s", __func__, luaL_typename(L, 1));
        return nullptr;
      }
    }
  };
}