#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <sstream>

#include "linenoise/linenoise.h"
#include "mugen.h"
#include "util.h"

namespace Mugen {
  
  struct Body {
    std::string str;
    int lineNr;
  };
  
  struct Opcode {
    std::string ident;
    size_t value;
  };
  
  int _lineNr = 0;
  std::string _file;
  
  template <typename ... Args>
  void error(Args ... args) {
    (std::cerr << Mugen::_file << ":" << Mugen::_lineNr << ": ERROR: " <<  ... << args) << '\n';
    std::exit(1);
  }
  
  template <typename ... Args>
  void error_if(bool condition, Args ... args) {
    if (condition) error(args...);
  }
  
  template <typename ... Args>
  void warning(Args ... args) {
    (std::cerr << _file << ":" << _lineNr << ": WARNING: " <<  ... << args) << '\n';
  }
  
  template <typename ... Args>
  void warning_if(bool condition, Args ... args) {
    if (condition) warning(args...);
  }
  
  
  void validateIdentifier(std::string const &ident) {
    error_if(!isalpha(ident[0]) && ident[0] != '_',
             "Identifier \"", ident, "\" does not start with a letter or underscore.");
    
    for (char c : ident) {
      error_if(std::isspace(c),
               "Identifier \"", ident, "\" can not contain whitespace.");
      error_if(!std::isalnum(c) && c != '_',
               "Identifier \"", ident, "\" contains invalid character: '", c, "'.");
    }
    
    error_if(ident == "x" || ident == "X", "\"x\" and \"X\" may not be used as identifiers.");
  }
  
  Signals parseSignals(Body const &body, Result const &result) {
    
    std::istringstream iss(body.str);
    Signals signals;
    std::string ident;
    _lineNr = body.lineNr;
    while (std::getline(iss, ident)) {
      trim(ident);
      if (ident.empty()) {
        ++_lineNr;
        continue;
      }
      
      validateIdentifier(ident);
      error_if(std::find(signals.begin(), signals.end(), ident) != signals.end(),
               "duplicate definition of signal \"", ident, "\".");
      
      signals.push_back(ident);
      ++_lineNr;
    }
    
    error_if(signals.size() > 64, "more than 64 signals declared.");
    
    size_t const romCount = result.rom.rom_count;
    size_t const segmentBits = result.address.segment_bits;
    size_t const nChunk = 1 + signals.size() / 8;
    size_t const segmentBitsRequired = bitsNeeded(nChunk / romCount);
    
    bool warned = false;
    warning_if(nChunk < romCount,
               "for ", signals.size(), " signals, only ", nChunk, " roms are necessary to store all of them.");
    warning_if(nChunk == romCount && (segmentBits > 0) && (warned = true),
               "for ", signals.size(), " signals and ", romCount, " rom chips, using segmented roms is not necessary.");
    warning_if(segmentBitsRequired < segmentBits && !warned,
               "for ", signals.size(), " signals, it is sufficient to use only ", segmentBitsRequired," segment bit(s) (when using ", romCount, " ROM chips).");
    
    size_t partsAvailable = romCount * (1 << segmentBits);
    size_t nParts = (signals.size() + 7) / 8;
    error_if(nParts > partsAvailable,
             "too many signals declared (", signals.size(),"). In this configuration (", romCount,
             " rom chip(s), ", segmentBits, " segment bit(s)), a maximum of ", partsAvailable * 8, " signals can be declared.");
    
    return signals;
  }
  
