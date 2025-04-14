#include <iostream>
#include <fstream>
#include "mugen.h"
#include "util.h"

int printHelp(std::string const &progName, int ret) {
  std::cout << "Usage: " << progName << " <specification-file (.mu)> <output-file> [OPTIONS]\n\n"
            << "Mugen is a microcode generator that converts a specification file\n"
            << "into microcode images suitable for flashing onto ROM chips.\n"
            << "Optionally, the layout report can be printed using the --layout or -l flag.\n"
            << "See https://github.com/jorenheit/mugen for more help.\n\n"
            << "Options:\n"
            << "  -h, --help       Display this help message and exit\n"
            << "  -l, --layout     Print the ROM layout report after generation\n"
            << "  -m, --msb-first  Store signals starting from the most significant bit.\n"
            << "  -p, --pad VALUE  Pad the remainder of the rom with the supplied value (may be hex).\n"
            << "  -p, --pad catch  Pad the remainder of the rom with the signals specified in the catch-rule.\n"
            << "  -d, --debug      Run Mugen in an interactive debug mode. Type \"help\" for more information.\n"
            << "\nExample:\n"
            << "  " << progName << " myspec.mu microcode.bin --pad catch --msb-first --layout\n";
  
  return ret;
}

int main(int argc, char **argv) {
  
  if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    printHelp(argv[0], 0);
    return 0;
  }
  if (argc < 3) {
    std::cerr << "ERROR: Invalid number of arguments.\n\n";
    printHelp(argv[0], 1);
    return 1;
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
    writeResult = Mugen::debug(inFilename, outFilename, result);
  }
  
  if (writeResult) {
    std::vector<std::string> files;
    for (size_t idx = 0; idx != result.images.size(); ++idx) {
      std::string filename = outFilename + ((result.images.size() > 1) ? ("." + std::to_string(idx)) : "");
      std::ofstream out(filename, std::ios::binary);
      if (!out) {
        std::cerr << "ERROR: Could not open output file \"" << filename << "\".";
        return 1;
      }
      
      files.push_back(filename);
      out.write(reinterpret_cast<char const *>(result.images[idx].data()), result.images[idx].size());
      out.close();
    }
    
    std::cout << "Successfully generated " << result.images.size() << " images from " << inFilename <<": \n";
    for (size_t idx = 0; idx != result.images.size(); ++idx) {
      std::cout << "  " << "ROM " << idx << " : " << files[idx]
                << " (" << result.images[idx].size() << " bytes)\n";
    }
    
    if (opt.printLayout) {
      std::cout << '\n' << result.layout;
    }
  }
  
  return 0;
} 
