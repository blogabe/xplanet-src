#include <clocale>
#include <cmath>
#include <limits>
#include <memory>
#include <locale>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

#include "body.h"
#include "findFile.h"
#include "keywords.h"
#include "Options.h"
#include "parse.h"
#include "PlanetProperties.h"
#include "SceneValidation.h"
#include "xpDefines.h"
#include "xpUtil.h"

#include "libplanet/Planet.h"

static PlanetProperties *defaultProperties;
static PlanetProperties *currentProperties;

template <typename Number>
static bool
readConfigNumber(const char *text, Number &number)
{
    if (text == NULL) return false;
    istringstream stream(text);
    stream.imbue(locale::classic());
    if (!(stream >> number) || !std::isfinite(number)) return false;
    stream >> ws;
    return stream.eof();
}

// Keep the integer extraction (and its overflow check), but accept a
// decimal suffix made entirely of zeroes. Do not round through a float:
// even a fraction too small to survive floating-point conversion is invalid.
static bool
readConfigInteger(const char *text, int &number)
{
    if (text == NULL) return false;
    istringstream stream(text);
    stream.imbue(locale::classic());
    int value;
    if (!(stream >> value)) return false;
    if (stream.peek() == '.')
    {
        stream.get();
        if (stream.get() != '0') return false;
        while (stream.peek() == '0') stream.get();
    }
    stream >> ws;
    if (!stream.eof()) return false;
    number = value;
    return true;
}

template <typename Number, size_t Count>
static bool
readConfigTuple(const char *text, Number (&values)[Count])
{
    if (text == NULL) return false;
    istringstream stream(text);
    stream.imbue(locale::classic());
    for (size_t i = 0; i < Count; ++i)
    {
        if (!(stream >> values[i]) || !std::isfinite(values[i])) return false;
        char comma;
        if (i + 1 < Count && (!(stream >> comma) || comma != ',')) return false;
    }
    stream >> ws;
    return stream.eof();
}

static void
numericWarning(const string &location, const char *field)
{
    xpWarn(location + ": invalid " + field + " value; keeping the previous value.\n",
           __FILE__, __LINE__);
}

