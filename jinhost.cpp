#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace jin::host {
  class dll {
    void* handle = nullptr;

  public:
    dll (void) = default;

    dll (const std::string& path) { open(path); }

    ~dll (void) { close(); }

    dll (const dll&) = delete;
    dll& operator=(const dll&) = delete;

    bool open(const std::string& path) {
      close();
  #if defined(_WIN32)
      handle = reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
  #else
      handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  #endif
      return handle != nullptr;
    }

    void close (void) {
      if (!handle) return;
  #if defined(_WIN32)
      FreeLibrary(reinterpret_cast<HMODULE>(handle));
  #else
      dlclose(handle);
  #endif
      handle = nullptr;
    }

    void* get(const char* name) const {
      if (!handle) return nullptr;
  #if defined(_WIN32)
      return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
  #else
      return dlsym(handle, name);
  #endif
    }

    explicit operator bool() const { return handle != nullptr; }
  };

  struct jcfun {
    void* funptr;
    std::string src;
  };

  struct jcontext {
    std::vector<std::unique_ptr<dll>> dlls;

    struct {
      std::vector<jcfun> loadedfuns;
    } debug;
  };

  using jctxptr = std::shared_ptr<jcontext>;

  jctxptr newjin (void) {
    return std::make_shared<jcontext>();
  }

  bool loadlib (const jctxptr& ctx, const std::string& path) {
    if (!ctx) return false;

    auto lib = std::make_unique<dll>(path);
    if (!*lib) return false;

    ctx->dlls.push_back(std::move(lib));
    return true;
  }

  void* jdlsym (const std::string& symname, const jctxptr& ctx) {
    if (!ctx) return nullptr;

    for (const auto& f: ctx->debug.loadedfuns) {
      if (f.src == symname) return f.funptr;
    }

    for (const auto& lib : ctx->dlls) {
      if (void* sym = lib->get(symname.c_str())) {
        ctx->debug.loadedfuns.push_back({sym, symname});
        return sym;
      }
    }

    return nullptr;
  }
} // namespace jin::host
