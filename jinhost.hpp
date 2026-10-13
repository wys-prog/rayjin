#pragma once

#include <string>
#include <memory>
#include <iostream>

namespace jin::host {
  struct jcontext;
  using jctxptr = std::shared_ptr<jcontext>;

  jctxptr newjin ();
  void* jdlsym (const std::string& symname, const jctxptr& ctx);
  bool loadlib (const jctxptr& ctx, const std::string& path);
}

