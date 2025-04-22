#include <filesystem>
#include <fstream>
#include <iostream>
#include "mugen.h"

template <typename Tuple>
struct FindWriter;

template <typename ... TupleTypes>
struct FindWriter<std::tuple<TupleTypes ...>> {
  static std::unique_ptr<Mugen::Writer> find(std::string const &filename) {
    std::unique_ptr<Mugen::Writer> ptrs[] = {
      std::make_unique<TupleTypes>(filename) ...
    };

    std::string ext = std::filesystem::path(filename).extension().string();
    for (size_t idx = 0; idx != sizeof...(TupleTypes); ++idx) {
      auto vec = ptrs[idx]->extensions();
      for (std::string const &str: vec) {
	if (str == ext) return std::move(ptrs[idx]);
      }
    }

    return std::unique_ptr<Mugen::Writer>{nullptr};
  }
};

std::unique_ptr<Mugen::Writer> Mugen::Writer::get(std::string const &filename) {
  return FindWriter<Mugen::Writers>::find(filename);
}
