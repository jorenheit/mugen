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
    };

    struct RomSpecs {
        int word_count;
        int bits_per_word;
        int address_bits;
    };

    int lineNr = 0;

    std::vector<std::string> parseSignals(Body const &body) {
        std::istringstream iss(body.str);
        std::vector<std::string> result;
        std::string line;
        lineNr = body.lineNr;
        while (std::getline(iss, line)) {
	    trim(line);
	    for (char c: line) {
		error_if(std::isspace(c), "signal identifier may not contain spaces.");
	    }
	    if (!line.empty()) result.push_back(line);
	    ++lineNr;
        }
        
        return result;
    }



    Opcode constructOpcode(std::vector<std::string> const &operands, int const lineNr) {
        error_if (operands.size() != 2, "incorrect opcode format, should be of the form [OPCODE] = [HEX VALUE].");
        for (char c: operands[0]) {
	    error_if(std::isspace(c), "opcode identifier may not contain spaces.");
        }
        
        size_t value = 0;
        try {
	    value = std::stoi(operands[1], nullptr, 16);
        }
        catch (...) {
	    error_if(true, "value assigned to opcode ", operands[1], " is not a valid hexadecimal number.");
        }
        
        return {operands[0], value};
    }

    std::vector<Opcode> parseOpcodes(Body const &body) {
        std::istringstream iss(body.str);
        std::vector<Opcode> result;
        std::string line;
        lineNr = body.lineNr;
        while (std::getline(iss, line)) {
	    trim(line);
	    if (!line.empty()) {
		result.push_back(constructOpcode(split(line, '='), lineNr));
	    }
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
	    if (!line.empty()) {
		auto operands = split(line, ':');
		error_if(operands.size() != 2, "invalid format for address specifier, should be [IDENTIFIER]: [NUMBER OF BITS].");
                        
		int bits = 0;
		try {
		    bits = std::stoi(operands[1]);
		}
		catch (...) {
		    error_if(true, "specified number of bits (", operands[1], ") is not a valid decimal number.");
		}
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
		else {
		    error_if(true, "unknown identifier ", ident, ".");
		}
                        
		count += bits;
	    }
	    ++lineNr;
        }
        
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
	    if (!line.empty()) {
		error_if(done, "rom specification can only contain one line.");
                        
		std::vector<std::string> values = split(line, 'x');
		error_if(values.size() != 2, "invalid format for rom specification, should be [NUMBER OF WORDS]x[BITS_PER_WORD].");
                
		try {
		    result.word_count = std::stoi(values[0]);
		}
		catch (...) {
		    error_if(true, "specified number of words (", values[0], ") is not a valid decimal number.");
		}
		error_if(result.word_count <= 0, "specified number of words (", result.word_count, ") must be a positive integer.");
                        
		try {
		    result.bits_per_word = std::stoi(values[1]);
		}
		catch (...) {
		    error_if(true, "specified number of bits per word (", values[1], ") is not a valid decimal number.");
		}
		error_if(result.bits_per_word != 8, "only 8 bit words are currently supported.");
		done = true;
	    }
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
	    if (ch == '\n') {
		++lineNr;
	    }
                
	    if (state == PARSING_TOP_LEVEL) {
		if (ch == '[') {
		    state = PARSING_SECTION_HEADER;
		}
	    }
	    else if (state == PARSING_SECTION_HEADER) {
		error_if(ch == '{' || ch == '}', "expected closing bracket ']' before '", ch, "' in section header.");
		if (ch == ']') {
		    state = LOOKING_FOR_OPENING_BRACE;
		}
		else {
		    currentSection += ch;
		}
	    }
	    else if (state == LOOKING_FOR_OPENING_BRACE) {
		if (std::isspace(ch)) continue;
		error_if(ch != '{', "expected opening brace '{' before '", ch, "' in section definition.");
		state = PARSING_SECTION_BODY;
		isFirstCharacterOfBody = true;
	    }
	    else if (state == PARSING_SECTION_BODY) {
		error_if(ch == '[', "expected closing brace '}' before '", ch, "' in section definition.");
                        
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
	    }
        }
        
        error_if(state == PARSING_SECTION_HEADER, "expected closing bracket ']' in section header.");
        error_if(state == LOOKING_FOR_OPENING_BRACE, "expected opening brace '{' in section definition.");
        error_if(state == PARSING_SECTION_BODY, "expecting closing brace '}' in section definition.");

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
	    // UNREACHABLE
	}
        }
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
	    if (!line.empty()) {
		std::vector<std::string> operands = split(line, '>');
		if (operands.size() == 1) { operands.push_back(""); }
		std::vector<std::string> lhs = operands.size() ? split(operands[0], ':') : std::vector<std::string>{};
                        
		error_if(operands.size() != 2 || lhs.size() != 3, 
			 "invalid format in microcode definition, should be [OPCODE]:[CYCLE]:[FLAGS] > [SIG1], ...");
                        
		// Build address string
		std::string addressString(rom.address_bits, 'x');
                        
		// Insert opcode into address string
		{
		    std::string userStr = lhs[0];
		    std::string opcodeStr;
		    if (userStr == "x") {
			opcodeStr = std::string(mapping.opcode_bits, 'x');
		    }
		    else {
			for (Opcode const &oc: opcodes) {
			    if (userStr == oc.ident) {
				opcodeStr = toBinaryString(oc.value, mapping.opcode_bits);
				error_if(opcodeStr.length() > mapping.opcode_bits,
					 "value assigned to opcode ", userStr, " (", oc.value, ") does not fit inside ", mapping.opcode_bits, " bits.");
				break;
			    }
			}
		    }
		    error_if(opcodeStr.empty(), "opcode ", userStr, " not declared in opcode section.");
		    int pos = rom.address_bits - mapping.opcode_bits_start - mapping.opcode_bits;
		    int count = mapping.opcode_bits;
		    addressString.replace(pos, count, opcodeStr);
		}
                        
		// Insert cycle into address string
		{
		    std::string userStr = lhs[1];
		    std::string cycleStr;
		    if (userStr == "x") {
			cycleStr = std::string(mapping.cycle_bits, 'x');
		    }
		    else {
			int value;
			try {
			    value = std::stoi(userStr);
			}
			catch (...) {
			    error_if(true, "cycle number (", userStr, ") is not a valid decimal number.");
			}
			cycleStr = toBinaryString(value, mapping.cycle_bits);
			error_if(cycleStr.length() > mapping.cycle_bits, 
				 "cycle number (", value, ") does not fit inside ", mapping.cycle_bits, " bits");
		    }
                                
		    int pos = rom.address_bits - mapping.cycle_bits_start - mapping.cycle_bits;
		    int count = mapping.cycle_bits;
		    addressString.replace(pos, count, cycleStr);
		}
                        
		// Insert flag bits into address string
		{
		    std::string flagStr = lhs[2];
		    error_if(flagStr.length() != mapping.flag_bits, 
			     "number of flag bits (", flagStr.length(), ") does not match number of flag bits defined in the address section (", mapping.flag_bits, ")");
                                
		    for (char c: flagStr) { 
			error_if(c != '0' && c != '1' && c != 'x', "invalid flag bit '", c,"'; can only be 0, 1 or x (wildcard).");
		    }
                                
		    int pos = rom.address_bits - mapping.flag_bits_start - mapping.flag_bits;
		    int count = mapping.flag_bits;
		    addressString.replace(pos, count, flagStr);
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
		    error_if(!match, "signal ", signal, " not declared in signal section.");
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
		error_if(true, "invalid section \"", pr.first, "\".");
	    }
        }

        lineNr = 0;
        error_if(!romSpecsDefined, "missing section: rom.");
        error_if(!signalsDefined, "missing section: signals.");
        error_if(!opcodesDefined, "missing section: opcodes.");
        error_if(!addressMappingDefined, "missing section: address.");
        error_if(!microcodeDefined, "missing section: microcode.");
        
        RomSpecs rom = parseRomSpecs(sections["rom"]);
        std::vector<std::string> signals = parseSignals(sections["signals"]);
        std::vector<Opcode> opcodes = parseOpcodes(sections["opcodes"]);
        AddressMapping addressMapping = parseAddressMapping(sections["address"]);
        return parseMicrocode(sections["microcode"], rom, signals, opcodes, addressMapping);
    }

} // namespace Mugen
