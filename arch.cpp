#include <mutex>
#include <cmath>
#include <format>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdint>
#include <iostream>
#include <unordered_map>

#define vregcount           16
#define vmask               (vregcount - 1)
#define vreg(b)             (b & vmask)
#define local               static
#define vcu                 uint16_t
#define vsimd               float
#define vsimdintegral       int /* for reading and writing */

#ifndef varchname
#define varchname v_arch_##vcu
#endif

#define mix(a, b) a##b

struct vthread;
typedef void(*vcfun)(vthread*);

struct vthread {
  uint8_t* pmem     {0};
  vcfun    *vclib   {0};
  vsimd    simd[16] {0};
  vcu      cu[16]   {0}; // Calcul Unit
  vcu      lmem     {0};
  vcu      lvclib   {0};
  vcu      ip       {0};
  vcu      id       {0};
  vcu      pzdata   {0}; // Protected Zone Data
  uint8_t  flag     {0};
};

local void panic (vthread& th, const char* msg, vcu udatum);

local uint8_t fetch (vthread& th) {
  if (th.ip + 1 >= th.lmem) { /* reading out of bounds! */
    panic(th, "reading out of bounds!", th.ip);
    return -1;
  }

  return th.pmem[th.ip++];
}

enum opcodes : uint8_t {
  NOP = 0x00, // X

  CUOP = 0x31, // X
  ADD, SUB, MUL, DIV, MOD,

  SMOP = 0x52, // X
  SMADD, SMSUB, SMMUL, SMDIV, SMMOD,

  CTRL = 0x87, // X
  JMP, JZ, JNZ, JQ, JNQ, JG, JL, JGQ, JLQ,
  CMPCU, CMPSM, TESTCU, TESTSM,

  DTCU = 0xA3, // X
  LOAD, WRITE, MOV, MOVX, SWAP,

  DTSM = 0xC1, // X
  SMLOAD, SMWRITE, SMMOV, SMMOVX, SMSWAP,

  BOOL = 0xD7, // X
  OR, XOR, AND, NAND, NOT,
  SMBOOL, SMOR, SMXOR, SMAND, SMNAND, SMNOT,
  SHL, SHR, SMSHL, SMSHR,

  SYS = 0xF0,
  INT, HALT, PANIC, BEEP
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

namespace complexe {
  template <typename T>
  struct opvref;

  template <>
  struct opvref<vsimd> {
    template <auto OP>
    static void dovop (uint8_t a, uint8_t b, vthread& th) {
      auto& ra = th.simd[vreg(a)];
      auto& rb = th.simd[vreg(b)];
      ra = OP(ra, rb);
    }
  };

  template <>
  struct opvref<vcu> {
    template <auto OP>
    static void dovop (uint8_t a, uint8_t b, vthread& th) {
      auto& ra = th.cu[vreg(a)];
      auto& rb = th.cu[vreg(b)];
      ra = OP(ra, rb);
    }
  };

  template <typename T>
  constexpr auto add = [](T a, T b) {
    return T(a + b);
  };

  template <typename T>
  constexpr auto sub = [](T a, T b) {
    return T(a - b);
  };

  template <typename T>
  constexpr auto mul = [](T a, T b) {
    return T(a * b);
  };

  template <typename T>
  constexpr auto div = [](T a, T b) {
    return T(a / b);
  };

  template <typename T>
  constexpr auto mod = [](T a, T b) {
    if constexpr (std::is_floating_point_v<T>) {
      return T(std::fmod(a, b));
    } else {
      return T(a % b);
    }
  };

  template <typename T>
  constexpr auto bit_or = [](T a, T b) {
    return T(a | b);
  };

  template <typename T>
  constexpr auto bit_xor = [](T a, T b) {
    return T(a ^ b);
  };

  template <typename T>
  constexpr auto bit_and = [](T a, T b) {
    return T(a & b);
  };

  template <typename T>
  constexpr auto bit_nand = [](T a, T b) {
    return T(~(a & b));
  };

  template <typename T>
  constexpr auto shl = [](T a, T b) {
    return T(a << (b % (sizeof(T) * 8)));
  };