  Opcodes parseOpcodes(Body const &body, Result const &result) {
    
    auto constructOpcode = [](std::string const &lhs, std::string const &rhs) -> std::pair<std::string, size_t> {
      validateIdentifier(lhs);
      int value;
      error_if(!stringToInt(rhs, value, 16),
               "value assigned to opcode \"", rhs, "\" is not a valid hexadecimal number.");
      return {lhs, static_cast<size_t>(value)};
    };
    
    
    std::istringstream iss(body.str);
    Opcodes opcodes;
    std::string line;
    _lineNr = body.lineNr;
    
    while (std::getline(iss, line)) {
      trim(line);
      if (line.empty()) {
        ++_lineNr;
        continue;
      }
      
      
      std::vector<std::string> operands = split(line, '=');
      error_if(operands.size() == 1,
               "expected \"=\" in opcode definition.");
            
      error_if(operands.size() != 2,
               "incorrect opcode format, should be of the form <OPCODE> = <HEX VALUE>.");
      
      auto [ident, value] = constructOpcode(operands[0], operands[1]);
      size_t const maxOpcodeBits = result.address.opcode_bits;
      error_if(value >= (1U << maxOpcodeBits),
               "value assigned to opcode \"", ident, "\" (", value, ") does not fit inside ", maxOpcodeBits, " bits.");
      
      bool success = opcodes.insert({ident, value}).second;
      error_if(!success,
               "duplicate definition of opcode \"", ident, "\".");
      
      for (auto const &[other, otherValue]: opcodes) {
        if (other == ident) continue;
        warning_if(value == otherValue,
                   "signals \"", ident, "\" and \"", other, "\" are defined with the same value (", value, ").");
      }
      
      ++_lineNr;
    }
    
    return opcodes;
  }
  
  
  
  AddressMapping parseAddressMapping(Body const &body, Result const &result) {
    
    std::istringstream iss(body.str);
    AddressMapping address;
    _lineNr = body.lineNr;   
    std::string line;
    size_t count = 0;
    
    while (std::getline(iss, line)) {
      trim(line);
      if (line.empty()) {
        ++_lineNr;
        continue;
      }
      
      std::vector<std::string> operands = split(line, ':');
      error_if(operands.size() != 2,
               "invalid format for address specifier, should be <IDENTIFIER>: <NUMBER OF BITS>.");
      
      std::string const &ident = operands[0];
      std::string const &rhs = operands[1];
      
      auto parseField = [&ident, &rhs, &count](size_t &bits, size_t &start, int minValue, auto &&parseRhs){
        error_if(bits > 0,
                 "multiple definitions of \"", ident, "\" bits.");
        
        int result;
        error_if(!parseRhs(rhs, result),
                 "right hand side of \"", ident, "\" (", rhs, ") is not valid. "
                 "Should be either a number or a list of identifiers (when specifying the flag bits)."); 
        error_if(result < minValue,
                 "number of bits must be a positive integer.");
        
        bits = result;
        start = count;
        count += bits;
        
      };
            
      if (ident == "cycle") {
        parseField(address.cycle_bits, address.cycle_bits_start, 1, [](auto& ... args) {
          return stringToInt(args...);
        });
      }
      else if (ident == "opcode") {
        parseField(address.opcode_bits, address.opcode_bits_start, 1, [](auto& ... args) {
          return stringToInt(args...);
        });
      }
      else if (ident == "flags") {
        parseField(address.flag_bits, address.flag_bits_start, 0, [&address](std::string const &str, int &bits) {
          if (stringToInt(str, bits)) return true;
          
          // Not a number -> interpret as labels
          address.flag_labels = split(str, ',');
          std::unordered_set<std::string> set;
          for (std::string &label: address.flag_labels) {
            warning_if(!set.insert(label).second,
                       "duplicate flag \"", label, "\".");
          }
          
          bits = address.flag_labels.size();
          return true;
        });
      }
      else if (ident == "segment") {
        parseField(address.segment_bits, address.segment_bits_start, 0, [](auto& ... args) {
          return stringToInt(args...);
        });
      }
      else error("unknown address field \"", ident, "\".");
      
      ++_lineNr;
    }
    
    
    error_if(count > result.rom.address_bits,
             "Total number of bits used in address specification (", count ,") "
             "exceeds number of address lines of the ROM (", result.rom.address_bits, ").");
    
    // Check if mandatory fields have been set
    error_if(address.opcode_bits == 0,
             "number of opcode bits must be specified.");
    error_if(address.cycle_bits == 0,
             "number of cycle bits must be specified.");
    
    address.total_address_bits = count;
    return address;
  }
  
  
  RomSpecs parseRomSpecs(Body const &body) {
    
    std::istringstream iss(body.str);
    _lineNr = body.lineNr;
    RomSpecs result;
    bool done = false;
    std::string line;
    
    while (std::getline(iss, line)) {
      trim(line);
      if (line.empty()) {
        ++_lineNr;
        continue;
      }
      error_if(done,
               "rom specification can only contain at most 1 non-empty line.");
      
      std::vector<std::string> values = split(line, 'x');
      error_if(values.size() < 2 || values.size() > 3,
               "invalid format for rom specification, should be <NUMBER OF WORDS> x <BITS_PER_WORD> "
               "or <NUMBER OF WORDS> x <BITS_PER_WORD> x <NUMBER_OF_CHIPS>.");
      
      // Get number of words
      error_if(!stringToInt(values[0], result.word_count),
               "specified number of words (", values[0], ") is not a valid decimal number.");
      error_if(result.word_count <= 0,
               "specified number of words (", result.word_count, ") must be a positive integer.");
      
      // Get bits per word
      error_if(!stringToInt(values[1], result.bits_per_word),
               "specified number of bits per word (", values[1], ") is not a valid decimal number.");
      error_if(result.bits_per_word != 8,
               "only 8 bit words are currently supported.");
      
      // Get number of roms
      if (values.size() == 3) { 
        error_if(!stringToInt(values[2], result.rom_count),
                 "specified number rom chips (", values[2], ") is not a valid decimal number.");
        error_if(result.rom_count <= 0,
                 "Number of rom chips (", result.rom_count, ") must be a positive integer.");
      }
      else result.rom_count = 1;
            
      done = true;
      ++_lineNr;
    }
    
    result.address_bits = bitsNeeded(result.word_count);
    return result;
  }
  
