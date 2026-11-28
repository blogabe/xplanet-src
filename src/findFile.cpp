#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include <sys/stat.h>

#include "Options.h"
#include "xpDefines.h"
#include "xpUtil.h"

static bool
fileExists(string &filename)
{
    bool returnVal = false;

    ostringstream msg;
    msg << "Looking for " << filename << " ... ";
    struct stat status;
    if (stat(filename.c_str(), &status) != 0)
        msg << "not found\n";
    else if (!S_ISREG(status.st_mode))
        msg << "is not a regular file!\n";
    else
    {
        ifstream file(filename.c_str());
        returnVal = file.is_open();
        msg << (returnVal ? "found\n" : "not readable\n");
    }

    Options *options = Options::getInstance();
    if (options->Verbosity() > 2)
    {
        // only say we're looking for a file once
        static vector<string> searchedFor;
        bool firstTime = true;
        for (int i = searchedFor.size()-1; i >= 0; i--)
        {
            if (searchedFor[i] == filename)
            {
                firstTime = false;
                break;
            }
        }
        if (firstTime)
        {
            searchedFor.push_back(filename);
            xpMsg(msg.str(), __FILE__, __LINE__);
        }
    }

    return(returnVal);
}

bool
findFile(string &filename, const string &subdir)
{
    // Check if the file exists in the current directory before going
    // to searchdir
    if (fileExists(filename)) return(true);

    Options *options = Options::getInstance();
    vector<string> searchdir = options->getSearchDir();

    string newname;
    for (int i = searchdir.size() - 1; i >= 0; i--)
    {
        // Check in searchdir itself
        newname = searchdir[i];
        newname += separator;
        newname += filename;

        if (fileExists(newname))
        {
            filename = newname;
            return(true);
        }

        // Now look in searchdir + subdir
        newname = searchdir[i];
        newname += separator;
        if (!subdir.empty())
        {
            newname += subdir;
            newname += separator;
        }
        newname += filename;

        if (fileExists(newname))
        {
            filename = newname;
            return(true);
        }
    }

    string errMsg("Can't find ");
    errMsg += filename;
    errMsg += " in\n";
    for (int i = searchdir.size() - 1; i >= 0; i--)
    {
        errMsg += searchdir[i];
        errMsg += separator;
        errMsg += subdir;
        errMsg += "\n";
    }
    xpWarn(errMsg, __FILE__, __LINE__);
    return(false);
}
