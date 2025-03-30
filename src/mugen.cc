#include <cassert>
#include <fstream>
#include <unordered_map>
#include <sstream>
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

    struct AddressMapping {
        size_t cycle_bits = 0;
        size_t cycle_bits_start = 0;
        
        size_t opcode_bits = 0;
        size_t opcode_bits_start = 0;
        
        size_t flag_bits = 0;
        size_t flag_bits_start = 0;
    };

    struct RomSpecs {
        int word_count;
        int bits_per_word;
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

    
    void validateIdentifier(std::string const &ident) {
	for (char c : ident) {
	    error_if(std::isspace(c),
		     "Identifier \"", ident, "\" can not contain whitespace.");
	    error_if(!std::isalnum(c) && c != '_',
		     "Identifier \"", ident, "\" contains invalid character: '", c, "'.");
	}
	error_if(ident == "x", "\"x\" may not be used as an identifier.");
    }
    
    std::vector<std::string> parseSignals(Body const &body) {
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
	    result.push_back(ident);
	    ++_lineNr;
        }
        
        return result;
    }

    std::vector<Opcode> parseOpcodes(Body const &body, int maxOpcodeBits) {

	auto constructOpcode = [](std::string const &lhs, std::string const &rhs) -> Opcode {
	    validateIdentifier(lhs);
	    int value;
	    error_if(!stringToInt(rhs, value, 16),
		     "value assigned to opcode \"", rhs, "\" is not a valid hexadecimal number.");
	    return {lhs, static_cast<size_t>(value)};
	};

	
        std::istringstream iss(body.str);
        std::vector<Opcode> result;
        std::string line;
        _lineNr = body.lineNr;

	while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++_lineNr;
		continue;
	    }


	    std::vector<std::string> operands = split(line, '=');
	    error_if(operands.size() != 2,
		     "incorrect opcode format, should be of the form [OPCODE] = [HEX VALUE].");

	    Opcode oc = constructOpcode(operands[0], operands[1]);
	    error_if(oc.value >= (1U << maxOpcodeBits),
		     "value assigned to opcode \"", oc.ident, "\" (", oc.value, ") does not fit inside ", maxOpcodeBits, " bits.");

	    result.push_back(oc);
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
		     "invalid format for address specifier, should be [IDENTIFIER]: [NUMBER OF BITS].");

	    std::string rhs = operands[1];
	    int bits;
	    error_if(!stringToInt(rhs, bits),
		     "specified number of bits (", rhs, ") is not a valid decimal number.");
	    error_if(bits <= 0,
		     "number of bits must be a positive integer.");

	    
	    std::string const &ident = operands[0];
	    if (ident == "cycle") {
		error_if(result.cycle_bits > 0,
			 "multiple definitions of cycle bits.");
		
		result.cycle_bits = bits;
		result.cycle_bits_start = count;
	    }
	    else if (ident == "opcode") {
		error_if(result.opcode_bits > 0,
			 "multiple definitions of opcode bits.");
		
		result.opcode_bits = bits;
		result.opcode_bits_start = count;
	    }
	    else if (ident == "flags") {
		error_if(result.flag_bits > 0,
			 "multiple definitions of flag bits.");
		
		result.flag_bits = bits;
		result.flag_bits_start = count;
	    }
	    else error("unknown identifier ", ident, ".");
                        
	    count += bits;
	    ++_lineNr;
        }


	error_if(count > maxAddressBits,
		 "Total number of bits used in address specification (", count ,") "
		 "exceeds number of address lines of the ROM (", maxAddressBits,").");

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
	    error_if(values.size() != 2,
		     "invalid format for rom specification, should be [NUMBER OF WORDS]x[BITS_PER_WORD].");

	    // Get number of words
	    {
		error_if(!stringToInt(values[0], result.word_count),
			 "specified number of words (", values[0], ") is not a valid decimal number.");
		error_if(result.word_count <= 0,
			 "specified number of words (", result.word_count, ") must be a positive integer.");
	    }
	    // Get bits per word
	    {
		error_if(!stringToInt(values[1], result.bits_per_word),
			 "specified number of bits per word (", values[1], ") is not a valid decimal number.");
		error_if(result.bits_per_word != 8,
			 "only 8 bit words are currently supported.");
	    }
	    
	    done = true;
	    ++_lineNr;
        }
        
        int n = result.word_count;
        int count = 0;
        while (n != 0) {
	    ++count;
	    n >>= 1;
        }
        result.address_bits = count - 1;
        
        return result;
    }

    std::unordered_map<std::string, Body> parseTopLevel(std::istream &file) {

	enum State {
	    PARSING_TOP_LEVEL,
	    PARSING_SECTION_HEADER,
	    LOOKING_FOR_OPENING_BRACE,
	    PARSING_SECTION_BODY
        };
        
        State state = PARSING_TOP_LEVEL;
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
		break; // ignore all other characters at top level
	    }
	    case PARSING_SECTION_HEADER: {
		error_if(ch == '{' || ch == '}',
			 "expected ']' before '", ch, "' in section header.");

		if (ch == ']') state = LOOKING_FOR_OPENING_BRACE;
		else currentSection += ch;
		break;
	    }
	    case LOOKING_FOR_OPENING_BRACE: {
		if (std::isspace(ch)) break;
		error_if(ch != '{',
			 "expected '{' before '", ch, "' in section definition.");
		
		state = PARSING_SECTION_BODY;
		isFirstCharacterOfBody = true;
		break;
	    }
	    case PARSING_SECTION_BODY: {
		error_if(ch == '[',
			 "expected '}' before '", ch, "' in section definition.");
                        
		if (ch != '}') {
		    if (!std::isspace(ch) && isFirstCharacterOfBody) {
			bodyLineNr = _lineNr;
			isFirstCharacterOfBody = false;
		    }
		    currentBody += ch;
		}
		else {
		    trim(currentSection);
		    trim(currentBody);
		    result[currentSection] = {currentBody, bodyLineNr};
		    currentSection.clear();
		    currentBody.clear();
		    state = PARSING_TOP_LEVEL;
		}
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
							   std::vector<Opcode> const &opcodes,
							   AddressMapping const &mapping) {
        
        size_t nParts = (signals.size() + 7) / 8;
        std::vector<std::vector<unsigned char>> result;
        for (size_t part = 0; part != nParts; ++part) {
	    result.emplace_back(rom.word_count);
        }
        std::vector<bool> visited(rom.word_count);
        
        std::istringstream iss(body.str);
        _lineNr = body.lineNr;
        std::string line;
        
        while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++_lineNr;
		continue;
	    }
	    
	    std::vector<std::string> operands = split(line, '>', true);
	    std::vector<std::string> lhs = operands.size() ? split(operands[0], ':') : std::vector<std::string>{};
	    error_if(operands.size() != 2 || lhs.size() != 3, 
		     "invalid format in microcode definition, should be [OPCODE]:[CYCLE]:[FLAGS] > [SIG1], ...");
                        
	    // Build address string
	    std::string addressString(rom.address_bits, 'x');

	    // Lambda for inserting bits into the address-string
	    auto insertIntoAddressString = [&addressString, &rom](std::string const &bitString, int bits_start, int n_bits) {
		addressString.replace(rom.address_bits - bits_start - n_bits, n_bits, bitString);
	    };
	    
	    // Insert opcode into address string
	    {
		std::string userStr = lhs[0];
		if (userStr != "x") {
		    std::string opcodeStr;
		    for (Opcode const &oc: opcodes) {
			if (userStr == oc.ident) {
			    opcodeStr = toBinaryString(oc.value, mapping.opcode_bits);
			    break;
			}
		    }
		    error_if(opcodeStr.empty(),
			     "opcode ", userStr, " not declared in opcode section.");
		    insertIntoAddressString(opcodeStr, mapping.opcode_bits_start, mapping.opcode_bits);
		}
	    }
                        
	    // Insert cycle into address string
	    {
		std::string userStr = lhs[1];
		if (userStr != "x") {
		    std::string cycleStr;
		    int value;
		    error_if(!stringToInt(userStr, value),
			     "cycle number (", userStr, ") is not a valid decimal number.");

		    cycleStr = toBinaryString(value, mapping.cycle_bits);
		    error_if(cycleStr.length() > mapping.cycle_bits, 
			     "cycle number (", value, ") does not fit inside ", mapping.cycle_bits, " bits");
		    
		    insertIntoAddressString(cycleStr, mapping.cycle_bits_start, mapping.cycle_bits);
		}

	    }
                        
	    // Insert flag bits into address string
	    {
		std::string flagStr = lhs[2];
		error_if(flagStr.length() != mapping.flag_bits, 
			 "number of flag bits (", flagStr.length(), ") does not match number of flag bits "
			 "defined in the address section (", mapping.flag_bits, ")");
                                
		for (char c: flagStr) { 
		    error_if(c != '0' && c != '1' && c != 'x',
			     "invalid flag bit '", c,"'; can only be 0, 1 or x (wildcard).");
		}

		insertIntoAddressString(flagStr, mapping.flag_bits_start, mapping.flag_bits);
	    }
                        
	    // Construct control signal bitvector                   
	    std::vector<std::string> rhs = split(operands[1], ',');
	    size_t bitvector = 0;
	    for (std::string const &signal: rhs) {
		bool match = false;
		for (size_t idx = 0; idx != signals.size(); ++idx) {
		    if (signal == signals[idx]) {
			bitvector |= (1 << idx);
			match = true;
			break;
		    }
		}
		error_if(!match,
			 "signal \"", signal, "\" not declared in signal section.");
	    }


	    // Lambda that applies 'func' to each match of the address-string
	    auto for_each_match = [](std::string &bits, auto const &func) -> decltype(func(0)) {
		auto impl = [&bits, &func](auto const &self, size_t idx) -> decltype(func(0)) {
		    if (idx == bits.length()) return func(std::stoi(bits, nullptr, 2));
        
		    char &c = bits[idx];
		    if (c == '0' || c == '1') {
			self(self, idx + 1);
		    }
		    else if (c == 'x') {
			c = '0'; self(self, idx + 1);
			c = '1'; self(self, idx + 1);
			c = 'x';
		    }
		    else assert(false && "UNREACHABLE");
		};
		
		return impl(impl, 0);
	    };

	    // Find all matching indices and assign the bitvectors to the images.
	    for_each_match(addressString, [&](int idx) {
		if (!visited[idx]) {
		    for (size_t part = 0; part != nParts; ++part) {
			result[part][idx] = (bitvector >> (8 * part)) & 0xff;
		    }
		    visited[idx] = true;
		}
	    });
	    
	    ++_lineNr;
        }
        
        return result;                                                                                                                                  
    }

    std::vector<std::vector<unsigned char>> parse(std::string const &filename) {

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
		error("invalid section \"", name, "\".");
	    }
	}

	_lineNr = 0;
	for (auto const &[name, defined]: requiredSections) {
	    error_if(!defined,
		     "missing section: \"", name, "\".");
	}

        auto rom     = parseRomSpecs(sections["rom"]);
        auto address = parseAddressMapping(sections["address"], rom.address_bits);
        auto signals = parseSignals(sections["signals"]);
        auto opcodes = parseOpcodes(sections["opcodes"], address.opcode_bits);
	
        return parseMicrocode(sections["microcode"], rom, signals, opcodes, address);
    }

} // namespace Mugen
