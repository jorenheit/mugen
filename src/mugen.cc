#include <cassert>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include "linenoise/linenoise.h"
#include "mugen.h"
#include "util.h"

#define UNREACHABLE assert(false && "UNREACHABLE");

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
  
    // Debug mode
    template <typename ... Args>
    void debug_error(std::string const &cmd, Args const & ... args) {
        std::cout << "Invalid use of \"" << cmd << "\": ";
        (std::cout << ... << args) << '\n';
        std::cout << "Type \"help\" for more information.\n";
    }
  
    void printState(std::vector<bool> const &state, Result const &result) {
        std::ostringstream labelLine;
        std::ostringstream valueLine;
        std::ostringstream delimLine;
    
        labelLine << "  |";
        valueLine << "  |";
        delimLine << "  +";
        for (size_t idx = 0; idx != state.size(); ++idx) {
            std::string label = result.address.flag_labels.empty() ?
                " FLAG " + std::to_string(result.address.flag_bits - idx - 1) + " ":
                " " + result.address.flag_labels[idx] + " ";
      
            std::string value(label.length(), ' ');
            value.replace(label.length() / 2, 1, std::to_string(state[result.address.flag_bits - idx - 1]));
      
            labelLine << label << "|";
            valueLine << value << "|";
            delimLine << std::string(label.length(), '-') << '+';
        }
    
        std::cout << delimLine.str() << '\n'
                  << labelLine.str() << '\n'
                  << delimLine.str() << '\n'
                  << valueLine.str() << '\n'
                  << delimLine.str() << '\n';
    
    }
  
    bool setOrReset(std::vector<std::string> const &args, bool const value, std::vector<bool> &state, Result const &result) {
        for (size_t idx = 1; idx != args.size(); ++idx) {
            std::string const &flag  = args[idx];
            if (flag == "*") {
                for (auto &&val: state) val = value;
                return true;
            }
      
            size_t flagBit = -1;
            if (!stringToInt(flag, flagBit)) {
                if (result.address.flag_labels.empty()) {
                    debug_error(args[0], "Specification file does not specify flag names, "
                                "so its must be a bit-indices (0 - ", result.address.flag_bits, ") or \"*\".");
                    return false;
                }
                for (size_t idx = 0; idx != result.address.flag_bits; ++idx) {
                    if (result.address.flag_labels[idx] == flag) {
                        flagBit = result.address.flag_bits - idx - 1;
                        break;
                    }
                }
            }
            if (flagBit == static_cast<size_t>(-1) || flagBit >= result.address.flag_bits) {
                debug_error(args[0], "Invalid flag \"", flag, "\".");
                return false;
            }
      
            state[flagBit] = value;
        }
    
        return true;
    }    
  
  
    void runOpcode(std::string const &opcode, size_t maxCycles, std::vector<bool> const &state, Result const &result) {
        if (!result.opcodes.contains(opcode)) {
            std::cout << "Opcode \"" << opcode << "\" not specified in specification file.\n";
            return;
        }
    
        // Build address string
        std::string addressString(result.address.total_address_bits, '0');
        auto insertIntoAddressString = [&addressString](std::string const &bitString, int bits_start) {
            addressString.replace(addressString.length() - bits_start - bitString.length(), bitString.length(), bitString);
        };
    
        // Insert opcode bits
        std::string opcodeBits = toBinaryString(result.opcodes.find(opcode)->second, result.address.opcode_bits);
        insertIntoAddressString(opcodeBits, result.address.opcode_bits_start);
    
        // Insert flag bits
        std::string flagBits(result.address.flag_bits, ' ');
        for (size_t idx = 0; idx != result.address.flag_bits; ++idx)
            flagBits[flagBits.length() - idx - 1] = state[idx] ? '1' : '0';
        insertIntoAddressString(flagBits, result.address.flag_bits_start);
    
        // Iterate over cycles and collect signals on every cycle
        for (size_t cycle = 0; cycle != maxCycles; ++cycle) {
            Signals activeSignals;
      
            // Insert cycle bits
            std::string cycleBits = toBinaryString(cycle, result.address.cycle_bits);
            insertIntoAddressString(cycleBits, result.address.cycle_bits_start);
            size_t const address = std::stoi(addressString, nullptr, 2);
      
            // Iterate over segments
            size_t nSegments = (1 << result.address.segment_bits);
            for (size_t segment = 0; segment != nSegments; ++segment) {
        
                // Insert segment bits (if segmented)
                if (nSegments > 1) {
                    std::string segmentStr = toBinaryString(segment, result.address.segment_bits);
                    insertIntoAddressString(segmentStr, result.address.segment_bits_start);
                }
        
                // Iterate over roms and fetch signals
                for (size_t romIndex = 0; romIndex != result.rom.rom_count; ++romIndex) {
                    unsigned char word = result.images[romIndex][address];
                    for (size_t bit = 0; bit != result.rom.bits_per_word; ++bit) {
                        if (word & (1 << bit)) {
                            size_t signalIndex = (segment * result.rom.rom_count + romIndex) * result.rom.bits_per_word + bit;
                            activeSignals.push_back(result.signals[signalIndex]);
                        }
                    }
                }
            }
      
            // Print list of signals active on this cycle
            std::cout << "  " << cycle << ": ";
            for (std::string const &signal: activeSignals) {
                std::cout << signal << (signal != activeSignals.back() ? ", " : "");
        
            }
            std::cout << '\n';
        }
    }
  
    void printOpcodes(Result const &result) {
        std::vector<std::string> sorted(1 << result.address.opcode_bits);
        size_t maxWidth = 0;
        for (auto const &[str, value]: result.opcodes) {
            sorted[value] = str;
            if (str.length() > maxWidth) maxWidth = str.length();
        }
        for (size_t value = 0; value != sorted.size(); ++value) {
            if (sorted[value].empty()) continue;
            std::cout << std::setw(maxWidth + 2) << std::setfill(' ') << sorted[value]
                      << " = 0x" << std::setw(2) << std::setfill('0') << std::hex << value << '\n';
        }
    }
  
    void printSignals(Result const &result) {
        for (auto const &signal: result.signals) {
            std::cout << "  " << signal << '\n';
        }
    }
  
    void printInfo(Result const &result, std::string const &outFileBase) {
    
        auto property = [](std::string const &str) -> std::ostream& {
            return (std::cout << std::setw(15) << std::setfill(' ') << str << ": ");
        };
    
        size_t nImages = result.images.size();
        property("#images") << nImages << " -> ";
        for (size_t idx = 0; idx != nImages; ++idx) {
            std::cout << outFileBase;
            if (nImages > 1) {
                std::cout << ("." + std::to_string(idx));
                if (idx != nImages - 1) {
                    std::cout << ", ";
                }
            }
        }
        std::cout << "\n";
    
        property("image size") << result.images[0].size() << " bytes (";
        if (result.images[0].size() <= (1UL << result.address.total_address_bits)) {
            std::cout << "not ";
        }
        std::cout << "padded)\n"; 
    
        property("segmented") << (result.address.segment_bits > 0 ? "yes" : "no");
        if (result.address.segment_bits > 0)
            std::cout << ", " << (1 << result.address.segment_bits) << " segments per image.";
        std::cout << '\n';
    
        property("#signals") << result.signals.size() << '\n';
        property("#opcodes") << result.opcodes.size() << '\n';
    }
  
    void printHelp() {
        std::cout << "Available commands:\n"
            "  help   (h) - Display this text.\n"
            "  info   (i) - Display image information.\n"
            "  opcodes    - Display a list of opcodes and their values.\n"
            "  signals    - Display a list of signals.\n"
            "  layout     - Display the memory layout (signal allocation and address breakdown).\n"
            "  flags  (f) - Display the current flag-state.\n"
            "  set    (s) - Set one or multiple flags (by name or index, seperated by a space).\n"
            "               Examples: \"set C\", \"set 0 1\", \"set *\"\n"
            "  reset  (r) - Reset one or multiple flags (same syntax as \"set\").\n"
            "  run        - Display activated signals on subsequent cycles when a certain opcode\n"
            "               is run when the processor is in the current state.\n" 
            "               Example: \"run ADD\"\n"
            "               An optional second argument can be provided to specify the number of cycles.\n"
            "               If this argument is omitted, all possible cycles will be handled.\n"
            "               Example: \"run ADD 4\"\n"
            "  write  (w) - Write the results to disk and quit.\n"
            "  exit   (q) - Quit the program without writing the results to disk.\n";
    }
    
