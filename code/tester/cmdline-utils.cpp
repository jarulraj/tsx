#include <cmath>
#include <sstream>

#include <iostream>
#include "cmdline-utils.h"

using namespace std;

const int VALUE_LENGTH = 10;
const int NUM_KEYS = 1024;

mutex global_cout_mutex;
int total_txns = 0;

#define STR_VALUE(arg)      #arg
#define STRINGIFY(arg)      STR_VALUE(arg) /* Weird macro magic */

const option::Descriptor usage[] =
{
    {UNKNOWN,     0, "" , "",        option::Arg::None,     "Usage: htm-test [options] "
        HLE_NAME "|" RTM_NAME "|" LOCK_TABLE_NAME "|" SPIN_NAME "\n\n"
            "Options:" },
    {HELP,        0, "" , "help",    option::Arg::None,     "  --help  \tPrint usage and exit." },
    {NUM_THREADS, 0, "t", "threads", option::Arg::Integer,  "  --threads, -t  \tNumber of threads to run with."
        " Default: max supported by hardware." },
    {NUM_SECONDS, 0, "s", "seconds", option::Arg::Integer,  "  --seconds, -s  \tNumber of seconds to run for."
        " Default: " STRINGIFY(DEFAULT_SECONDS) "." },
    {OPS_PER_TXN, 0, "o", "txn_ops", option::Arg::Integer,  "  --txn_ops, -o  \tOperations per transaction."
        " Default: " STRINGIFY(DEFAULT_OPS_PER_TXN) "." },
    {RATIO,      0,  "r", "ratio",   option::Arg::Required, "  --ratio,   -r  \tRatio of gets to puts in each transaction,"
        " in the format gets:puts. Default: 1:1." },
    {0,0,0,0,0,0}
};

double getRatio(const option::Option *options) {
    if (!options[RATIO]) {
        return 1.0;
    }
        
    const char *arg = options[RATIO].arg;
    std::istringstream argStream(arg);
    int gets;
    int inserts;
    argStream >> gets;
    if (argStream.bad()) {
        return nan("");
    }
    if (argStream.get() != ':') {
        return nan("");
    }
    argStream >> inserts;
    if (argStream.bad()) {
        return nan("");
    }
    return gets / static_cast<double>(inserts);
}
