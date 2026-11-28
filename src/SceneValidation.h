#ifndef SCENE_VALIDATION_H
#define SCENE_VALIDATION_H

#include <cstddef>
#include <string>

class Options;

namespace SceneValidation
{
bool active();
int run(Options &options, int argc, char **argv);
void parserWarning(const std::string &message);
void requiredResourceError(const std::string &message);
void satelliteOrbitSource(const std::string &marker, const std::string &path,
                          const char *format);
[[noreturn]] void parserError(const std::string &message);

// Attach config locations to diagnostics from the shared parser.
class Location
{
public:
    explicit Location(const std::string &path, std::size_t line = 0);
    ~Location();
    Location(const Location &) = delete;
    Location &operator=(const Location &) = delete;
private:
    bool enabled_ = false;
    std::string oldPath_;
    std::size_t oldLine_ = 0;
};
}

#endif
