#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <unordered_map>

namespace Mugen {

  struct RomSpecs {
    size_t rom_count;
    size_t word_count;
    size_t bits_per_word;
    size_t address_bits;
  };
    
  struct AddressMapping {
    size_t cycle_bits = 0;
    size_t cycle_bits_start = 0;
        
    size_t opcode_bits = 0;
    size_t opcode_bits_start = 0;
        
    size_t flag_bits = 0;
    size_t flag_bits_start = 0;

    size_t segment_bits = 0;
    size_t segment_bits_start = 0;

    size_t total_address_bits = 0;
    std::vector<std::string> flag_labels;
  };

  using Opcodes = std::unordered_map<std::string, size_t>;
  using Signals = std::vector<std::string>;
  using Image = std::vector<unsigned char>;

  struct Options {
    enum class Padding {
      NONE,
      VALUE,
      CATCH
    };
        
    bool printLayout = false;
    bool lsbFirst = true;
    Padding padImages = Padding::NONE;
    unsigned char padValue = 0;
  };
    
  struct Result {
    std::vector<Image> images;
    std::string layout;

    Opcodes opcodes;
    AddressMapping address;
    Signals signals;
    RomSpecs rom;
  };

  Result generate(std::string const &specFile, Options const &opt);
  bool debug(std::string const &specFile, std::string const &outFileBase, Result const &result);
}

#endif