static void
readConfig(const char *line, PlanetProperties *planetProperties[], const string &location)
{
    int i = 0;
    while (isDelimiter(line[i]))
    {
        i++;
        if (static_cast<unsigned int> (i) > strlen(line)) return;
    }
    if (isEndOfLine(line[i])) return;

    Options *options = Options::getInstance();

    while (static_cast<unsigned int> (i) < strlen(line))
    {
        char *returnString = NULL;
        int val = parse(i, line, returnString);
        unique_ptr<char[]> token(returnString);

        if (val != BODY && currentProperties == NULL)
            xpExit("No Planet defined in config file!\n", 
                   __FILE__, __LINE__);

        switch (val)
        {
        case ARC_COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->ArcColor(color);
            }
            else
            {
                xpWarn("Need three values for arc_color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case ARC_FILE:
            currentProperties->AddArcFile(returnString);
            break;
        case BODY:
        {
            body index = Planet::parseBodyName(returnString);
            if (index < RANDOM_BODY)
            {
                // [default] should come first in the config file.
                // The first time we get to a body after [default],
                // set all of the planet properties to the default
                // value.  If [default] isn't first, all of the bodies
                // specified before it get wiped out.
                if (currentProperties == defaultProperties)
                {
                    for (int ibody = SUN; ibody < RANDOM_BODY; ibody++)
                    {
                        *planetProperties[ibody] = *defaultProperties;
                    }
                }

                currentProperties = planetProperties[index];
            }
            else if (index == DEFAULT)
            {
                // We really shouldn't have to do this, since
                // currentProperties is set to defaultProperties
                // before the file is read in, and [default] should
                // come at the top.
                currentProperties = defaultProperties;
            }
            else
            {
                xpExit("Unknown body in config file\n", __FILE__, __LINE__);
            }
        }
        break;
        case BUMP_MAP:
        {
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "bump_map specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            string bumpMap(returnString);
            if (bumpMap.find('.') == string::npos)
                bumpMap += defaultMapExt;
            currentProperties->BumpMap(bumpMap);
        }
        break;
        case BUMP_SCALE:
        {
            double bumpScale;
            if (!readConfigNumber(returnString, bumpScale))
            {
                numericWarning(location, "bump_scale");
                break;
            }
            currentProperties->BumpScale(bumpScale);
        }
        break;
        case BUMP_SHADE:
        {
            int s;
            if (!readConfigInteger(returnString, s))
            {
                numericWarning(location, "bump_shade");
                break;
            }
            if (s < 0) 
                s = 0;
            else if (s > 100) 
                s = 100;
            currentProperties->BumpShade(s/100.);
        }
        break;
        case CLOUD_GAMMA:
        {
            double cloudGamma;
            if (!readConfigNumber(returnString, cloudGamma) || cloudGamma <= 0)
            {
                numericWarning(location, "cloud_gamma");
                break;
            }
            currentProperties->CloudGamma(cloudGamma);
        }
        break;
        case CLOUD_MAP:
        {
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "cloud_map specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            string cloudMap(returnString);
            if (cloudMap.find('.') == string::npos)
                cloudMap += defaultMapExt;
            currentProperties->CloudMap(cloudMap);
        }
        break;
        case CLOUD_SSEC:
            currentProperties->SSECMap(returnString[0] == 't'
                                     || returnString[0] == 'T');
            break;
        case CLOUD_THRESHOLD:
        {
            int t;
            if (!readConfigInteger(returnString, t))
            {
                numericWarning(location, "cloud_threshold");
                break;
            }
            if (t < 0) 
                t = 0;
            else if (t > 255) 
                t = 255;
            currentProperties->CloudThreshold(t);
        }
        break;
        case COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->Color(color);
            }
            else
            {
                xpWarn("Need three values for color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case DAY_MAP:
        case IMAGE:
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "image or map specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            currentProperties->DayMap(returnString);
            break;
        case DELIMITER:
            break;
        case DRAW_ORBIT:
        {
            bool drawOrbit = (returnString[0] == 't' 
                              || returnString[0] == 'T');
            currentProperties->DrawOrbit(drawOrbit);
        }
        break;
        case ENDOFLINE:
            break;
        case GRID:
        {
            bool grid = (returnString[0] == 't' 
                         || returnString[0] == 'T');
            currentProperties->Grid(grid);
        }
        break;
        case GRID1:
        {
            int grid1;
            if (!readConfigInteger(returnString, grid1))
            {
                numericWarning(location, "grid1");
                break;
            }
            if (grid1 < 0 || grid1 > 90) 
                xpExit("grid1 must be between 0 and 90\n", 
                       __FILE__, __LINE__);
            currentProperties->Grid1(grid1);
        }
        break;
        case GRID2:
        {
            int grid2;
            if (!readConfigInteger(returnString, grid2))
            {
                numericWarning(location, "grid2");
                break;
            }
            if (grid2 < 0)
                xpExit("grid2 must be positive\n", __FILE__, __LINE__);
            currentProperties->Grid2(grid2);
        }
        break;
        case GRID_COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->GridColor(color);
            }
            else
            {
                xpWarn("Need three values for grid_color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case MAGNIFY:
        {
            double value;
            if (readConfigNumber(returnString, value) && value > 0)
                currentProperties->Magnify(value);
            else
                numericWarning(location, "magnify");
        }
        break;
        case MAP_BOUNDS:
        {
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "map_bounds specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            double bounds[4];
            if (readConfigTuple(returnString, bounds) && bounds[0] > bounds[2]
                && bounds[0] <= 90 && bounds[2] >= -90 && bounds[1] != bounds[3])
                currentProperties->MapBounds(true, bounds[0], bounds[1], bounds[2], bounds[3]);
            else
                numericWarning(location, "map_bounds");
        }
        break;
        case MARKER_COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->MarkerColor(color);
            }
            else
            {
                xpWarn("Need three values for marker_color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case MARKER_FILE:
            currentProperties->AddMarkerFile(returnString);
            break;
        case MARKER_FONT:
            currentProperties->MarkerFont(returnString);
            break;
        case MARKER_FONTSIZE:
        { 
            int fontSize;
            if (!readConfigInteger(returnString, fontSize))
            {
                numericWarning(location, "marker_fontsize");
                break;
            }
            if (fontSize > 0)
            {
                currentProperties->MarkerFontSize(fontSize);
            }
            else
            {
                xpWarn("fontSize must be positive.\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case MAX_RAD_FOR_LABEL:
        {
            double value;
            if (!readConfigNumber(returnString, value))
            {
                numericWarning(location, "max_rad_for_label");
                break;
            }
            currentProperties->MaxRadiusForLabel(value);
        }
        break;
        case MIN_RAD_FOR_LABEL:
        {
            double value;
            if (!readConfigNumber(returnString, value))
            {
                numericWarning(location, "min_rad_for_label");
                break;
            }
            currentProperties->MinRadiusForLabel(value);
        }
        break;
        case MIN_RAD_FOR_MARKERS:
        {
            double value;
            if (!readConfigNumber(returnString, value))
            {
                numericWarning(location, "min_rad_for_markers");
                break;
            }
            currentProperties->MinRadiusForMarkers(value);
        }
        break;
        case NAME:
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "name specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            currentProperties->Name(returnString);
            break;
        case NIGHT_MAP:
            // This is one that doesn't belong in [default]
            if (currentProperties == defaultProperties)
            {
                ostringstream errStr;
                errStr << "night_map specified in [default] section.  "
                       << "You probably want to put it in a specific "
                       << "planet's section (like [earth]).\n";
                xpWarn(errStr.str(), __FILE__, __LINE__);
            }
            currentProperties->NightMap(returnString);
            break;
        case ORBIT:
        {
            double orbit[3];
            if (readConfigTuple(returnString, orbit) && orbit[0] <= orbit[1] && orbit[2] > 0)
            {
                currentProperties->StartOrbit(orbit[0]);
                currentProperties->StopOrbit(orbit[1]);
                currentProperties->DelOrbit(orbit[2]);
            }
            else
                numericWarning(location, "orbit");
        }
        break;
        case ORBIT_COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->OrbitColor(color);
            }
            else
            {
                xpWarn("Need three values for orbit_color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case RANDOM_ORIGIN:
            currentProperties->RandomOrigin(returnString[0] == 't' 
                                            || returnString[0] == 'T');
            break;
        case RANDOM_TARGET:
            currentProperties->RandomTarget(returnString[0] == 't' 
                                            || returnString[0] == 'T');
            break;
        case RAYLEIGH_EMISSION_WEIGHT:
        {
            double scale;
            if (!readConfigNumber(returnString, scale))
            {
                numericWarning(location, "rayleigh_emission_weight");
                break;
            }
            currentProperties->RayleighEmissionWeight(scale);
        }
        break;
        case RAYLEIGH_FILE:
            currentProperties->RayleighFile(returnString);
            break;
        case RAYLEIGH_LIMB_SCALE:
        {
            double scale;
            if (!readConfigNumber(returnString, scale))
            {
                numericWarning(location, "rayleigh_limb_scale");
                break;
            }
            currentProperties->RayleighLimbScale(scale);
        }
        break;
        case RAYLEIGH_SCALE:
        {
            double scale;
            if (!readConfigNumber(returnString, scale))
            {
                numericWarning(location, "rayleigh_scale");
                break;
            }
            currentProperties->RayleighScale(scale);
        }
        break;
        case SATELLITE_FILE:
            currentProperties->AddSatelliteFile(returnString);
            break;
        case SHADE:
        {
            double s;
            if (!readConfigNumber(returnString, s) || std::trunc(s) != s)
            {
                numericWarning(location, "shade");
                break;
            }
            if (s < 0) 
                s = 0;
            else if (s > 100) 
                s = 100;
            currentProperties->Shade(s/100.);
        }
        break;
        case SPECULAR_MAP:
            currentProperties->SpecularMap(returnString);
            break;
        case TEXT_COLOR:
        {
            int rgb[3];
            if (readConfigTuple(returnString, rgb) &&
                rgb[0] >= 0 && rgb[0] <= 255 && rgb[1] >= 0 && rgb[1] <= 255 &&
                rgb[2] >= 0 && rgb[2] <= 255)
            {
                unsigned char color[3] = {
                    static_cast<unsigned char>(rgb[0]),
                    static_cast<unsigned char>(rgb[1]),
                    static_cast<unsigned char>(rgb[2]) };
                currentProperties->TextColor(color);
            }
            else
            {
                xpWarn("Need three values for text_color\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case THICKNESS:
        {
            int thickness;
            if (!readConfigInteger(returnString, thickness))
            {
                numericWarning(location, "thickness");
                break;
            }
            if (thickness > 0)
            {
                currentProperties->ArcThickness(thickness);
            }
            else
            {
                xpWarn("thickness must be positive.\n", 
                       __FILE__, __LINE__);
            }
        }
        break;
        case TWILIGHT:
        {
            int value;
            if (!readConfigInteger(returnString, value))
            {
                numericWarning(location, "twilight");
                break;
            }
            if (value >= 0 && value <= 90)
            {
                currentProperties->Twilight(value);
            }
            else
            {
                xpWarn("Twilight value should be between 0 and 90 degrees\n",
                       __FILE__, __LINE__);
            }
        }
        break;
        default:
        {
            ostringstream errStr;
            errStr << "Unknown keyword in configuration file:\n\t" 
                   << line << endl;
            xpWarn(errStr.str(), __FILE__, __LINE__);
        }
        break;
        }

        if (val != DELIMITER && options->Verbosity() > 3)
        {
            ostringstream msg;
            msg << "value is " << keyWordString[val - '?'];
            if (returnString != NULL)
                msg << ", returnString is " << returnString;
            msg << endl;
            xpMsg(msg.str(), __FILE__, __LINE__);
        }

        if (val == ENDOFLINE) break;
    }
}

void
readConfigFile(string configFile, PlanetProperties *planetProperties[])
{
    bool foundFile = findFile(configFile, "config", !SceneValidation::active());
    const SceneValidation::Location configLocation(configFile);
    if (foundFile)
    {
        unique_ptr<PlanetProperties> defaults(new PlanetProperties(UNKNOWN_BODY));
        defaultProperties = defaults.get();
        currentProperties = defaultProperties;

        ifstream inFile(configFile.c_str());
        char line[4096];
        size_t lineNumber = 0;
        for (;;)
        {
            ++lineNumber;
            const SceneValidation::Location lineLocation(configFile, lineNumber);
            if (inFile.getline(line, sizeof(line)))
            {
                ostringstream location;
                location << configFile << ':' << lineNumber;
                readConfig(line, planetProperties, location.str());
            }
            else if (inFile.eof() || inFile.bad() || inFile.gcount() == 0)
                break;
            else
            {
                inFile.clear();
                inFile.ignore(numeric_limits<streamsize>::max(), '\n');
                ostringstream message;
                message << configFile << ':' << lineNumber
                        << ": ignoring an overlong configuration line\n";
                xpWarn(message.str(), __FILE__, __LINE__);
            }
        }
        
        // This condition will only be true if [default] is the only
        // section in the config file.  In this case, set all planet
        // properties to the default values.
        if (currentProperties == defaultProperties)
        {
            for (int ibody = SUN; ibody < RANDOM_BODY; ibody++)
            {
                *planetProperties[ibody] = *defaultProperties;
            }
        }

        inFile.close();

        if (SceneValidation::active() && inFile.bad())
            xpExit("Error reading configuration file " + configFile + "\n", __FILE__, __LINE__);
    }
    else
    {
        ostringstream errStr;
        errStr << "Can't load configuration file " << configFile << endl;
        xpExit(errStr.str(), __FILE__, __LINE__);
    }
}