#include "command_line.h"
    bool debug(std::string const &specFile, std::string const &outFileBase, Result const &result) {
    
        // Construct prompt and helper function (lambda) that wraps linenoise
        std::string const prompt = "[" + specFile + "]$ ";
        auto promptAndGetInput = [&prompt]() -> std::pair<std::string, bool> {
            char *line = linenoise(prompt.c_str());
            if (line == nullptr) return {"", false};
            
            linenoiseHistoryAdd(line);
            std::string input(line);
            linenoiseFree(line);
            return {input, true};
        };
    
        // Initialize state vector (flags)
        std::vector<bool> state(result.address.flag_bits);

        // Create commands
        CommandLine cli;
        goto create_cli;

    run_cli:
        // Start interactive session -> return true/false to indicate if the images should be writen to disk
        std::cout << "<Mugen Debug> Type \"help\" for a list of available commands.\n\n";
        while (true) {
            // Get user input and split into parts
            auto [input, good] = promptAndGetInput();
            if (!good) break;
            auto args = split(input, ' ');
            if (args.empty()) continue;
            auto [quit, write] = cli.exec(args);
            if (quit) return write;
        }
        return false;

    create_cli:
#define COMMAND [&](CommandLine::CommandArgs const &args)
        cli.add({"help", "h"}, COMMAND {
                printHelp();
            });
    
        cli.add({"info", "i"}, COMMAND {
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                }
                else printInfo(result, outFileBase);
            });
    
        cli.add({"flags", "f"}, COMMAND {
                if (args.size() > 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                }
                else printState(state, result);
            });
    
        cli.add({"set", "reset", "s", "r"}, COMMAND {
                if (args.size() < 2) {
                    debug_error(args[0], "command expects at least 1 flag name, index or \"*\".");
                }
                else if (setOrReset(args, (args[0] == "set" || args[0] == "s"), state, result))
                    printState(state, result);
            });
    
        cli.add({"run"}, COMMAND {
                if (args.size() < 2) {
                    debug_error(args[0], "command expects at least one argument (run <opcode>).");
                    return;
                }
                else if (args.size() > 3) {
                    debug_error(args[0], "command expects at most two arguments (run <opcode> <cycles>).");
                    return;
                }
      
                size_t const maxCycles = (1UL << result.address.cycle_bits);
                size_t runCycles = maxCycles;
                if (args.size() == 3) {
                    if (!stringToInt(args[2], runCycles)) {
                        debug_error(args[0], "cycle number \"", args[2], "\" is not a number.");
                        return;
                    }
                    if (runCycles > maxCycles) {
                        debug_error(args[0], "cycle number (", runCycles, ") exceeds the maximum number of allowed cycles (", maxCycles, ").");
                        return;
                    }
                }
                runOpcode(args[1], runCycles, state, result);
            });
    
        cli.add({"signals"}, COMMAND {
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                }
                else printSignals(result); 
            });
    
        cli.add({"opcodes"}, COMMAND {
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                }
                else printOpcodes(result);
            });
    
        cli.add({"layout"}, COMMAND {
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                }
                else std::cout << result.layout;
            });
    
        cli.add({"write", "w"}, COMMAND -> CommandLine::CommandReturn {        
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                    return {};
                }
                return {true, true};
            });
    
        cli.add({"exit", "quit", "q"}, COMMAND -> CommandLine::CommandReturn {
                if (args.size() != 1) {
                    debug_error(args[0], "command does not expect any arguments.");
                    return {};
                }
                return {true, false};
            });
#undef COMMAND
        goto run_cli;
    }
  
} // namespace Mugen
