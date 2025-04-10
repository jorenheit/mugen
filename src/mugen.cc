#include <cassert>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
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

    struct AddressMapping {
        size_t cycle_bits = 0;
        size_t cycle_bits_start = 0;
        
        size_t opcode_bits = 0;
        size_t opcode_bits_start = 0;
        
        size_t flag_bits = 0;
        size_t flag_bits_start = 0;

	size_t segment_bits = 0;
	size_t segment_bits_start = 0;

	std::vector<std::string> flag_labels;
    };

    struct RomSpecs {
	size_t rom_count;
        size_t word_count;
        size_t bits_per_word;
        size_t address_bits;
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

    std::vector<std::string> parseSignals(Body const &body, size_t romCount, size_t segmentBits) {
    
        std::istringstream iss(body.str);
	std::vector<std::string> result;
        std::string ident;
        _lineNr = body.lineNr;
        while (std::getline(iss, ident)) {
	    trim(ident);
	    if (ident.empty()) {
		++_lineNr;
		continue;
	    }

	    validateIdentifier(ident);
	    error_if(std::find(result.begin(), result.end(), ident) != result.end(),
		     "duplicate definition of signal \"", ident, "\".");

	    result.push_back(ident);
	    ++_lineNr;
        }

	error_if(result.size() > 64, "more than 64 signals declared.");

	size_t nChunk = 1 + result.size() / 8;
	size_t segmentBitsRequired = bitsNeeded(nChunk / romCount);

	bool warned = false;
	warning_if(nChunk < romCount,
		   "for ", result.size(), " signals, only ", nChunk, " roms are necessary to store all of them.");
	warning_if(nChunk == romCount && (segmentBits > 0) && (warned = true),
		   "for ", result.size(), " signals and ", romCount, " rom chips, using segmented roms is not necessary.");
	warning_if(segmentBitsRequired < segmentBits && !warned,
		   "for ", result.size(), " signals, it is sufficient to use only ", segmentBitsRequired," segment bit(s) (when using ", romCount, " ROM chips).");
	
	size_t partsAvailable = romCount * (1 << segmentBits);
        size_t nParts = (result.size() + 7) / 8;
	error_if(nParts > partsAvailable,
		 "too many signals declared (", result.size(),"). In this configuration (", romCount,
		 " rom chip(s), ", segmentBits, " segment bit(s)), a maximum of ", partsAvailable * 8, " signals can be declared.");
        
        return result;
    }

    std::unordered_map<std::string, size_t> parseOpcodes(Body const &body, int maxOpcodeBits) {

	auto constructOpcode = [](std::string const &lhs, std::string const &rhs) -> std::pair<std::string, size_t> {
	    validateIdentifier(lhs);
	    int value;
	    error_if(!stringToInt(rhs, value, 16),
		     "value assigned to opcode \"", rhs, "\" is not a valid hexadecimal number.");
	    return {lhs, static_cast<size_t>(value)};
	};

	
        std::istringstream iss(body.str);
	std::unordered_map<std::string, size_t> result;
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
	    error_if(value >= (1U << maxOpcodeBits),
		     "value assigned to opcode \"", ident, "\" (", value, ") does not fit inside ", maxOpcodeBits, " bits.");

	    bool success = result.insert({ident, value}).second;
	    error_if(!success,
		     "duplicate definition of opcode \"", ident, "\".");

	    for (auto const &[other, otherValue]: result) {
		if (other == ident) continue;
		warning_if(value == otherValue,
			   "signals \"", ident, "\" and \"", other, "\" are defined with the same value (", value, ").");
	    }
		     
	    ++_lineNr;
        }
	
	return result;
    }



    AddressMapping parseAddressMapping(Body const &body, int maxAddressBits) {
	
        std::istringstream iss(body.str);
        AddressMapping result;
        _lineNr = body.lineNr;   
        std::string line;
        int count = 0;

	while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++_lineNr;
		continue;
	    }

	    std::vector<std::string> operands = split(line, ':');
	    error_if(operands.size() != 2,
		     "invalid format for address specifier, should be <IDENTIFIER>: <NUMBER OF BITS>.");

	    std::string const &rhs = operands[1];
	    std::string const &ident = operands[0];

	    if (ident == "cycle") {
		error_if(result.cycle_bits > 0,
			 "multiple definitions of cycle bits.");
		error_if(!stringToInt(rhs, result.cycle_bits),
		     "specified number of bits (", rhs, ") is not a valid decimal number.");
		error_if(result.cycle_bits <= 0,
		     "number of bits must be a positive integer.");

		result.cycle_bits_start = count;
		count += result.cycle_bits;
	    }
	    else if (ident == "opcode") {
		error_if(result.opcode_bits > 0,
			 "multiple definitions of opcode bits.");
		error_if(!stringToInt(rhs, result.opcode_bits),
		     "specified number of bits (", rhs, ") is not a valid decimal number.");
		error_if(result.opcode_bits <= 0,
		     "number of bits must be a positive integer.");
		
		result.opcode_bits_start = count;
		count += result.opcode_bits;
	    }
	    else if (ident == "flags") {
		error_if(result.flag_bits > 0,
			 "multiple definitions of flag bits.");

		if (!stringToInt(rhs, result.flag_bits)) {
		    // Not a number -> interpret as labels
		    result.flag_labels = split(rhs, ',');
		    for (size_t idx = 0; idx != result.flag_labels.size(); ++idx) {
			validateIdentifier(result.flag_labels[idx]);
			for (size_t jdx = idx + 1; jdx != result.flag_labels.size(); ++jdx) {
			    if (idx == jdx) continue;
			    warning_if(result.flag_labels[idx] == result.flag_labels[jdx],
				     "duplicate flag \"", result.flag_labels[idx], "\".");
			}
		    }
		    
		    result.flag_bits = result.flag_labels.size();
		}
		
		error_if(result.flag_bits < 0,
			 "number of bits must be a positive integer or 0 if no flags are used.");
		
		result.flag_bits_start = count;
		count += result.flag_bits;
	    }
	    else if (ident == "segment") {
		error_if(result.segment_bits > 0,
			 "multiple definitions of segment bits.");
		error_if(!stringToInt(rhs, result.segment_bits),
		     "specified number of bits (", rhs, ") is not a valid decimal number.");
		error_if(result.segment_bits < 0,
			 "number of bits must be a positive integer or 0 if no segments are used.");

		result.segment_bits_start = count;
		count += result.segment_bits;
	    }     
	    else error("unknown identifier \"", ident, "\".");
                        
	    ++_lineNr;
        }


	error_if(count > maxAddressBits,
		 "Total number of bits used in address specification (", count ,") "
		 "exceeds number of address lines of the ROM (", maxAddressBits,").");

	// Check if mandatory fields have been set
	error_if(result.opcode_bits == 0,
		 "number of opcode bits must be specified.");
	error_if(result.cycle_bits == 0,
		 "number of cycle bits must be specified.");
	
        return result;
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


    std::vector<std::vector<unsigned char>> parseMicrocode(Body const &body,        
							   RomSpecs const &rom,
							   std::vector<std::string> const &signals,
							   std::unordered_map<std::string, size_t> const &opcodes,
							   AddressMapping const &mapping,
							   bool lsbFirst) {

        std::vector<std::vector<unsigned char>> result;
        for (size_t chip = 0; chip != rom.rom_count; ++chip) {
	    result.emplace_back(rom.word_count);
        }
        std::vector<size_t> visited(rom.word_count);
        
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


	    // Build address string
	    std::string addressString(rom.address_bits, 'x');

	    // Lambda for inserting bits into the address-string
	    auto insertIntoAddressString = [&addressString, &rom](std::string const &bitString, int bits_start, int n_bits) {
		addressString.replace(rom.address_bits - bits_start - n_bits, n_bits, bitString);
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
			    opcodeStr = toBinaryString(value, mapping.opcode_bits);
			    break;
			}
		    }
		    error_if(opcodeStr.empty(),
			     "opcode \"", userStr, "\" not declared in opcode section.");
		    insertIntoAddressString(opcodeStr, mapping.opcode_bits_start, mapping.opcode_bits);
		}

		// Insert cycle bits
		userStr = lhs[1];
		if (userStr != "x" && userStr != "X") {
		    std::string cycleStr;
		    int value;
		    error_if(!stringToInt(userStr, value),
			     "cycle number (", userStr, ") is not a valid decimal number.");

		    cycleStr = toBinaryString(value, mapping.cycle_bits);
		    error_if(cycleStr.length() > mapping.cycle_bits, 
			     "cycle number (", value, ") does not fit inside ", mapping.cycle_bits, " bits");
		    
		    insertIntoAddressString(cycleStr, mapping.cycle_bits_start, mapping.cycle_bits);
		}

		// Insert flag bits
		std::string flagStr = lhs[2];
		error_if(flagStr.length() != mapping.flag_bits, 
			 "number of flag bits (", flagStr.length(), ") does not match number of flag bits "
			 "defined in the address section (", mapping.flag_bits, ").");

		if (!flagStr.empty()) {
		    for (char c: flagStr) { 
			error_if(c != '0' && c != '1' && c != 'x' && c != 'X',
				 "invalid flag bit '", c,"'; can only be 0, 1 or x (wildcard).");
		    }

		    insertIntoAddressString(flagStr, mapping.flag_bits_start, mapping.flag_bits);
		}

		// Check if this is a catchall scenario after all
		for (char &c: addressString) {
		    if (c == 'X') c = 'x';
		}
		catchAll = (addressString == std::string(rom.address_bits, 'x'));
	    }
	    
	    // Construct control signal bitvector                   
	    std::vector<std::string> rhs = split(operands[1], ',');
	    size_t bitvector = 0;
	    for (std::string const &signal: rhs) {
		if (auto const it = std::find(signals.begin(), signals.end(), signal); it != signals.end()) {
		    bitvector |= (1 << std::distance(signals.begin(), it));
		}
		else error("signal \"", signal, "\" not declared in signal section.");
	    }


	    // Lambda that applies 'func' to each match of the address-string
	    auto for_each_match = [&addressString](auto const &func) -> decltype(func(0)) {
		auto impl = [&addressString, &func](auto const &self, size_t idx) -> decltype(func(0)) {
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
	    size_t nSegments = (1 << mapping.segment_bits);
	    for (size_t segment = 0; segment != nSegments; ++segment) {

		// add segment bits into the address string
		if (nSegments > 1) {
		    std::string segmentStr = toBinaryString(segment, mapping.segment_bits);
		    insertIntoAddressString(segmentStr, mapping.segment_bits_start, mapping.segment_bits);
		}

		// assign part of the bitvector to this segment (for each rom)
		for_each_match([&](int idx) {
		    if (!visited[idx]) {
			for (size_t chip = 0; chip != rom.rom_count; ++chip) {
			    int chunkIdx = segment * rom.rom_count + chip;
			    unsigned char byte = (bitvector >> (8 * chunkIdx)) & 0xff;
			    result[chip][idx] = (lsbFirst ? byte : reverseBits(byte));
			}
			visited[idx] = _lineNr;
		    }
		    else error_if(!catchAll,
				  "rule overlaps with rule previously defined on line ", visited[idx], ".");
		});
	    }
	    
	    ++_lineNr;
        }
        
        return result;                                                                                                                                  
    }


    std::string reportLayout(RomSpecs const &rom, AddressMapping const &address, std::vector<std::string> const &signals, bool lsbFirst) {
	std::ostringstream oss;
	size_t nSegments = (1 << address.segment_bits);
	for (size_t i = 0; i != rom.rom_count; ++i) {
	    for (size_t j = 0; j != nSegments; ++j) {
		size_t chunkIdx = 8 * (j * rom.rom_count + i);
		oss << "[ROM " << i << ", Segment " << j << "] {\n";
		for (size_t k = 0; k != 8; ++k) {
		    size_t signalIdx = chunkIdx + (lsbFirst ? k : (7 - k));
		    oss << "  " << k << ": " << (signalIdx < signals.size() ? signals[signalIdx] : "UNUSED") << '\n';
		}
		oss << "}\n\n";
	    }
	}

	std::vector<std::string> layout(rom.address_bits);
	for (size_t bit = 0; bit != address.opcode_bits; ++bit) {
	    layout[address.opcode_bits_start + bit] = "OPCODE BIT " + std::to_string(bit);
	}
	for (size_t bit = 0; bit != address.cycle_bits; ++bit) {
	    layout[address.cycle_bits_start + bit] = "CYCLE BIT " + std::to_string(bit);
	}
	for (size_t bit = 0; bit != address.flag_bits; ++bit) {
	    layout[address.flag_bits_start + bit] = (address.flag_labels.size() > 0) ?
		address.flag_labels[address.flag_labels.size() - bit - 1] :
		("FLAG BIT " + std::to_string(bit));
	}
	for (size_t bit = 0; bit != address.segment_bits; ++bit) {
	    layout[address.segment_bits_start + bit] = "SEGMENT BIT " + std::to_string(bit);
	}

	oss << "[Address Layout] {\n";
	for (size_t bit = 0; bit != rom.address_bits; ++bit) {
	    oss << "  " << bit << ": " << (layout[bit].empty() ? "UNUSED" : layout[bit]) << '\n';
	}
	oss << "}\n\n";
	
	return oss.str();
    }
    
    std::vector<std::vector<unsigned char>> parse(std::string const &filename, std::string &report, bool lsbFirst) {

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

        auto rom     = parseRomSpecs(sections["rom"]);
        auto address = parseAddressMapping(sections["address"], rom.address_bits);
        auto signals = parseSignals(sections["signals"], rom.rom_count, address.segment_bits);
        auto opcodes = parseOpcodes(sections["opcodes"], address.opcode_bits);

	report = reportLayout(rom, address, signals, lsbFirst);
	
        return parseMicrocode(sections["microcode"], rom, signals, opcodes, address, lsbFirst);
    }

} // namespace Mugen
