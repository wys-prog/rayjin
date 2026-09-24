#include <format>
#include <string>
#include <stdexcept>
#include "except.hpp"

namespace rayjin::dev {
  rayjin_exception::rayjin_exception(): std::runtime_error("<rayjin exception>") {}

  rayjin_exception::rayjin_exception(const std::string &whut): std::runtime_error(whut) {}

  lua_exception::lua_exception(const std::string &whut): rayjin_exception(whut) {}

  lua_bad_alloc::lua_bad_alloc(const std::string &tpname, size_t requestsize): lua_exception(
    std::format("lua bad alloc when requesting {} bytes (with type {})", requestsize, tpname)
  ) {}
}
