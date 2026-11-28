#ifndef OMM_H
#define OMM_H

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "Satellite.h"

namespace omm
{
struct Record
{
    sgp4sdp4::tle_t elements;
    std::string name;
};

struct Diagnostic
{
    std::size_t line;
    std::string message;
};

// CelesTrak GP CSV, with one orbital record per line. Valid records are
// returned even when other rows fail. An empty file is not a valid load.
bool readCsv(std::istream &input, std::vector<Record> &records,
             std::vector<Diagnostic> &diagnostics);
}

#endif