  std::unordered_map<std::string, Body> parseTopLevel(std::istream &file) {
    
    enum State {
      PARSING_TOP_LEVEL,
      PARSING_SECTION_HEADER,
      LOOKING_FOR_OPENING_BRACE,
      PARSING_SECTION_BODY,
      PARSING_COMMENT
    };
    
    State state = PARSING_TOP_LEVEL;
    State stateBeforeComment = state;
    
    _lineNr = 1;
    std::unordered_map<std::string, Body> result;
    std::string currentSection;
    std::string currentBody;
    int bodyLineNr;
    bool isFirstCharacterOfBody;
    char ch;
    
    while (file.get(ch)) {
      if (ch == '\n') ++_lineNr;
      
      switch (state) {
      case PARSING_TOP_LEVEL: {
        if (ch == '[') state = PARSING_SECTION_HEADER;
        else if (ch == '#') {
          stateBeforeComment = state;
          state = PARSING_COMMENT;
        }
        else if (!std::isspace(ch)){
          error("only comments (use #) may appear outside sections.");
        }
        break;
      }
      case PARSING_SECTION_HEADER: {
        error_if(ch == '{' || ch == '}',
                 "expected ']' before '", ch, "' in section header.");
        error_if(ch == '#',
                 "cannot place comments inside a section header.");
          
        if (ch == ']') state = LOOKING_FOR_OPENING_BRACE;
        else currentSection += ch;
        break;
      }
      case LOOKING_FOR_OPENING_BRACE: {
        if (std::isspace(ch)) break;
        if (ch == '#') {
          stateBeforeComment = state;
          state = PARSING_COMMENT;
        }
        else {
          error_if(ch != '{',
                   "expected '{' before '", ch, "' in section definition.");
          state = PARSING_SECTION_BODY;
          isFirstCharacterOfBody = true;
        }
        break;
      }
      case PARSING_SECTION_BODY: {
        error_if(ch == '[',
                 "expected '}' before '", ch, "' in section definition.");
          
        if (ch != '}') {
          if (ch == '#') {
            stateBeforeComment = state;
            state = PARSING_COMMENT;
            break;
          }
          else if (!std::isspace(ch) && isFirstCharacterOfBody) {
            bodyLineNr = _lineNr;
            isFirstCharacterOfBody = false;
          }
          currentBody += ch;
          break;
        }
        else {
          trim(currentSection);
          trim(currentBody);
          error_if(!result.insert({currentSection, {currentBody, bodyLineNr}}).second,
                   "multiple definitions of section \"", currentSection, "\".");
            
          currentSection.clear();
          currentBody.clear();
          state = PARSING_TOP_LEVEL;
          break;
        }
        UNREACHABLE;
      }
      case PARSING_COMMENT: {
        if (ch == '\n') {
          state = stateBeforeComment;
          if (state == PARSING_SECTION_BODY) { 
            currentBody += '\n';
          }
        }
        break;
      }}
    }
    
    // After parsing the entire file, state should be back to TOP_LEVEL
    
    error_if(state == PARSING_SECTION_HEADER,
             "expected closing bracket ']' in section header.");
    
    error_if(state == LOOKING_FOR_OPENING_BRACE,
             "expected opening brace '{' in section definition.");
    
    error_if(state == PARSING_SECTION_BODY,
             "expecting closing brace '}' in section definition.");
    
    return result;
  }
  
  
  std::vector<Image> parseMicrocode(Body const &body, Result const &result, Options const &opt) {
    
    auto const &rom = result.rom;
    auto const &address = result.address;
    auto const &signals = result.signals;
    auto const &opcodes = result.opcodes;
    
    size_t const addressBits = (opt.padImages == Options::Padding::CATCH)
      ? rom.address_bits
      : address.total_address_bits;
    
    std::vector<Image> images;
    size_t imageSize = (1 << addressBits);
    for (size_t chip = 0; chip != rom.rom_count; ++chip) 
      images.emplace_back(imageSize);
    
    std::vector<size_t> visited(imageSize);
    std::vector<bool> signalsUsed(signals.size());
    auto opcodesCopy = opcodes;
    bool catchRuleDefined = false;
    
    std::istringstream iss(body.str);
    _lineNr = body.lineNr;
    std::string line;
    
    while (std::getline(iss, line)) {
      trim(line);
      if (line.empty()) {
        ++_lineNr;
        continue;
      }
            
      std::vector<std::string> operands = split(line, "->", true);
      error_if(operands.size() == 1,
               "expected \"->\" in microcode rule.");
      error_if(operands.size() != 2,
               "invalid format in microcode definition, should be (<OPCODE>:<CYCLE>:<FLAGS> | catch) -> [SIG1], ...");
      
      
      // Build address string, fill with wildcards
      std::string addressString(addressBits, 'x');
      
      // Lambda for inserting bits into the address-string
      auto insertIntoAddressString = [&addressString](std::string const &bitString, int bits_start) {
        addressString.replace(addressString.length() - bits_start - bitString.length(), bitString.length(), bitString);
      };
      
      
      bool catchAll = (operands[0] == "catch");
      if (!catchAll) {
        std::vector<std::string> lhs = split(operands[0], ':');
        error_if(!catchAll && (lhs.size() < 2 || lhs.size() > 3),
                 "expected ':' before '->' in rule definition.");
        
        if (lhs.size() == 2) lhs.push_back("");
        
        // Insert opcode bits
        std::string userStr = lhs[0];
        if (userStr != "x" && userStr != "X") {
          std::string opcodeStr;
          for (auto const &[ident, value]: opcodes) {
            if (userStr == ident) {
              opcodesCopy.erase(ident);
              opcodeStr = toBinaryString(value, address.opcode_bits);
              break;
            }
          }
          error_if(opcodeStr.empty(),
                   "opcode \"", userStr, "\" not declared in opcode section.");
          insertIntoAddressString(opcodeStr, address.opcode_bits_start);
        }
        
        // Insert cycle bits
        userStr = lhs[1];
        if (userStr != "x" && userStr != "X") {
          std::string cycleStr;
          int value;
          error_if(!stringToInt(userStr, value),
                   "cycle number (", userStr, ") is not a valid decimal number.");
          
          cycleStr = toBinaryString(value, address.cycle_bits);
          error_if(cycleStr.length() > address.cycle_bits, 
                   "cycle number (", value, ") does not fit inside ", address.cycle_bits, " bits");
          
          insertIntoAddressString(cycleStr, address.cycle_bits_start);
        }
        
        // Insert flag bits
        std::string flagStr = lhs[2];
        error_if(flagStr.length() != address.flag_bits, 
                 "number of flag bits (", flagStr.length(), ") does not match number of flag bits "
                 "defined in the address section (", address.flag_bits, ").");
        
        if (!flagStr.empty()) {
          for (char c: flagStr) { 
            error_if(c != '0' && c != '1' && c != 'x' && c != 'X',
                     "invalid flag bit '", c,"'; can only be 0, 1 or x (wildcard).");
          }
          
          insertIntoAddressString(flagStr, address.flag_bits_start);
        }
        
        // Check if this is a catchall scenario after all
        for (char &c: addressString) {
          if (c == 'X') c = 'x';
        }
        catchAll = (addressString == std::string(rom.address_bits, 'x'));
      }
      if (catchAll) catchRuleDefined = true;
            
      // Construct control signal bitvector                   
      std::vector<std::string> rhs = split(operands[1], ',');
      size_t bitvector = 0;
      for (std::string const &signal: rhs) {
        bool validSignal = false;
        for (size_t idx = 0; idx != signals.size(); ++idx) {
          if (signals[idx] != signal) continue;
          
          bitvector |= (1 << idx);
          signalsUsed[idx] = true;
          validSignal = true;
          break;
        }
        
        error_if(!validSignal,
                 "signal \"", signal, "\" not declared in signal section.");
      }
      
      
      // Lambda that applies 'func' to each match of the address-string
      auto for_each_match = [&addressString](auto const &func) {
        auto impl = [&addressString, &func](auto const &self, size_t idx) {
          if (idx == addressString.length()) return func(std::stoi(addressString, nullptr, 2));          
          char &c = addressString[idx];
          if (c == '0' || c == '1') {
            self(self, idx + 1);
          }
          else if (c == 'x' || c == 'X') {
            c = '0'; self(self, idx + 1);
            c = '1'; self(self, idx + 1);
            c = 'x';
          }
          else UNREACHABLE;
        };
        
        return impl(impl, 0);
      };
      
      // Find all matching indices and assign the bitvectors to the images.
      size_t nSegments = (1 << address.segment_bits);
      for (size_t segment = 0; segment != nSegments; ++segment) {
        
        // If segmented, add segment bits into the address string
        if (nSegments > 1) {
          std::string segmentStr = toBinaryString(segment, address.segment_bits);
          insertIntoAddressString(segmentStr, address.segment_bits_start);
        }
        
        // Assign part of the bitvector to this segment (for each rom)
        for_each_match([&](int idx) {
          if (visited[idx]) {
            error_if(!catchAll,
                     "rule overlaps with rule previously defined on line ", visited[idx], ".");
            return;
          }
          
          for (size_t chip = 0; chip != rom.rom_count; ++chip) {
            int chunkIdx = segment * rom.rom_count + chip;
            unsigned char byte = (bitvector >> (8 * chunkIdx)) & 0xff;
            images[chip][idx] = (opt.lsbFirst ? byte : reverseBits(byte));
          }
          visited[idx] = _lineNr;
        });
      }
            
      ++_lineNr;
    }
    
    // Raise a warning on unused opcodes
    for (auto &pair: opcodesCopy)
      warning("unused opcode \"", pair.first, "\".");
    
    // Raise a warning on unused signals
    for (size_t idx = 0; idx != signals.size(); ++idx) 
      warning_if(!signalsUsed[idx],
                 "unused signal \"", signals[idx], "\".");
    
    // Raise a warning when padding with catch is enabled but no catch rule was defined
    error_if(!catchRuleDefined && opt.padImages == Options::Padding::CATCH,
             "no catch rule defined. This is mandatory when using '--pad catch'.");
    
    return images;                                                                                                                                  
  }
  
  
  void padImages(Result &result, unsigned char padValue) {
    size_t padSize = (result.rom.word_count - result.images[0].size());
    std::vector<unsigned char> const padVector(padSize, padValue);
    
    for (auto &image: result.images) {
      image.insert(image.end(), padVector.begin(), padVector.end());
    }
  }
  
