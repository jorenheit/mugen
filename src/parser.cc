#include <fstream>
#include <map>
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

	size_t total_bits = 0;
    };

    struct RomSpecs {
        int word_count;
        int bits_per_word;
        size_t address_bits;
    };

    int lineNr = 0;

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
        lineNr = body.lineNr;
        while (std::getline(iss, ident)) {
	    trim(ident);
	    if (ident.empty()) {
		++lineNr;
		continue;
	    }

	    validateIdentifier(ident);
	    result.push_back(ident);
	    ++lineNr;
        }
        
        return result;
    }

    std::vector<Opcode> parseOpcodes(Body const &body) {

	auto constructOpcode = [](std::string const &lhs, std::string const &rhs) -> Opcode {
	    validateIdentifier(lhs);
	    int value;
	    error_if(!stringToInt(rhs, value, 16),
		     "value assigned to opcode ", rhs, " is not a valid hexadecimal number.");
	    return {lhs, static_cast<size_t>(value)};
	};

	
        std::istringstream iss(body.str);
        std::vector<Opcode> result;
        std::string line;
        lineNr = body.lineNr;

	while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++lineNr;
		continue;
	    }


	    std::vector<std::string> operands = split(line, '=');
	    error_if (operands.size() != 2,
		      "incorrect opcode format, should be of the form [OPCODE] = [HEX VALUE].");

	    Opcode oc = constructOpcode(operands[0], operands[1]);
	    result.push_back(oc);
	    ++lineNr;
        }

	return result;
    }



    AddressMapping parseAddressMapping(Body const &body) {
	
        std::istringstream iss(body.str);
        AddressMapping result;
        lineNr = body.lineNr;   
        std::string line;
        int count = 0;

	while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++lineNr;
		continue;
	    }

	    std::vector<std::string> operands = split(line, ':');
	    error_if(operands.size() != 2, "invalid format for address specifier, should be [IDENTIFIER]: [NUMBER OF BITS].");

	    std::string rhs = operands[1];
	    int bits;
	    error_if(!stringToInt(rhs, bits),
		     "specified number of bits (", rhs, ") is not a valid decimal number.");
	    error_if(bits <= 0, "number of bits must be a positive integer.");
                        
	    std::string const &ident = operands[0];
	    if (ident == "cycle") {
		error_if(result.cycle_bits > 0, "multiple definitions of cycle bits.");
		result.cycle_bits = bits;
		result.cycle_bits_start = count;
	    }
	    else if (ident == "opcode") {
		error_if(result.opcode_bits > 0, "multiple definitions of opcode bits.");
		result.opcode_bits = bits;
		result.opcode_bits_start = count;
	    }
	    else if (ident == "flags") {
		error_if(result.flag_bits > 0, "multiple definitions of flag bits.");
		result.flag_bits = bits;
		result.flag_bits_start = count;
	    }
	    else error("unknown identifier ", ident, ".");
                        
	    count += bits;
	    ++lineNr;
        }

	result.total_bits = count;
        return result;
    }


    RomSpecs parseRomSpecs(Body const &body) {

        std::istringstream iss(body.str);
        lineNr = body.lineNr;
        RomSpecs result;
        bool done = false;
        std::string line;

	while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++lineNr;
		continue;
	    }
	    error_if(done, "rom specification can only contain at most 1 non-empty line.");
                        
	    std::vector<std::string> values = split(line, 'x');
	    error_if(values.size() != 2, "invalid format for rom specification, should be [NUMBER OF WORDS]x[BITS_PER_WORD].");

	    // Get number of words
	    {
		error_if(!stringToInt(values[0], result.word_count),
			 "specified number of words (", values[0], ") is not a valid decimal number.");
		error_if(result.word_count <= 0, "specified number of words (", result.word_count, ") must be a positive integer.");
	    }
	    // Get bits per word
	    {
		error_if(!stringToInt(values[1], result.bits_per_word),
			 "specified number of bits per word (", values[1], ") is not a valid decimal number.");
		error_if(result.bits_per_word != 8, "only 8 bit words are currently supported.");
	    }
	    
	    done = true;
	    ++lineNr;
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

    std::map<std::string, Body> parseSections(std::istream &file) {

	enum State {
	    PARSING_TOP_LEVEL,
	    PARSING_SECTION_HEADER,
	    LOOKING_FOR_OPENING_BRACE,
	    PARSING_SECTION_BODY
        };
        
        State state = PARSING_TOP_LEVEL;
        lineNr = 1;
        std::map<std::string, Body> result;
        std::string currentSection;
        std::string currentBody;
        int bodyLineNr;
        bool isFirstCharacterOfBody;
        char ch;
	
        while (file.get(ch)) {
	    if (ch == '\n') ++lineNr;

	    switch (state) {
	    case PARSING_TOP_LEVEL: {
		if (ch == '[') state = PARSING_SECTION_HEADER;
		break; // ignore all other characters at top level
	    }
	    case PARSING_SECTION_HEADER: {
		error_if(ch == '{' || ch == '}',
			 "expected closing bracket ']' before '", ch, "' in section header.");

		if (ch == ']') state = LOOKING_FOR_OPENING_BRACE;
		else currentSection += ch;
		break;
	    }
	    case LOOKING_FOR_OPENING_BRACE: {
		if (std::isspace(ch)) break;
		error_if(ch != '{',
			 "expected opening brace '{' before '", ch, "' in section definition.");
		
		state = PARSING_SECTION_BODY;
		isFirstCharacterOfBody = true;
		break;
	    }
	    case PARSING_SECTION_BODY: {
		error_if(ch == '[',
			 "expected closing brace '}' before '", ch, "' in section definition.");
                        
		if (ch != '}') {
		    if (!std::isspace(ch) && isFirstCharacterOfBody) {
			bodyLineNr = lineNr;
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

    void findMatches(std::string &bits, std::vector<size_t> &matches, size_t idx = 0) {

        if (idx == bits.length()) {
	    matches.push_back(std::stoi(bits, nullptr, 2));
	    return;
        }
        
        char &c = bits[idx];
        switch (c) {
	case '0': 
	case '1': {
	    findMatches(bits, matches, idx + 1);
	    break;
	}
	case 'x': {
	    c = '0'; findMatches(bits, matches, idx + 1);
	    c = '1'; findMatches(bits, matches, idx + 1);
	    c = 'x';
	    break;
	}
	default: {
	    error("UNREACHABLE");
	}}
    }



    std::vector<std::vector<unsigned char>> parseMicrocode(Body const &body,        
							   RomSpecs const &rom,
							   std::vector<std::string> const &signals, 
							   std::vector<Opcode> const &opcodes,
							   AddressMapping const &mapping) {
        
        size_t nParts = (signals.size() + 7) / 8;
        std::vector<std::vector<unsigned char>> images;
        for (size_t part = 0; part != nParts; ++part) {
	    images.emplace_back(rom.word_count);
        }
        std::vector<bool> visited(rom.word_count);
        
        std::istringstream iss(body.str);
        lineNr = body.lineNr;
        std::string line;
        
        while (std::getline(iss, line)) {
	    trim(line);
	    if (line.empty()) {
		++lineNr;
		continue;
	    }
	    
	    std::vector<std::string> operands = split(line, '>', true);
	    //	    if (operands.size() == 1) operands.push_back("");
	    
	    std::vector<std::string> lhs = operands.size() ? split(operands[0], ':') : std::vector<std::string>{};
	    error_if(operands.size() != 2 || lhs.size() != 3, 
		     "invalid format in microcode definition, should be [OPCODE]:[CYCLE]:[FLAGS] > [SIG1], ...");
                        
	    // Build address string
	    std::string addressString(rom.address_bits, 'x');

	    // Lambda for inserting bits into the address-string
	    auto insertIntoAddressString = [&](std::string const &bitString, int bits_start, int n_bits) {
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
			    error_if(opcodeStr.length() > mapping.opcode_bits,
				     "value assigned to opcode ", userStr, " (", oc.value, ") does not fit inside ", mapping.opcode_bits, " bits.");
			    break;
			}
		    }
		    error_if(opcodeStr.empty(), "opcode ", userStr, " not declared in opcode section.");
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
			 "number of flag bits (", flagStr.length(), ") does not match number of flag bits defined in the address section (", mapping.flag_bits, ")");
                                
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
			 "signal ", signal, " not declared in signal section.");
	    }

	    // Assign bitvector to all matching indices
	    std::vector<size_t> matchedIndices;
	    findMatches(addressString, matchedIndices);
                        
	    for (size_t idx: matchedIndices) {
		if (!visited[idx]) {
		    for (size_t part = 0; part != nParts; ++part) {
			images[part][idx] = (bitvector >> (8 * part)) & 0xff;
		    }
		    visited[idx] = true;
		}
	    }
                
	    ++lineNr;
        }
        
        return images;                                                                                                                                  
    }

    std::vector<std::vector<unsigned char>> parse(std::string const &filename) {

	std::ifstream file(filename);
        error_if(!file, "could not open file ", filename, ".");
        
        bool romSpecsDefined = false;
        bool signalsDefined = false;
        bool opcodesDefined = false;
        bool addressMappingDefined = false;
        bool microcodeDefined = false;

        auto sections = parseSections(file);
        for (auto const &pr: sections) {
	    if (pr.first == "rom")            romSpecsDefined = true;
	    else if (pr.first == "signals")   signalsDefined = true;
	    else if (pr.first == "opcodes")   opcodesDefined = true;
	    else if (pr.first == "address")   addressMappingDefined = true;
	    else if (pr.first == "microcode") microcodeDefined = true;
	    else {
		lineNr = pr.second.lineNr;
		error("invalid section \"", pr.first, "\".");
	    }
        }

        lineNr = 0;
        error_if(!romSpecsDefined, "missing section: rom.");
        error_if(!signalsDefined, "missing section: signals.");
        error_if(!opcodesDefined, "missing section: opcodes.");
        error_if(!addressMappingDefined, "missing section: address.");
        error_if(!microcodeDefined, "missing section: microcode.");

        RomSpecs rom = parseRomSpecs(sections["rom"]);
        AddressMapping addressMapping = parseAddressMapping(sections["address"]);

	error_if(addressMapping.total_bits > rom.address_bits,
		 "Total number of bits used in address specification (", addressMapping.total_bits ,") exceeds number of address lines of the ROM (", rom.address_bits,").");
	
        std::vector<std::string> signals = parseSignals(sections["signals"]);
        std::vector<Opcode> opcodes = parseOpcodes(sections["opcodes"]);
	
        return parseMicrocode(sections["microcode"], rom, signals, opcodes, addressMapping);
    }

} // namespace Mugen
