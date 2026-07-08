#include "printer.h"

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// cmd_buf_to_dmc
//
// Converts the CMD stringstream format used throughout this codebase into a
// raw DMC program string for GProgramDownload().
//
// Input lines look like:  "GCmd,SPA=25600\n"
// Output lines look like: "SPA=25600\n"
//
// The function strips the "GCmd," (or "GCmdInt,", "GProgramComplete,", etc.)
// prefix and the comma.  Lines without a comma are passed through as-is
// (this handles blank lines gracefully).
// ---------------------------------------------------------------------------

std::string CMD::cmd_buf_to_dmc(const std::stringstream &s)
{
    // Work on a copy so the caller's stream position is unaffected.
    std::stringstream ss;
    ss << s.rdbuf();

    std::string returnString;
    std::string buffer;

    while (std::getline(ss, buffer))
    {
        const std::string delimiter = ",";
        size_t pos = buffer.find(delimiter);

        if (pos != std::string::npos)
        {
            // Strip everything up to and including the first comma.
            buffer.erase(0, pos + delimiter.length());
        }
        // (else: no comma found — include the whole line as-is)

        returnString += buffer;
        returnString += "\n";
    }

    return returnString;
}
