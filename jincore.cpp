#include <stack>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <functional>
#include <unordered_map>
#include <filesystem>
#include <vector>

#include "jinhost.hpp"
#include "jincore.hpp"
#include "jinstdarches.hpp"

namespace fs = std::filesystem;

namespace jin::rt {
  std::mutex stdcppwrite_global_mutex;

  namespace app {
    struct varch_symbol {
      std::shared_ptr<jin::rt::v_varch_base> (*vnew)(void);
    };

    template <auto vregcount, typename vcu, typename vsimd, typename vsimdintegral>
    std::pair<std::string, varch_symbol> mk_arch (void) {
      using v = v_varch<vregcount, vcu, vsimd, vsimdintegral>;
      auto name = v::name();
      auto vnew = [](void) -> std::shared_ptr<jin::rt::v_varch_base> {
        return std::make_shared<v>();
      };
      return {name, {.vnew = vnew}};
    }

    thread_local std::unordered_map<std::string, varch_symbol> builtin_arches{
      mk_arch<16, uint16_t, sized::float32, sized::integral_storage_t<sized::float32>>(),
      mk_arch<32, uint64_t, sized::float64, sized::integral_storage_t<sized::float64>>(),
      mk_arch<32, sized::largeint, sized::largefloat, sized::integral_storage_t<sized::largefloat>>(),
    };

    static constexpr uint32_t V_DISK_MAGIC = 0x5241594A;

    struct vdiskheader {
      uint32_t magic{0};
      uint32_t version;
      char     varchname[32];
      uint32_t cfuncs{0};
    };

    bool canloadarch (vdiskheader header) {
      for (const auto&arch: builtin_arches) {
        if (header.varchname == arch.first) {
          return true;
        }
      }

      return false;
    }

    varch_symbol loadarch (vdiskheader header) {
      for (const auto&arch: builtin_arches) {
        if (header.varchname == arch.first) {
          return arch.second;
        }
      }

      return {};
    }

    std::pair<bool, vdiskheader> loaddisk (std::istream& disk, const std::string& filename) {
      vdiskheader header{};
      disk.read((char*)&header, sizeof(header));

      if (header.magic != V_DISK_MAGIC) {
        std::cerr << std::format("{}: invalid magic header! (should be {})", filename, V_DISK_MAGIC) << std::endl;
        return {false, {}};
      }

      if (! canloadarch(header)) {
        std::cerr << std::format("{}: no such architecture found for this disk!", filename);
        return {false, {}};
      }

      return {true, header};
    }

    bool loadcsymbols (const std::string& file, const std::vector<std::string> &symbols, const host::jctxptr &ctx, std::vector<void*>& out) {
      std::vector<void*> funcs{};

      for (const auto&sym: symbols) {
        auto fun = host::jdlsym(sym, ctx);
        if (! fun) {
          std::cerr << file << ": unable to load function '" << sym << "': not found in the jinhost" << std::endl;
          return false;
        }
      }

      out = funcs;
      return true;
    }

    bool loadsymbols (std::istream& disk, const std::string& file, uint32_t nfuncs, const host::jctxptr& ctx, std::vector<void*>& out) {
      std::vector<std::string> syms{};
      syms.reserve(nfuncs);
      std::string symname{};
      uint32_t i{0};

      while (i++ < nfuncs && std::getline(disk, symname, '\0')) {
        syms.push_back(symname);
      }

      if (syms.size() < nfuncs) {
        std::cerr << file << ": missing " << nfuncs - syms.size() << " C-function references in the disk file!" << std::endl;
        return false;
      }

      if (! loadcsymbols(file, syms, ctx, out)) return false;

      return true;
    }

    auto preparevm (const std::string& file, const host::jctxptr& ctx) {
      std::ifstream disk{file, std::ios::binary};
      auto [success, header] = loaddisk(disk, file);
      if (! success) throw std::runtime_error("cannot run");

      std::vector<void*> funcs;
      if (! loadsymbols(disk, file, header.cfuncs, ctx, funcs)) throw std::runtime_error("cannot run");

      std::vector<uint8_t> buff{std::istreambuf_iterator<char>(disk), std::istreambuf_iterator<char>()};
      buff.shrink_to_fit();

      if (! canloadarch(header)) {
        std::cerr << file << ": no such arch found for this file!" << std::endl;
        throw std::runtime_error("cannot run");
      }

      auto varch = loadarch(header);
      auto obj = varch.vnew();

      obj->load(buff, funcs);

      return obj;
    }
  }
}

namespace config {
  static bool debug_server = false;
  static std::vector<std::string> programs{};
  static auto jirun = jin::host::newjin();
  static std::vector<std::unique_ptr<std::thread>> global_run_threadpool{};
  using taskfunc = std::function<void(std::shared_ptr<jin::rt::v_varch_base> arch)>;
  // todo: -Xmem? ou -Xmem-max?

  static taskfunc task = [](auto arch) -> void {
    arch->run();
  };
}

static std::unordered_map<std::string, std::function<void(std::vector<std::string>&, size_t&)>> arghandler {
  {"--servering", [](auto&, auto&) { 
    config::task = [&](std::shared_ptr<jin::rt::v_varch_base> arch) -> void {
      config::global_run_threadpool.push_back(std::make_unique<std::thread>([arch = arch]() { 
        arch->run(); 
      }));
    };
  }},
  {"--debug-server", [](auto&, auto&) { config::debug_server = true; }}, // + Visual VM?
  {"--nlib", [](auto& vec, auto& pos) { 
    if (pos + 1 >= vec.size()) std::cerr << "expected native library path after argument '--nlib'!" << std::endl;
    else jin::host::loadlib(config::jirun, vec[++pos]);
  }},
  {"--arches", [](auto&, auto&) {
    for (const auto&a: jin::rt::app::builtin_arches) {
      std::cout << a.first << std::endl;
    }
  }}
};

int main(int argc, char** argv) {
  std::cout << "<rayjin>" << std::endl;

  std::vector<std::string> args(argv + 1, argv + argc);
  fs::path pbin = fs::path(argv[0]).parent_path();
  fs::path ppwd = fs::current_path();
  
  jin::host::loadlib(config::jirun, pbin.string());
  jin::host::loadlib(config::jirun, ppwd.string());

  for (size_t i = 0; i < args.size(); i++) {
    if (arghandler.contains(args[i])) arghandler[args[i]](args, i);
    else {
      config::programs.push_back(args[i]);
    }
  }

  std::stack<std::shared_ptr<jin::rt::v_varch_base>> runstack{};

  for (const auto& prog: config::programs) {
    try {
      auto sym = jin::rt::app::preparevm(prog, config::jirun);
      runstack.push(sym);
    } catch (const std::exception& e) {
      std::cerr << "cannot prepare a virtual machine for the file '" << prog << "': " << e.what() << std::endl;
    }
  }

  while (! runstack.empty()) {
    std::cout << "<rayjin.runStack>: launching task at " << runstack.top().get() << std::endl;
    config::task(runstack.top());
    runstack.pop();
  }

  while (! config::global_run_threadpool.empty()) {
    std::erase_if(config::global_run_threadpool, [](std::unique_ptr<std::thread>& thread) {
      if (thread->joinable()) {
        thread->join();
        std::cout << "<rayjin.globalRunThreadPool>: joined thread " << thread.get() << std::endl;
        return true;
      }

      return false;
    });
  }

  return 0;
}

// Live Server
// Debug Server
/**
 * RayjinLiveServer + Rayjin C Source Provider
 */
