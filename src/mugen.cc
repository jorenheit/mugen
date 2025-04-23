#include <iostream>
#include <fstream>
#include "mugen.h"
#include "util.h"

int printHelp(std::string const &progName, int ret = 0) {
  std::cout << "Usage: " << progName << " <specification-file (.mu)> <output-file> [OPTIONS]\n\n"
	    << "Supported output-file extensions:\n"
	    << "  .bin, .rom           -> Generate binary file.\n"
	    << "  .c, .cpp, .cc, .cxx  -> Generate C/C++ source file\n"
	    << "\n"
            << "Options:\n"
            << "  -h, --help       Display this help message and exit\n"
            << "  -l, --layout     Print the ROM layout report after generation\n"
            << "  -m, --msb-first  Store signals starting from the most significant bit.\n"
            << "  -p, --pad VALUE  Pad the remainder of the rom with the supplied value (may be hex).\n"
            << "  -p, --pad catch  Pad the remainder of the rom with the signals specified in the catch-rule.\n"
            << "  -d, --debug      Run Mugen in an interactive debug mode. Type \"help\" for more information.\n"
            << "\nExample:\n"
            << "  " << progName << " myspec.mu microcode.bin --pad catch --msb-first --layout\n"
            << "See https://github.com/jorenheit/mugen for more help.\n";
  
  return ret;
}

int main(int argc, char **argv) {
  
  if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    return printHelp(argv[0]);
  }
  if (argc < 3) {
    std::cerr << "ERROR: Invalid number of arguments.\n\n";
    return printHelp(argv[0], 1);
  }
  
  bool debugMode = false;
  Mugen::Options opt;
  
  for (int idx = 3; idx != argc; ++idx) {
    std::string flag = argv[idx];
    if (flag == "-l" || flag == "--layout") opt.printLayout = true;
    else if (flag == "-m" || flag == "--msb-first") opt.lsbFirst = false;
    else if (flag == "-p" || flag == "--pad") {
      if (idx == argc - 1) {
        std::cerr << "ERROR: no argument to --pad (-p) option.\n\n";
        return printHelp(argv[0], 1);
      }
      int value = 0;
      idx += 1;
      if (argv[idx] == std::string("catch")) {
        opt.padImages = Mugen::Options::Padding::CATCH;
        continue;
      }
      if (!stringToInt(argv[idx], value, 16)) {
        std::cerr << "ERROR: argument passed to --pad (-p) must be a hex value or \"catch\".\n\n";
        return printHelp(argv[0], 1);
      }
      if (value > 0xff) {
        std::cerr << "ERROR: hex value passed to --pad (-p) exceeds 8 bits.\n\n";
        return printHelp(argv[0], 1);
      }
      
      opt.padImages = Mugen::Options::Padding::VALUE;
      opt.padValue = value;
    }
    else if (flag == "-d" || flag == "--debug") debugMode = true;
    else if (flag == "-h" || flag == "--help") return printHelp(argv[0], 0);
    else {
      std::cerr << "ERROR: Unknown option \"" << flag << "\".\n\n";
      return printHelp(argv[0], 1);
    }
  }
  
  std::string inFilename = argv[1];
  std::string outFilename = argv[2];
  
  auto result = Mugen::generate(inFilename, opt);
  bool writeResult = true;
  if (debugMode) {
    writeResult = Mugen::debug(result, outFilename);
  }
  
  if (writeResult) {
    auto writer = Mugen::Writer::get(outFilename);
    if (!writer) {
      std::cerr << "ERROR: unsupported file extension.\n\n";
      return printHelp(argv[0], 1);
    }
    
    auto [success, report] = writer->write(result);
    if (!success) return 1;
    std::cout << report << '\n';

    if (opt.printLayout) {
      std::cout << '\n' << layoutReport(result);
    }
  }
  
  return 0;
} 
