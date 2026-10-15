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
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "jinhost.hpp"

namespace fs = std::filesystem;

inline std::ostream& operator<<(std::ostream& os, __uint128_t n) {
  char buf[40];
  int i = 0;

  do {
    buf[i++] = '0' + n % 10;
    n /= 10;
  } while (n);

  while (i--) os.put(buf[i]);

  return os;
}

inline std::ostream& operator<<(std::ostream& os, __int128_t n) {
  if (n < 0) {
   os.put('-');
   __uint128_t u = -static_cast<__uint128_t>(n);
   return os << u;
  }

  return os << static_cast<__uint128_t>(n);
}

namespace jin::rt {
  extern std::mutex stdcppwrite_global_mutex;

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
    READ,
    WRITE,
    MOV,
    MOVX,
    SWAP,
    DTSM,
    SMLOAD,
    SMREAD,
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
    BEEP,
    MOVCUSM,
    MOVSMCU,
    CALL,
    RET,
  };
  
  inline static const char* opcodes_string[] {
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
    "READ",
    "WRITE",
    "MOV",
    "MOVX",
    "SWAP",
    "DTSM",
    "SMLOAD",
    "SMREAD",
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
    "BEEP",
    "MOVCUSM",
    "MOVSMCU",
    "CALL",
    "RET",
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
    virtual void load(std::vector<uint8_t>, std::vector<uint8_t>, std::vector<void*>) = 0;
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
      std::stack<vcu> callstack;
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

    thread_local vcu org {32};

    static std::string name() {
      return "varchc" + std::to_string(vregcount) + "x" +
            std::to_string(sizeof(vcu) * 8) + "X" +
            std::to_string(sizeof(vsimd) * 8);
    }

    uint8_t fetch(vthread& th) { /* FUCKING REF. */
      if (th.ip + 1 > th.lmem) { /* reading out of bounds! */
        panic(th, "segmentation fault", th.ip);
        return -1;
      }

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
          auto& dst = th.cu[vreg(fetch(th))];
          auto value = read_le<vcu>(th.pmem + th.ip);
          th.ip += sizeof(vcu);
          dst = value;
        } break;
        case READ: {
          auto &dst = th.cu[vreg(fetch(th))];
          auto adr = th.cu[vreg(fetch(th))] - org;
  
          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }
  
          dst = read_le<vcu>(th.pmem + adr);
          th.ip += sizeof(vcu);
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
          auto value = read_le<vsimdintegral>(th.pmem+th.ip);
          th.ip += sizeof(vcu);
          auto& dst = th.simd[vreg(fetch(th))];

          dst = *((vsimd*)&value);
        } break;
        case SMREAD: {
          auto adr = th.cu[vreg(fetch(th))] - org;
          auto &dst = th.simd[vreg(fetch(th))];
  
          if (adr >= th.pzdata) {
            panic(th, "segmentation fault", adr);
            return false;
          }
  
          auto buff = read_le<vsimdintegral>(th.pmem + adr);
          th.ip += sizeof(vcu);
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
        case BEEP: {
          std::lock_guard<std::mutex> lock1{stdcppwrite_global_mutex};
          std::cout << "<rayjin.vm.VThread@" << th.id << ": <beep>" << std::endl;
          break;
        }
        case MOVCUSM: {
          auto src = th.cu[vreg(fetch(th))];
          th.simd[vreg(fetch(th))] = src;
          break;
        }
        case MOVSMCU: {
          auto src = th.simd[vreg(fetch(th))];
          th.cu[vreg(fetch(th))] = src;
          break;
        }
        case CALL: {
          th.callstack.push(th.ip+2);
          th.ip = th.cu[vreg(fetch(th))];
          std::cout << "calling " << (unsigned long)th.ip << std::endl;
          break;
        }
        case RET: {
          if (! th.callstack.empty()) {
            th.ip = th.callstack.top();
            th.callstack.pop();
          } else panic(th, "retourning on an empty callstack!!", th.callstack.size());
          break;
        }
        default:
          panic(th, "illegal opcode", opc);
          return false;
      }

      return true;
    }

    void vloop(vthread &th) {
      while (! watcher_init.load()) std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
      std::cout << "<rayjin.vloop>: thread " << th.id << " is starting!" << std::endl;

      while (th.flag != FLAG_STOP) {
        auto opcode = fetch(th);
        if (! vdo(th, opcode)) break;
      }

      main_exiting.store(true);

      std::lock_guard<std::mutex> lock1{memory_mtx};
      std::erase_if(vmemmap, [&](auto p) { return (uintptr_t)th.pmem == p.first; });
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

    std::vector<uint8_t> disk;
    std::mutex disk_mtx;

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
      panicstruct *pan = new panicstruct({.message = msg, .thread = th, .userdatum = udatum});

      /** rollback! */
      pan->thread.ip -= sizeof(vcu);

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

    static size_t instruction_size(
      uint8_t opcode,
      size_t remaining
    ) {
      size_t size = 0;

      switch (opcode) {
        case NOP:
        case HALT:
        case BEEP: break;

        case TESTCU:
        case TESTSM:
        case ORG:
        case JMP:
        case JZ:
        case JNZ:
        case JQ:
        case JNQ:
        case JG:
        case JL:
        case JGQ:
        case JLQ:
        case PANIC: size += 1; break;

        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case MOD:
        case SMADD:
        case SMSUB:
        case SMMUL:
        case SMDIV:
        case SMMOD:
        case CMPCU:
        case CMPSM:
        case OR:
        case XOR:
        case AND:
        case NAND:
        case SHR:
        case SHL:
        case MOV:
        case MOVX:
        case SWAP:
        case READ:
        case WRITE:
        case SMMOV:
        case SMMOVX:
        case SMSWAP: size += 2; break;

        case LOAD: size += 1 + sizeof(vcu); break;
        case SMLOAD: size += 1 + sizeof(vsimd); break;

        default: break;
      }

      return size <= remaining ? size : remaining;
    }

    static constexpr const char* reset   = "\033[0m";
    static constexpr const char* dim     = "\033[2m";
    static constexpr const char* bold    = "\033[1m";
    static constexpr const char* cyan    = "\033[38;2;105;200;220m";
    static constexpr const char* purple  = "\033[38;2;195;155;255m";
    static constexpr const char* gold    = "\033[38;2;235;190;120m";
    static constexpr const char* red     = "\033[38;2;255;115;115m";
    static constexpr const char* white   = "\033[38;2;220;225;235m";

    static std::string dump (const uint8_t* p, size_t len, size_t& read, const char* rest) {
      std::ostringstream oss{""};
      oss << " ";
      if (p[0] < std::size(opcodes_string)) {
        oss << std::hex << cyan << (unsigned)p[0] << std::setw(2) << std::setfill('0')
            << "  " << red << std::setw(8) << std::setfill(' ') << opcodes_string[p[0]] << rest << " ";
        auto rem = instruction_size(p[0], len);
        oss << purple;

        for (auto i{0ull}; i < rem; i++) {
          oss << std::hex << (unsigned)p[i] << std::setw(sizeof(vcu)*2) << std::setfill('0') << ", ";
          read++;
        }

        oss << rest << std::endl;
      } else {
        oss << std::hex << white << std::setw(sizeof(vcu)*2) << std::setfill('0')
            << (unsigned)p[0] << rest << std::endl;
      }

      return oss.str();
    }

    static std::string dump_memory_segment (const uint8_t* p, size_t len, size_t ip) {
      std::ostringstream out;

      for (auto i{0ull}; i < len; i++) {
        size_t read{0};
        std::string rest = reset;

        if (i == ip) {
          out << bold;
          rest += bold;
        }

        out << cyan << std::hex << std::setw(16) << std::setfill('0') 
            << (size_t)(p + i) << reset << ((i == ip) ? " -> " : "    ")
            << std::setw(sizeof(vcu)*2) << std::setfill('0') << (size_t)i
            << dump(p + i, len - 1, read, rest.c_str());
        i += read;
      }
      
      return out.str();
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
            "on sub-vthread {} relative to thread {}, '{}' ({})\nmemory dump:\n{}\n",
            pan->thread.id, virtual_id.load(), pan->message, pan->userdatum, 
            dump_memory_segment(pan->thread.pmem, pan->thread.lmem, pan->thread.ip)
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

    void v_vm_load_vthread_memory (vthread* vth, vcu start, vcu len) {
      std::lock_guard<std::mutex> lock(disk_mtx);
      for (vcu i{0}; i < std::min(len, vth->lmem); i++) {
        vth.pmem[i] = disk[i];
      }
    }

    bool v_vm_get_memory_segment (vthread* vth, size_t size) {
      vcu pstart = 0;
      bool found = false;

      for (auto [segstart, seglen] : vmemmap) {
        /* Is there enough space before this segment? */
        if (segstart >= pstart && segstart - pstart >= size) {
          found = true;
          break;
        }

        /* Move candidate past this allocated segment. */
        if (segstart + seglen > pstart) pstart = segstart + seglen;
      }

      /* Check space after the final allocated segment. */
      if (!found && memory.size() - pstart >= size) {
        found = true;
      }

      if (!found) {
        vth->cu[0] = 0;
        return false;
      } else {
        /* Otherwise, ima reserve the region */
        vmemmap.push_back({
          pstart, size
        });

        vth->pmem = memory.data() + pstart;
        vth->lmem = size;
        return true;
      }
    }

    void v_vm_spawn_vthread (vthread *vth) {
      std::lock_guard<std::mutex> lock(memory_mtx);
      auto size = vth->cu[0];
      auto codestart = vth->cu[1];
      auto codelen = vth->cu[2];
      vthread child{};

      if (! v_vm_get_memory_segment(&child, size)) {
        vth->cu[0] = 0;
        return;
      }

      v_vm_load_vthread_memory(&child, codestart, codelen);

      std::lock_guard<std::mutex> lock2(shared_vclib_mtx);
      std::vector<vcfun> funcs = shared_vclib;
      vthreaddata data{
        .thread = std::make_shared<vthread>(child),
        .funcs = funcs,
      };

      data.thread->id = thread_ids.fetch_add(1);
      data.thread->lmem = size;
      data.thread->lvclib = data.funcs.size();
      data.thread->vclib = data.funcs.data();

      std::pair<std::thread, vthreaddata> v_elem;
      v_elem.second = data;
      v_elem.first = vspawn(data->thread);

      std::lock_guard<std::mutex> lock3(vthreadpool_mtx);
      vthreadpool.emplace_back(v_elem);
      vth->cu[0] = 1;
    }

    void load(std::vector<uint8_t> mem, std::vector<uint8_t> dsk, std::vector<void*> funcs) override {
      mainvth.vclib = (vcfun*)funcs.data();
      memory = mem;
      disk = dsk;
    }

    void run() override {
      vthread mainth{};
      mainth.vclib = shared_vclib.data();

      vmemmap.push_back({0, 1024});

      mainth.lmem = 1024;
      mainth.pmem = this->memory.data();
      watcherthead = std::thread{[this]() -> void { this->watcher_thread_update(); }};
      vloop(mainth);
      main_exiting.store(true);
      watcherthead.join();
    }

    static_assert((((vregcount) & (vregcount - 1)) == 0 && vregcount >= 4), "vregcount should be a power of 2!");
  };
} // namespace jin::rt