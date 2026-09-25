#include "core/Measurement.h"

const char *unitToString(Unit unit)
{
    switch (unit)
    {
    case Unit::CELSIUS:
        return "degC";
    case Unit::HECTOPASCAL:
        return "hPa";
    case Unit::PERCENT:
        return "pct";
    case Unit::VOLT:
        return "V";
    case Unit::AMPERE:
        return "A";
    case Unit::WATT:
        return "W";
    case Unit::LUX:
        return "lx";
    case Unit::PPM:
        return "ppm";
    case Unit::METER:
        return "m";
    case Unit::MILLIMETER:
        return "mm";
    case Unit::HERTZ:
        return "Hz";
    case Unit::KILOWATT:
        return "kW";
    case Unit::KILOVAR:
        return "kvar";
    case Unit::KILOVOLTAMPERE:
        return "kVA";
    case Unit::KILOWATTHOUR:
        return "kWh";
    case Unit::NONE:
    default:
        return "none";
    }
}
