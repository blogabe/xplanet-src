#ifndef FINDFILE_H
#define FINDFILE_H

#include <string>
#include <vector>

extern void
buildDirectoryVector(std::vector<std::string> &directoryVector, 
                     const std::string &subdir);

extern bool
findFile(std::string &filename, const std::string &subdir, bool warnIfMissing = true);

// Find a readable companion by directory priority, then suffix priority.
extern bool
findCompanionFile(const std::string &name, const std::string &subdir,
                  const std::vector<std::string> &suffixes, std::string &filename);

#endif