  std::string layoutReport(Result const &result, Options const &opt) {
    
    std::ostringstream oss;
    size_t nSegments = (1 << result.address.segment_bits);
    for (size_t i = 0; i != result.rom.rom_count; ++i) {
      for (size_t j = 0; j != nSegments; ++j) {
        size_t chunkIdx = 8 * (j * result.rom.rom_count + i);
        oss << "[ROM " << i << ", Segment " << j << "] {\n";
        for (size_t k = 0; k != 8; ++k) {
          size_t signalIdx = chunkIdx + (opt.lsbFirst ? k : (7 - k));
          oss << "  " << k << ": " << (signalIdx < result.signals.size() ? result.signals[signalIdx] : "UNUSED") << '\n';
        }
        oss << "}\n\n";
      }
    }
    
    std::vector<std::string> layout(result.rom.address_bits);
    for (size_t bit = 0; bit != result.address.opcode_bits; ++bit) {
      layout[result.address.opcode_bits_start + bit] = "OPCODE BIT " + std::to_string(bit);
    }
    for (size_t bit = 0; bit != result.address.cycle_bits; ++bit) {
      layout[result.address.cycle_bits_start + bit] = "CYCLE BIT " + std::to_string(bit);
    }
    for (size_t bit = 0; bit != result.address.flag_bits; ++bit) {
      layout[result.address.flag_bits_start + bit] = (result.address.flag_labels.size() > 0) ?
        result.address.flag_labels[result.address.flag_labels.size() - bit - 1] :
        ("FLAG BIT " + std::to_string(bit));
    }
    for (size_t bit = 0; bit != result.address.segment_bits; ++bit) {
      layout[result.address.segment_bits_start + bit] = "SEGMENT BIT " + std::to_string(bit);
    }
    
    oss << "[Address Layout] {\n";
    for (size_t bit = 0; bit != result.rom.address_bits; ++bit) {
      oss << "  " << bit << ": " << (layout[bit].empty() ? "UNUSED" : layout[bit]) << '\n';
    }
    oss << "}\n";
    
    return oss.str();
  }
  
  
  
