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

namespace fs = std::filesystem;

namespace jin::rt {
  template <typename T> static void write_le(uint8_t *dst, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
      dst[i] = static_cast<uint8_t>(value >> (i * 8));
    }
  }

  template <typename T> static T read_le(const uint8_t *src) {
    T value = 0;

    for (size_t i = 0; i < sizeof(T); ++i) {
      value |= static_cast<T>(src[i]) << (i * 8);
    }

    return value;
  }

  template <auto vregcount, typename vcu, typename vsimd, typename vsimdintegral>
  class v_varch {
  public:
    static inline constexpr auto vmask = (vregcount - 1);
    static inline constexpr auto vreg = [](auto b) -> auto { return b & vmask; };

    struct vthread;
    typedef void (*vcfun)(vthread *);

    struct vthread {
      uint8_t *pmem{0};
      vcfun *vclib{0};
      vsimd simd[16]{0};
      vcu cu[16]{0}; // Calcul Unit
      vcu lmem{0};
      vcu lvclib{0};
      vcu ip{0};
      vcu id{0};
      vcu pzdata{0}; // Protected Zone Data
      uint8_t flag{0};
    };

    uint8_t fetch(vthread &th) {
      if (th.ip + 1 >= th.lmem) { /* reading out of bounds! */
        panic(th, "reading out of bounds!", th.ip);
        return -1;
      }

      return th.pmem[th.ip++];
    }

    enum opcodes : uint8_t {
      NOP = 0x00, // X

      CUOP = 0x31, // X
      ADD,
      SUB,
      MUL,
      DIV,
      MOD,

      SMOP = 0x52, // X
      SMADD,
      SMSUB,
      SMMUL,
      SMDIV,
      SMMOD,

      CTRL = 0x87, // X
      JMP,
      JZ,
      JNZ,
      JQ,
      JNQ,
      JG,
      JL,
      JGQ,
      JLQ,
      CMPCU,
      CMPSM,
      TESTCU,
      TESTSM,

      DTCU = 0xA3, // X
      LOAD,
      WRITE,
      MOV,
      MOVX,
      SWAP,

      DTSM = 0xC1, // X
      SMLOAD,
      SMWRITE,
      SMMOV,
      SMMOVX,
      SMSWAP,

      BOOL = 0xD7, // X
      OR,
      XOR,
      AND,
      NAND,
      NOT,
      SMBOOL,
      SMOR,
      SMXOR,
      SMAND,
      SMNAND,
      SMNOT,
      SHL,
      SHR,
      SMSHL,
      SMSHR,

      SYS = 0xF0,
      INT,
      HALT,
      PANIC,
      BEEP
    };

    enum flags {
      FLAG_NONE = 0,
      FLAG_STOP = 'S',
      FLAG_CMPWTF = 'C',
      FLAG_ZERO,
      FLAG_EQUAL,
      FLAG_GREATER,
      FLAG_LESSER,
    };

    template <auto OP>
    static void dovop(uint8_t a, uint8_t b, vthread &th, vsimd) {
      auto &ra = th.simd[vreg(a)];
      auto &rb = th.simd[vreg(b)];
      ra = OP(ra, rb);
    }

    template <auto OP> static void dovop(uint8_t a, uint8_t b, vthread &th, vcu) {
      auto &ra = th.cu[vreg(a)];
      auto &rb = th.cu[vreg(b)];
      ra = OP(ra, rb);
    }

    template <typename T>
    static constexpr auto add = [](T a, T b) { return T(a + b); };

    template <typename T>
    static constexpr auto sub = [](T a, T b) { return T(a - b); };

    template <typename T>
    static constexpr auto mul = [](T a, T b) { return T(a * b); };

    template <typename T>
    static constexpr auto div = [](T a, T b) { return T(a / b); };

    template <typename T>
    static constexpr auto mod = [](T a, T b) {
      if constexpr (std::is_floating_point_v<T>) {
        return T(std::fmod(a, b));
      } else {
        return T(a % b);
      }
    };

    template <typename T>
    static constexpr auto bit_or = [](T a, T b) { return T(a | b); };

    template <typename T>
    static constexpr auto bit_xor = [](T a, T b) { return T(a ^ b); };

    template <typename T>
    static constexpr auto bit_and = [](T a, T b) { return T(a & b); };

    template <typename T>
    static constexpr auto bit_nand = [](T a, T b) { return T(~(a & b)); };

    template <typename T>
    static constexpr auto shl = [](T a, T b) { return T(a << (b % (sizeof(T) * 8))); };

    template <typename T>
    static constexpr auto shr = [](T a, T b) { return T(a >> (b % (sizeof(T) * 8))); };

    void vdo(vthread &th, uint8_t opc) {
      switch (opc) {
      case NOP:
        break;
      case ADD:
        dovop<add<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case SUB:
        dovop<sub<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case MUL:
        dovop<mul<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case DIV:
        dovop<div<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case MOD:
        dovop<mod<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case SMADD:
        dovop<add<vsimd>>(fetch(th), fetch(th), th, vsimd{});
        break;
      case SMSUB:
        dovop<sub<vsimd>>(fetch(th), fetch(th), th, vsimd{});
        break;
      case SMMUL:
        dovop<mul<vsimd>>(fetch(th), fetch(th), th, vsimd{});
        break;
      case SMDIV:
        dovop<div<vsimd>>(fetch(th), fetch(th), th, vsimd{});
        break;
      case SMMOD:
        dovop<mod<vsimd>>(fetch(th), fetch(th), th, vsimd{});
        break;
      case CMPCU: {
        auto a = th.cu[vreg(fetch(th))], b = th.cu[vreg(fetch(th))];
        if (a > b)
          th.flag = FLAG_GREATER;
        else if (a < b)
          th.flag = FLAG_LESSER;
        else
          th.flag = FLAG_EQUAL;
        break;
      }
      case CMPSM: {
        auto a = th.simd[vreg(fetch(th))], b = th.simd[vreg(fetch(th))];
        if (a > b)
          th.flag = FLAG_GREATER;
        else if (a < b)
          th.flag = FLAG_LESSER;
        else
          th.flag = FLAG_EQUAL;
        break;
      }
      case TESTCU:
        th.flag = (th.cu[vreg(fetch(th))] == 0) ? FLAG_ZERO : FLAG_NONE;
        break;
      case TESTSM:
        th.flag = (th.simd[vreg(fetch(th))] == 0) ? FLAG_ZERO : FLAG_NONE;
        break;
      case JMP:
        th.ip = th.cu[vreg(fetch(th))];
        break;
      case JZ:
        if (th.flag == FLAG_ZERO)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JNZ:
        if (th.flag != FLAG_ZERO)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JG:
        if (th.flag == FLAG_GREATER)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JL:
        if (th.flag == FLAG_LESSER)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JQ:
        if (th.flag == FLAG_EQUAL)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JNQ:
        if (th.flag != FLAG_EQUAL)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JLQ:
        if (th.flag == FLAG_EQUAL || th.flag == FLAG_LESSER)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case JGQ:
        if (th.flag == FLAG_EQUAL || th.flag == FLAG_GREATER)
          th.ip = th.cu[vreg(fetch(th))];
        break;
      case OR:
        dovop<bit_or<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case XOR:
        dovop<bit_xor<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case AND:
        dovop<bit_and<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case NAND:
        dovop<bit_nand<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case SHR:
        dovop<shr<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case SHL:
        dovop<shl<vcu>>(fetch(th), fetch(th), th, vcu{});
        break;
      case LOAD: {
        auto adr = th.cu[vreg(fetch(th))];
        auto &dst = th.cu[vreg(fetch(th))];

        if (adr >= th.pzdata) return panic(th, "segmentation fault", adr);

        dst = read_le<vcu>(th.pmem + adr);
      } break;
      case WRITE: {
        auto adr = th.cu[vreg(fetch(th))];
        auto &val = th.cu[vreg(fetch(th))];

        if (adr >= th.pzdata) return panic(th, "segmentation fault", adr);

        write_le<vcu>(th.pmem + adr, val);
      } break;
      case MOV: {
        auto &src = th.cu[vreg(fetch(th))];
        auto &dst = th.cu[vreg(fetch(th))];
        dst = src;
      } break;
      case MOVX: {
        auto &src = th.cu[vreg(fetch(th))];
        auto &dst = th.cu[vreg(fetch(th))];
        dst = src;
        src = vcu{};
      } break;
      case SWAP: {
        auto &src = th.cu[vreg(fetch(th))];
        auto &dst = th.cu[vreg(fetch(th))];
        vcu buff{dst};
        dst = src;
        src = buff;
      } break;
      case SMLOAD: {
        auto adr = th.cu[vreg(fetch(th))];
        auto &dst = th.simd[vreg(fetch(th))];

        if (adr >= th.pzdata) return panic(th, "segmentation fault", adr);

        auto buff = read_le<vsimdintegral>(th.pmem + adr);
        dst = *((vsimd *)&buff);
      } break;
      case SMWRITE: {
        auto adr = th.cu[vreg(fetch(th))];
        auto &val = th.simd[vreg(fetch(th))];
        vsimdintegral integral = *((vsimdintegral *)&val);

        if (adr >= th.pzdata) return panic(th, "segmentation fault", adr);

        write_le<vsimdintegral>(th.pmem + adr, integral);
      } break;
      case SMMOV: {
        auto &src = th.simd[vreg(fetch(th))];
        auto &dst = th.simd[vreg(fetch(th))];
        dst = src;
      } break;
      case SMMOVX: {
        auto &src = th.simd[vreg(fetch(th))];
        auto &dst = th.simd[vreg(fetch(th))];
        dst = src;
        src = vsimd{};
      } break;
      case SMSWAP: {
        auto &src = th.simd[vreg(fetch(th))];
        auto &dst = th.simd[vreg(fetch(th))];
        vsimd buff{dst};
        dst = src;
        src = buff;
      } break;
      case INT: {
        auto adr = th.cu[vreg(fetch(th))];

        if (adr >= th.lvclib)
          return panic(th, "virtual-c segmentation fault!", adr);
        th.vclib[adr](&th);
      } break;
      case HALT:
        th.flag = FLAG_STOP;
        break;
      case PANIC:
        return panic(th, "user panic!", th.cu[vreg(fetch(th))]);
        break;
      case BEEP:
        puts("<beep>");
        break;
      default:
        return panic(th, "illegal opcode", opc);
      }
    }

    void vloop(vthread &th) {
      while (th.flag != FLAG_STOP) {
        vdo(th, fetch(th));
      }
    }

    std::thread vspawn(vthread vth) {
      return std::thread{[thread = vth, this](void) mutable -> void { vloop(thread); }};
    }

    struct panicstruct {
      std::string message;
      vthread thread;
      vcu userdatum;
    };

    struct vthreaddata {
      vthread thread;
      std::vector<vcfun> funcs;
    };

    std::vector<uint8_t> memory;
    std::vector<std::pair<vcu, vcu>> vmemmap;
    std::mutex memory_mtx;

    vthread mainvth{};
    std::thread watcherthead;
 
    std::vector<vcfun> shared_vclib;
     std::mutex shared_vclib_mtx;

    std::vector<std::pair<std::thread, vthreaddata>> vthreadpool;
    std::mutex vthreadpool_mtx;

    std::atomic<vcu> virtual_id = -1;
    std::atomic<bool> main_exiting = false;
    std::atomic<bool> watcher_exiting = false;
    std::atomic<bool> panicking = false;
    std::atomic<panicstruct *> current_panic{};
    std::atomic<vcu> thread_ids = 0;

    inline void panic(vthread &th, const char *msg, vcu udatum) {
      panicstruct *pan =
          new panicstruct({.message = msg, .thread = th, .userdatum = udatum});

      current_panic.store(pan);
    }

    void finish_threadpool(void) {
      std::lock_guard<std::mutex> lock(vthreadpool_mtx);

      for (auto &thread : vthreadpool) {
        std::cout << "waiting for native v-thread " << thread.second.thread.id
                  << std::endl;
        thread.first.join();
      }
    }

    void watcher_thread_update(void) {
      bool even = false;

      while (true) {
        even = !even;
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

        if (panicking.load()) {
          std::cerr << std::format("[thread:{}] panicking!", virtual_id.load())
                    << std::endl;
          panicstruct *pan = current_panic.load();
          std::cerr << std::format(
              "on sub-vthread {} relative to thread {}, '{}' ({})\n",
              pan->thread.id, virtual_id.load(), pan->message, pan->userdatum);

          finish_threadpool();
          delete pan;
          current_panic.store(nullptr);
          return;
        }

        if (even)
          if (main_exiting.load())
            return;
      }
    }

    void v_vm_spawn_vthread(vthread *vth) {
      std::lock_guard<std::mutex> lock(memory_mtx);
      auto size = vth->cu[0];
      auto perm = vth->cu[1];
      (void)perm;
      vthread child{};

      vcu pstart = 0;
      bool found = false;

      for (auto [segstart, seglen] : vmemmap) {
        /* Is there enough space before this segment? */
        if (segstart >= pstart && segstart - pstart >= size) {
          found = true;
          break;
        }

        /* Move candidate past this allocated segment. */
        if (segstart + seglen > pstart)
          pstart = segstart + seglen;
      }

      /* Check space after the final allocated segment. */
      if (!found && memory.size() - pstart >= size) {
        found = true;
      }

      if (!found) {
        vth->cu[0] = 0;
        return;
      }

      std::lock_guard<std::mutex> lock2(shared_vclib_mtx);
      std::vector<vcfun> funcs = shared_vclib;
      vthreaddata data{
          .thread = child,
          .funcs = funcs,
      };

      data.thread.id = thread_ids.fetch_add(1);
      std::pair<std::thread, vthreaddata> v_elem;
      v_elem.second = data;
      v_elem.first = vspawn(child);

      std::lock_guard<std::mutex> lock3(vthreadpool_mtx);
      vthreadpool.emplace_back(v_elem);
      vth->cu[0] = 1;
    }

    void load(std::vector<uint8_t> mem, std::vector<vcfun> funcs) {
      mainvth.vclib = funcs.data();
      memory = mem;
    }

    void run() {
      vthread mainth{};
      mainth.vclib = shared_vclib.data();
      watcherthead = std::thread{[this]() -> void { this->watcher_thread_update(); }};
      vloop(mainth);
      main_exiting.store(true);
      watcherthead.join();
    }

    static_assert((((vregcount) & (vregcount - 1)) == 0 && vregcount >= 4), "vregcount should be a power of 2!");
  };

  namespace app {
    struct varch_symbol {
      void (*prun)(void) = nullptr;
      void (*pload)(std::vector<uint8_t> mem, std::vector<void*> funcs) = nullptr;
      void* pdata = nullptr;

      using pload_t = decltype(pload);
      using prun_t = decltype(prun);
    };

    thread_local std::unordered_map<std::string, varch_symbol> builtin_arches{};

    void load_arches(void) {
      using ptype = varch_symbol::pload_t;
      
    }

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

    varch_symbol dofile (const std::string& file, const host::jctxptr& ctx) {
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

      return loadarch(header);
    }
  }
}

namespace config {
  static bool servering = false;
  static bool debug_server = false;
  static std::vector<std::string> programs{};
  static auto jirun = jin::host::newjin();
}

static std::unordered_map<std::string, std::function<void(std::vector<std::string>&, size_t&)>> arghandler {
  {"--servering", [](auto& vec, auto& pos) { config::servering = true; }},
  {"--debug-server", [](auto& vec, auto& pos) { config::debug_server = true; }},
  {"--nlib", [](auto& vec, auto& pos) { 
    if (pos + 1 >= vec.size()) std::cerr << "expected native library path after argument '--nlib'!" << std::endl;
    else jin::host::loadlib(config::jirun, vec[++pos]);
  }},
};

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  fs::path pbin = argv[0];
  fs::path ppwd = fs::current_path();
  
  jin::host::loadlib(config::jirun, pbin.string());
  jin::host::loadlib(config::jirun, ppwd.string());

  for (size_t i = 0; i < args.size(); i++) {
    if (arghandler.contains(args[i])) arghandler[args[i]](args, i);
    else {
      config::programs.push_back(args[i]);
    }
  }

  return 0;
}

// Live Server
// Debug Server
/**
 * RayjinLiveServer + Rayjin C Source Provider
 */
