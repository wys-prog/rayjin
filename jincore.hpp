#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "jinhost.hpp"

namespace fs = std::filesystem;

std::ostream& operator<<(std::ostream& os, __uint128_t n) {
  char buf[40];
  int i = 0;

  do {
    buf[i++] = '0' + n % 10;
    n /= 10;
  } while (n);

  while (i--) os.put(buf[i]);

  return os;
}

std::ostream& operator<<(std::ostream& os, __int128_t n) {
  if (n < 0) {
   os.put('-');
   __uint128_t u = -(__uint128_t)n;
   return os << u;
  }

  return os << (__uint128_t)n;
}

namespace jin::rt {
  enum opcodes : uint8_t {
    NOP,
    CUOP,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    SMOP,
    SMADD,
    SMSUB,
    SMMUL,
    SMDIV,
    SMMOD,
    CTRL,
    ORG,
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
    DTCU,
    LOAD,
    WRITE,
    MOV,
    MOVX,
    SWAP,
    DTSM,
    SMLOAD,
    SMWRITE,
    SMMOV,
    SMMOVX,
    SMSWAP,
    BOOL,
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
    SYS,
    HALT,
    PANIC,
    BEEP
  };

  inline static const char* opcodes_string[] = {
    "NOP",
    "CUOP",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "SMOP",
    "SMADD",
    "SMSUB",
    "SMMUL",
    "SMDIV",
    "SMMOD",
    "CTRL",
    "ORG",
    "JMP",
    "JZ",
    "JNZ",
    "JQ",
    "JNQ",
    "JG",
    "JL",
    "JGQ",
    "JLQ",
    "CMPCU",
    "CMPSM",
    "TESTCU",
    "TESTSM",
    "DTCU",
    "LOAD",
    "WRITE",
    "MOV",
    "MOVX",
    "SWAP",
    "DTSM",
    "SMLOAD",
    "SMWRITE",
    "SMMOV",
    "SMMOVX",
    "SMSWAP",
    "BOOL",
    "OR",
    "XOR",
    "AND",
    "NAND",
    "NOT",
    "SMBOOL",
    "SMOR",
    "SMXOR",
    "SMAND",
    "SMNAND",
    "SMNOT",
    "SHL",
    "SHR",
    "SMSHL",
    "SMSHR",
    "SYS",
    "HALT",
    "PANIC",
    "BEE",
  };

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

  class v_varch_base {
  public:
    virtual void run() = 0;
    virtual void load(std::vector<uint8_t> mem, std::vector<void *>) = 0;
  };

  template <auto vregcount, typename vcu, typename vsimd, typename vsimdintegral>
  class v_varch : public v_varch_base {
  public:
    static inline constexpr auto vmask = (vregcount - 1);
    static inline constexpr auto vreg = [](auto b) -> auto { return b & vmask; };
    using vcu_t = vcu;
    using vsimd_t = vsimd;
    using vsimd_integral_t = vsimdintegral;
    static inline constexpr auto virtual_register_count = vregcount;

    struct vthread;
    typedef void (*vcfun)(vthread *);

    using vthread_t = vthread;
    using vcfun_t = vcfun;

    struct vthread {
      uint8_t* pmem{0};
      vcfun* vclib{0};
      vsimd simd[vregcount]{0};
      vcu cu[vregcount]{0}; // Calcul Unit
      vcu lmem{0};
      vcu lvclib{0};
      vcu ip{0};
      vcu id{0};
      vcu pzdata{0}; // Protected Zone Data
      uint8_t flag{0};
    };

    vcu org {32};

    static std::string name() {
      return "varchc" + std::to_string(vregcount) + "x" +
            std::to_string(sizeof(vcu) * 8) + "X" +
            std::to_string(sizeof(vsimd) * 8);
    }

    uint8_t fetch(vthread& th) { /* FUCKING REF. */
      if (th.ip + 1 > th.lmem) { /* reading out of bounds! */
        panic(th, "reading out of bounds!", th.ip);
        return -1;
      }

      std::cout << "fetching! " << th.ip << std::endl;

      return th.pmem[th.ip++];
    }

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