  template <typename T>
  constexpr auto shr = [](T a, T b) {
    return T(a >> (b % (sizeof(T) * 8)));
  };
}

template <typename T>
local void write_le(uint8_t* dst, T value) {
  for (size_t i = 0; i < sizeof(T); ++i) {
    dst[i] = static_cast<uint8_t>(value >> (i * 8));
  }
}

template <typename T>
local T read_le(const uint8_t* src) {
  T value = 0;

  for (size_t i = 0; i < sizeof(T); ++i) {
    value |= static_cast<T>(src[i]) << (i * 8);
  }

  return value;
}

local void vdo (vthread& th, uint8_t opc) {
  switch (opc) {
    case NOP:   break;
    case ADD:   complexe::opvref<vcu>::dovop<complexe::add<vcu>>(fetch(th), fetch(th), th); break; 
    case SUB:   complexe::opvref<vcu>::dovop<complexe::sub<vcu>>(fetch(th), fetch(th), th); break;
    case MUL:   complexe::opvref<vcu>::dovop<complexe::mul<vcu>>(fetch(th), fetch(th), th); break;
    case DIV:   complexe::opvref<vcu>::dovop<complexe::div<vcu>>(fetch(th), fetch(th), th); break;
    case MOD:   complexe::opvref<vcu>::dovop<complexe::mod<vcu>>(fetch(th), fetch(th), th); break;
    case SMADD: complexe::opvref<vsimd>::dovop<complexe::add<vsimd>>(fetch(th), fetch(th), th); break;
    case SMSUB: complexe::opvref<vsimd>::dovop<complexe::sub<vsimd>>(fetch(th), fetch(th), th); break;
    case SMMUL: complexe::opvref<vsimd>::dovop<complexe::mul<vsimd>>(fetch(th), fetch(th), th); break;
    case SMDIV: complexe::opvref<vsimd>::dovop<complexe::div<vsimd>>(fetch(th), fetch(th), th); break;
    case SMMOD: complexe::opvref<vsimd>::dovop<complexe::mod<vsimd>>(fetch(th), fetch(th), th); break;
    case CMPCU: {
      auto a = th.cu[vreg(fetch(th))], b = th.cu[vreg(fetch(th))];
      if (a > b) th.flag = FLAG_GREATER;
      else if (a < b) th.flag = FLAG_LESSER;
      else th.flag = FLAG_EQUAL;
      break;
    }
    case CMPSM: {
      auto a = th.simd[vreg(fetch(th))], b = th.simd[vreg(fetch(th))];
      if (a > b) th.flag = FLAG_GREATER;
      else if (a < b) th.flag = FLAG_LESSER;
      else th.flag = FLAG_EQUAL;
      break;
    }
    case TESTCU: th.flag = (th.cu[vreg(fetch(th))] == 0) ? FLAG_ZERO : FLAG_NONE; break;
    case TESTSM: th.flag = (th.simd[vreg(fetch(th))] == 0) ? FLAG_ZERO : FLAG_NONE; break;
    case JMP: th.ip = th.cu[vreg(fetch(th))]; break;
    case JZ:  if (th.flag == FLAG_ZERO) th.ip = th.cu[vreg(fetch(th))]; break;
    case JNZ: if (th.flag != FLAG_ZERO) th.ip = th.cu[vreg(fetch(th))]; break;
    case JG:  if (th.flag == FLAG_GREATER) th.ip = th.cu[vreg(fetch(th))]; break;
    case JL:  if (th.flag == FLAG_LESSER) th.ip = th.cu[vreg(fetch(th))]; break;
    case JQ:  if (th.flag == FLAG_EQUAL) th.ip = th.cu[vreg(fetch(th))]; break;
    case JNQ: if (th.flag != FLAG_EQUAL) th.ip = th.cu[vreg(fetch(th))]; break;
    case JLQ: if (th.flag == FLAG_EQUAL || th.flag == FLAG_LESSER) th.ip = th.cu[vreg(fetch(th))]; break;
    case JGQ: if (th.flag == FLAG_EQUAL || th.flag == FLAG_GREATER) th.ip = th.cu[vreg(fetch(th))]; break;
    case OR:   complexe::opvref<vcu>::dovop<complexe::bit_or<vcu>>(fetch(th), fetch(th), th);  break;
    case XOR:  complexe::opvref<vcu>::dovop<complexe::bit_xor<vcu>>(fetch(th), fetch(th), th); break;
    case AND:  complexe::opvref<vcu>::dovop<complexe::bit_and<vcu>>(fetch(th), fetch(th), th); break;
    case NAND: complexe::opvref<vcu>::dovop<complexe::bit_nand<vcu>>(fetch(th), fetch(th), th); break;
    case SHR:  complexe::opvref<vcu>::dovop<complexe::shr<vcu>>(fetch(th), fetch(th), th); break;
    case SHL:  complexe::opvref<vcu>::dovop<complexe::shl<vcu>>(fetch(th), fetch(th), th); break;
    case LOAD: {
      auto  adr = th.cu[vreg(fetch(th))];
      auto& dst = th.cu[vreg(fetch(th))];

      if (adr >= th.pzdata) return panic (th, "segmentation fault", adr);

      dst = read_le<vcu>(th.pmem + adr);
    } break;
    case WRITE: {
      auto  adr = th.cu[vreg(fetch(th))];
      auto& val = th.cu[vreg(fetch(th))];

      if (adr >= th.pzdata) return panic (th, "segmentation fault", adr);

      write_le<vcu>(th.pmem + adr, val);
    } break;
    case MOV: {
      auto& src = th.cu[vreg(fetch(th))];
      auto& dst = th.cu[vreg(fetch(th))];
      dst = src;
    } break;
    case MOVX: {
      auto& src = th.cu[vreg(fetch(th))];
      auto& dst = th.cu[vreg(fetch(th))];
      dst = src;
      src = vcu{};
    } break;
    case SWAP: {
      auto& src = th.cu[vreg(fetch(th))];
      auto& dst = th.cu[vreg(fetch(th))];
      vcu  buff{dst};
      dst = src;
      src = buff;
    } break;
    case SMLOAD: {
      auto  adr = th.cu[vreg(fetch(th))];
      auto& dst = th.simd[vreg(fetch(th))];

      if (adr >= th.pzdata) return panic (th, "segmentation fault", adr);

      auto buff = read_le<vsimdintegral>(th.pmem + adr);
      dst = *((vsimd*)&buff);
    } break;
    case SMWRITE: {
      auto  adr = th.cu[vreg(fetch(th))];
      auto& val = th.simd[vreg(fetch(th))];
      vsimdintegral integral = *((vsimdintegral*)&val);

      if (adr >= th.pzdata) return panic (th, "segmentation fault", adr);

      write_le<vsimdintegral>(th.pmem + adr, integral);
    } break;
    case SMMOV: {
      auto& src = th.simd[vreg(fetch(th))];
      auto& dst = th.simd[vreg(fetch(th))];
      dst = src;
    } break;
    case SMMOVX: {
      auto& src = th.simd[vreg(fetch(th))];
      auto& dst = th.simd[vreg(fetch(th))];
      dst = src;
      src = vsimd{};
    } break;
    case SMSWAP: {
      auto& src = th.simd[vreg(fetch(th))];
      auto& dst = th.simd[vreg(fetch(th))];
      vsimd  buff{dst};
      dst = src;
      src = buff;
    } break;
    case INT: {
      auto adr = th.cu[vreg(fetch(th))];
      
      if (adr >= th.lvclib) return panic (th, "virtual-c segmentation fault!", adr);
      th.vclib[adr](&th);
    } break;
    case HALT: th.flag = FLAG_STOP; break;
    case PANIC: return panic(th, "user panic!", th.cu[vreg(fetch(th))]); break;
    case BEEP: puts("<beep>"); break;
    default: return panic(th, "illegal opcode", opc);
  }
}

local void vloop (vthread& th) {
  while (th.flag != FLAG_STOP) {
    vdo (th, fetch(th));
  }
}

local std::thread vspawn (vthread vth) {
  return std::thread{
    [thread = vth]() mutable -> void { 
      vloop(thread); 
    }
  };
}

struct panicstruct {
  std::string message;
  vthread     thread;
  vcu         userdatum;
};

struct vthreaddata {
  vthread thread;
  std::vector<vcfun> funcs;
};

namespace {
  local std::vector<uint8_t>              memory;
  local std::vector<std::pair<vcu, vcu>>  vmemmap;
  local std::mutex                        memory_mtx;

