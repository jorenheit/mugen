#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

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

    Opcodes opcodes;
    AddressMapping address;
    Signals signals;
    RomSpecs rom;
    bool lsbFirst;

    std::string specificationFilename;
  };

  Result generate(std::string const &specFile, Options const &opt);
  std::string layoutReport(Result const &result);
  bool debug(Result const &result, std::string const &outFile);

  struct WriteResult {
    bool success = false;
    std::string report;
  };
  
  class Writer {
  protected:
    std::string const _filename;
  public:
    Writer(std::string const &file):
      _filename(file)
    {}
    
    static std::unique_ptr<Writer> get(std::string const &filename);
    virtual WriteResult write(Result const &result) = 0;
    virtual std::vector<std::string> extensions() const = 0;
  };

  struct BinaryFileWriter: public Writer {
    using Writer::Writer;
    virtual WriteResult write(Result const &result) override;
    virtual std::vector<std::string> extensions() const override;
  };

  struct CPPWriter: public Writer {
    using Writer::Writer;
    virtual WriteResult write(Result const &result) override;
    virtual std::vector<std::string> extensions() const override;
  };

  using Writers = std::tuple<BinaryFileWriter, CPPWriter>;
}

#endif
