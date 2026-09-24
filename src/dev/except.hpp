#pragma once

#include <string>
#include <stdexcept>

namespace rayjin::dev {
  class rayjin_exception : public std::runtime_error {
  public:
    rayjin_exception();
    rayjin_exception(const std::string&);
  };

  class lua_exception : public rayjin_exception {
  public:
    lua_exception(const std::string&);
  };

  class lua_bad_alloc : public lua_exception, public std::bad_alloc {
  public:
    lua_bad_alloc(const std::string &tpname, size_t requestsize);
  };
}