    void jump(vthread& th, vcu at) {
      if (at < org) {
        /** in that case it is a C function */
        if (th.lvclib < at) return panic(th, "virtual C function out of range", at);
        try {
          th.vclib[at](&th);
        } catch (const std::exception &e) {
          panic(th, e.what(), 0x00);
        } catch (...) {
          panic(th, "unknown C++ exception caught", 0x00);
        }
      } else {
        /** otherwise, it is native code. */
        th.ip = at - org;
      }
    }

    bool vdo(vthread &th, uint8_t opc) {
      switch (opc) {
        case NOP:
          break;
        case ADD: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<add<vcu>>(a, b, th, vcu{});
          break;
        }
        case SUB: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<sub<vcu>>(a, b, th, vcu{});
          break;
        }
        case MUL: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<mul<vcu>>(a, b, th, vcu{});
          break;
        }
        case DIV: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<div<vcu>>(a, b, th, vcu{});
          break;
        }
        case MOD: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<mod<vcu>>(a, b, th, vcu{});
          break;
        }
        case SMADD: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<add<vsimd>>(a, b, th, vsimd{});
          break;
        }
        case SMSUB: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<sub<vsimd>>(a, b, th, vsimd{});
          break;
        }
        case SMMUL: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<mul<vsimd>>(a, b, th, vsimd{});
          break;
        }
        case SMDIV: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<div<vsimd>>(a, b, th, vsimd{});
          break;
        }
        case SMMOD: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<mod<vsimd>>(a, b, th, vsimd{});
          break;
        }
        case CMPCU: {
          auto ra = fetch(th);
          auto rb = fetch(th);
          auto a = th.cu[vreg(ra)], b = th.cu[vreg(rb)];
          if (a > b)
            th.flag = FLAG_GREATER;
          else if (a < b)
            th.flag = FLAG_LESSER;
          else
            th.flag = FLAG_EQUAL;
          break;
        }
        case CMPSM: {
          auto ra = fetch(th);
          auto rb = fetch(th);
          auto a = th.simd[vreg(ra)], b = th.simd[vreg(rb)];
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
        case ORG:
          org = th.cu[vreg(fetch(th))];
          break;
        case JMP:
          jump(th, th.cu[vreg(fetch(th))]);
          break;
        case JZ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_ZERO) jump(th, dst);
          break;
        }
        case JNZ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag != FLAG_ZERO) jump(th, dst);
          break;
        }
        case JG: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_GREATER) jump(th, dst);
          break;
        }
        case JL: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_LESSER) jump(th, dst);
          break;
        }
        case JQ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_EQUAL) jump(th, dst);
          break;
        }
        case JNQ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag != FLAG_EQUAL) jump(th, dst);
          break;
        }
        case JLQ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_EQUAL || th.flag == FLAG_LESSER) jump(th, dst);
          break;
        }
        case JGQ: {
          auto dst = th.cu[vreg(fetch(th))];
          if (th.flag == FLAG_EQUAL || th.flag == FLAG_GREATER) jump(th, dst);
          break;
        }
        case OR: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<bit_or<vcu>>(a, b, th, vcu{});
          break;
        }
        case XOR: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<bit_xor<vcu>>(a, b, th, vcu{});
          break;
        }
        case AND: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<bit_and<vcu>>(a, b, th, vcu{});
          break;
        }
        case NAND: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<bit_nand<vcu>>(a, b, th, vcu{});
          break;
        }
        case SHR: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<shr<vcu>>(a, b, th, vcu{});
          break;
        }
        case SHL: {
          auto a = fetch(th);
          auto b = fetch(th);
          dovop<shl<vcu>>(a, b, th, vcu{});
          break;
        }
        case LOAD: {
          auto adr = th.cu[vreg(fetch(th))] - org;
          auto &dst = th.cu[vreg(fetch(th))];

          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }

          dst = read_le<vcu>(th.pmem + adr);
        } break;
        case WRITE: {
          auto adr = th.cu[vreg(fetch(th))] - org;
          auto &val = th.cu[vreg(fetch(th))];

          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }

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
          auto adr = th.cu[vreg(fetch(th))] - org;
          auto &dst = th.simd[vreg(fetch(th))];

          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }

          auto buff = read_le<vsimdintegral>(th.pmem + adr);
          dst = *((vsimd *)&buff);
        } break;
        case SMWRITE: {
          auto adr = th.cu[vreg(fetch(th))] - org;
          auto &val = th.simd[vreg(fetch(th))];
          vsimdintegral integral = *((vsimdintegral *)&val);

          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }

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
        case HALT:
          th.flag = FLAG_STOP;
          break;
        case PANIC:
          panic(th, "user panic!", th.cu[vreg(fetch(th))]);
          return false;
        case BEEP:
          std::cout << "<rayjin.vm.VThread@" << th.id << ": <beep>" << std::endl;
          break;
        default:
          panic(th, "illegal opcode", opc);
          return false;
      }

      return true;
    }

    void vloop(vthread &th) {
      while (! watcher_init.load()) std::this_thread::sleep_for(std::chrono::nanoseconds(100));

      while (th.flag != FLAG_STOP) {
        auto opcode = fetch(th);
        if (! vdo(th, opcode)) break;
      }
    }

    std::thread vspawn(std::shared_ptr<vthread> vth) {
      return std::thread{[thread = vth, this](void) mutable -> void { vloop(*thread); }};
    }

    struct panicstruct {
      std::string message;
      vthread thread;
      vcu userdatum;
    };

    struct vthreaddata {
      std::shared_ptr<vthread> thread;
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
    std::atomic<bool> watcher_init = false;
    std::atomic<bool> panicking = false;
    std::atomic<panicstruct*> current_panic{};
    std::atomic<vcu> thread_ids = 0;

    inline void panic (vthread &th, const char *msg, vcu udatum) {
      std::this_thread::sleep_for(std::chrono::milliseconds(590));
      std::cout << "<rayjin.panic> a panic attack was simulated" << std::endl;
      panicstruct *pan = new panicstruct({.message = msg, .thread = th, .userdatum = udatum});

      current_panic.store(pan);
      panicking.store(true);
    }

    void finish_threadpool (void) {
      std::lock_guard<std::mutex> lock(vthreadpool_mtx);

      for (auto &thread : vthreadpool) {
        std::cout << "waiting for native v-thread " << thread.second.thread->id
                  << std::endl;
        thread.first.join();
      }
    }

    void watcher_thread_update(void) {
      std::cout << "<rayjin.watcherThread>: a watcher was initialized" << std::endl;
      bool even = false;
      watcher_init.store(true);

      while (true) {
        even = !even;
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
        
        if (panicking.load()) {
          std::cerr << std::format("<rayjin.watcherThread.thread:{}> panicking!", virtual_id.load())
          << std::endl;
          panicstruct *pan = current_panic.load();
          std::cerr << std::format(
            "on sub-vthread {} relative to thread {}, '{}' ({})\n",
            pan->thread.id, virtual_id.load(), pan->message, pan->userdatum
          );
          
          finish_threadpool();
          delete pan;
          current_panic.store(nullptr);
          
          break;
        }
        
        if (even) if (main_exiting.load()) break;;
      }
      
      std::cout << "<rayjin.watcherThread>: a watcher finished its execution" << std::endl;
    }

    void v_vm_spawn_vthread(vthread *vth) {
      std::lock_guard<std::mutex> lock(memory_mtx);
      auto size = vth->cu[0];
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
          .thread = std::make_shared<vthread>(child),
          .funcs = funcs,
      };

      data.thread.id = thread_ids.fetch_add(1);
      std::pair<std::thread, vthreaddata> v_elem;
      v_elem.second = data;
      v_elem.first = vspawn(data->thread);

      std::lock_guard<std::mutex> lock3(vthreadpool_mtx);
      vthreadpool.emplace_back(v_elem);
      vth->cu[0] = 1;
    }

    void load(std::vector<uint8_t> mem, std::vector<void *> funcs) override {
      mainvth.vclib = (vcfun*)funcs.data();
      memory = std::vector<uint8_t>{mem.begin(), mem.end()};
      std::cout << "at load, " << memory.size() << std::endl;
    }

    void run() override {
      vthread mainth{};
      mainth.vclib = shared_vclib.data();
      std::cout << mainth.lmem << " memory!" << std::endl;
      for (size_t i = 0; i < mainth.lmem; i++) {
        std::cout << (int)mainth.pmem[i] << '-' << std::flush;
      }
      

      watcherthead = std::thread{[this]() -> void { this->watcher_thread_update(); }};
      vloop(mainth);
      main_exiting.store(true);
      watcherthead.join();
    }

    static_assert((((vregcount) & (vregcount - 1)) == 0 && vregcount >= 4), "vregcount should be a power of 2!");
  };
} // namespace jin::rt