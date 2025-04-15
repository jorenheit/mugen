#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>

#include "linenoise/linenoise.h"
#include "mugen.h"
#include "util.h"

namespace Mugen {
  
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
    
#include "command_line.h"
  CommandLine generateCommandLine(std::string const &outFileBase, std::vector<bool> &state, Result const &result);

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
    CommandLine cli = generateCommandLine(outFileBase, state, result);

    // Start interactive session -> return true/false to indicate if the images should be writen to disk
    std::cout << "<Mugen Debug> Type \"help\" for a list of available commands.\n\n";
    while (true) {
      auto [input, good] = promptAndGetInput();
      if (!good) return false;

      auto args = split(input, ' ');
      if (args.empty()) continue;

      auto [quit, write] = cli.exec(args);
      if (quit) return write;
    }
    UNREACHABLE;
  }
  
  CommandLine generateCommandLine(std::string const &outFileBase, std::vector<bool> &state, Result const &result) {
  
    CommandLine cli;
    
    #define COMMAND [&](CommandLine::CommandArgs const &args)
      cli.add({"help", "h"}, COMMAND {
        if (args.size() == 1)
          cli.printHelp();
        else if (args.size() > 2) {
          debug_error(args[0], "command expects at most 1 argument.");
        }
        else cli.printHelp(args[1]);
      },
      "Display this text."
    );
    
    cli.add({"info", "i"}, COMMAND {
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
        }
        else printInfo(result, outFileBase);
      },
      "Display image information."
    );
    
    cli.add({"flags", "f"}, COMMAND {
        if (args.size() > 1) {
          debug_error(args[0], "command does not expect any arguments.");
        }
        else printState(state, result);
      },
      "Display current flag-state."
    );
    
    cli.add({"set", "s"}, COMMAND {
        if (args.size() < 2) {
          debug_error(args[0], "command expects at least 1 flag name, index or \"*\".");
        }
        else if (setOrReset(args, true, state, result))
          printState(state, result);
      },
      "Set a flag to true.",
      
      "  This command accepts one or more flags, seperated by a space.\n"
      "  The flags can be names (if the specification file uses named flags) or indices: (0 - #flag-bits).\n" 
      "  Alternatively, a \'*\' can be used to set all flags at once.\n"
      "  \n"
      "  Examples:\n"
      "    set Z\n"
      "    set Z C\n"
      "    set 0 1 2\n"
      "    set *\n"
    );
      
    cli.add({"reset", "r"}, COMMAND {
        if (args.size() < 2) {
          debug_error(args[0], "command expects at least 1 flag name, index or \"*\".");
        }
        else if (setOrReset(args, false, state, result))
          printState(state, result);
      },
      "Reset a flag to 0",
      
      "  This command resets the given flags to 0 in the same way \"set\" sets flags.\n"
      "  See \"help set\" for more details.\n"
    );      
    
    cli.add({"run", "exec", "x"}, COMMAND {
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
      },
      "Run an opcode.",
      
      "  This command simulates running an opcode in the current state (see set/reset).\n"
      "  The opcode is passed as its first argument: \"run ADD\".\n"
      "  When no additional argument is passed, all available cycles (limited by the number of cycle bits)\n"
      "  will be handled. Alternatively, a second argument can be provided to limit this number.\n"
      "  For example, to simulate the ADD opcode for 2 cycles:\n"
      "     run ADD 2\n"
    );
    
    cli.add({"signals", "S"}, COMMAND {
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
        }
        else printSignals(result); 
      },
      "Display the list of signals."
    );
    
    cli.add({"opcodes", "o"}, COMMAND {
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
        }
        else printOpcodes(result);
      },
      "Display the list of opcodes and their values."
    );
    
    cli.add({"layout", "l"}, COMMAND {
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
        }
        else std::cout << result.layout;
      },
      "Display the memory layout of the images."
    );
    
    cli.add({"write", "w"}, COMMAND {        
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
          return false;
        }
        return true;
      },
      "Write the results to disk."
    );
    
    cli.add({"exit", "quit", "q"}, COMMAND {
        if (args.size() != 1) {
          debug_error(args[0], "command does not expect any arguments.");
          return false;
        }
        return false;
      },
      "Exit without writing the results to disk."
    );
    #undef COMMAND
    
    return cli;
  }

  
} // namespace Mugen