  Result generate(std::string const &filename, Options const &opt) {
    
    std::ifstream file(filename);
    error_if(!file,
             "could not open file \"", filename, "\".");
    
    _file = filename;
    std::unordered_map<std::string, bool> requiredSections{
      {"rom", false},
      {"signals", false},
      {"opcodes", false},
      {"address", false},
      {"microcode", false}
    };
    
    auto sections = parseTopLevel(file);
    for (auto const &[name, body]: sections) {
      if (requiredSections.contains(name)) {
        requiredSections[name] = true;
      }
      else {
        _lineNr = body.lineNr;
        warning("ignoring unknown section \"", name, "\".");
      }
    }
    
    _lineNr = 0;
    for (auto const &[name, defined]: requiredSections) {
      error_if(!defined,
               "missing section: \"", name, "\".");
    }
    
    Result result;
    result.rom     = parseRomSpecs(sections["rom"]);
    result.address = parseAddressMapping(sections["address"], result);
    result.signals = parseSignals(sections["signals"], result);
    result.opcodes = parseOpcodes(sections["opcodes"], result);
    result.layout  = layoutReport(result, opt);
    result.images  = parseMicrocode(sections["microcode"], result, opt);
    
    if (opt.padImages == Options::Padding::VALUE) padImages(result, opt.padValue);
    return result;
  }
  
} // namespace Mugen
