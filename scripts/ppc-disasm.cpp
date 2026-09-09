// Inspect a range in the SDK-loaded image using its Xenon-aware disassembler.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "ppc/disasm.h"

int main(int argc, char** argv) {
  try {
    if (argc != 5)
      throw std::runtime_error("Usage: ppc-disasm IMAGE IMAGE_BASE START SIZE (hex accepted)");
    const uint64_t base = std::stoull(argv[2], nullptr, 0);
    const uint64_t start = std::stoull(argv[3], nullptr, 0);
    const uint64_t size = std::stoull(argv[4], nullptr, 0);
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open image");
    std::vector<char> data{std::istreambuf_iterator<char>(file), {}};
    if (start < base || start - base > data.size() ||
        size > data.size() - (start - base) || start % 4 || size % 4)
      throw std::runtime_error("Range is outside image or unaligned");
    rex::codegen::ppc::DisassemblerEngine engine{BFD_ENDIAN_BIG, nullptr};
    engine.info.buffer = reinterpret_cast<bfd_byte*>(data.data());
    engine.info.buffer_vma = base;
    engine.info.buffer_length = data.size();
    for (uint64_t pc = start; pc < start + size; pc += 4) {
      const auto* p = reinterpret_cast<const unsigned char*>(data.data() + pc - base);
      std::printf("%08llX  %02X%02X%02X%02X  ",
                  static_cast<unsigned long long>(pc), p[0], p[1], p[2], p[3]);
      print_insn_big_powerpc(pc, &engine.info);
      std::puts("");
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
