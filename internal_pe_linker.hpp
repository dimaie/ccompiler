#ifndef INTERNAL_PE_LINKER_HPP
#define INTERNAL_PE_LINKER_HPP

#include "internal_assembler.hpp"
#include <string>

class InternalPELinker {
public:
  static bool link(AssembledModule module,
                   const std::string &output_filepath,
                   std::string &error_msg);
};

#endif // INTERNAL_PE_LINKER_HPP
