#include <iostream>
#include <fstream>
#include <sstream>

#include "mugen.h"

std::vector<std::string> Mugen::BinaryFileWriter::extensions() const {
  return {".bin", ".rom"};
}

Mugen::WriteResult Mugen::BinaryFileWriter::write(Result const &result) {
  std::vector<std::string> files;
  for (size_t idx = 0; idx != result.images.size(); ++idx) {
    std::string filename = _filename + ((result.images.size() > 1) ? ("." + std::to_string(idx)) : "");
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
      std::cerr << "ERROR: Could not open output file \"" << filename << "\".";
      return {false, ""};
    }
      
    files.push_back(filename);
    out.write(reinterpret_cast<char const *>(result.images[idx].data()), result.images[idx].size());
    out.close();
  }

  std::ostringstream report;
  report << "Successfully generated " << result.images.size()
	 << " images from " << result.specificationFilename <<": \n\n";
    
  for (size_t idx = 0; idx != result.images.size(); ++idx) {
    report << "  " << "ROM " << idx << ": " << files[idx]
	   << " (" << result.images[idx].size() << " bytes)\n";
  }
  
  return {true, report.str()};
}