  local vthread mainvth{};
  local std::thread watcherthead;
  
  local std::vector<vcfun> shared_vclib;
  local std::mutex         shared_vclib_mtx;

  local std::vector<std::pair<std::thread, vthreaddata>> vthreadpool;
  local std::mutex vthreadpool_mtx;

  local std::atomic<vcu> virtual_id = -1;
  local std::atomic<bool> main_exiting = false;
  local std::atomic<bool> watcher_exiting = false;
  local std::atomic<bool> panicking = false;
  local std::atomic<panicstruct*> current_panic{};
  local std::atomic<vcu> thread_ids = 0;

  local void finish_threadpool (void) {
    std::lock_guard<std::mutex> lock(vthreadpool_mtx);

    for (auto& thread: vthreadpool) {
      std::cout << "waiting for native v-thread " << thread.second.thread.id << std::endl;
      thread.first.join();
    }
  }

  local void watcher_thread_update (void) {
    bool even = false;

    while (true) {
      even = !even;
      std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

      if (panicking.load()) {
        std::cerr << std::format("[thread:{}] panicking!", virtual_id.load()) << std::endl;
        panicstruct* pan =  current_panic.load();
        std::cerr << std::format(
          "on sub-vthread {} relative to thread {}, '{}' ({})\n",
          pan->thread.id, virtual_id.load(), pan->message, pan->userdatum
        );

        finish_threadpool();
        return;
      }

      if (even) if (main_exiting.load()) return;
    }
  }
}

local void v_vm_spawn_vthread (vthread* vth) {
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
    if (segstart + seglen > pstart) pstart = segstart + seglen;
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
  vthreaddata data {
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

local auto spawn_mainthread (std::vector<uint8_t> &memory, std::vector<vcfun> funcs) {
  vthread mainvth{};
  
  mainvth.vclib = funcs.data();
  
}

void mix(do, varchname) () {

}

static_assert((((vregcount) & vregcount - 1) == 0 && vregcount >= 4), "vregcount should be a power of 2!");