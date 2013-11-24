#include <cmath>
#include <sstream>

#include <iostream>
#include "cmdline-utils.h"

using namespace std;

mutex global_cout_mutex;

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
    {RATIO,       0, "r", "ratio",   CheckRatio, "  --ratio,   -r  \tRatio of gets to puts in each"
        " transaction, in the format gets:puts. Default: 1:1." },
    {NUM_KEYS,    0, "n", "keys",    option::Arg::Integer,  "  --keys,    -n  \tNumber of keys in the database."
        " Default: " STRINGIFY(DEFAULT_KEYS) "." },
    {VALUE_LENGTH,0, "v", "val_len", option::Arg::Integer,  "  --val_len, -v  \tLength of each value string."
        " Default: " STRINGIFY(DEFAULT_VALUE_LENGTH) "." },
    {SANITY_TEST, 0, "a", "sanity",  option::Arg::None,     "  --sanity,  -a  \tRun a sanity test to check"
        " validity of CC schemes, instead of real workloads. Disables -t, -o, and -r flags."},
    {KEY_DIST,    0, "k", "keydist", option::Arg::Required, "  --keydist, -k  \tDistribution of keys to use."
        " Permitted values: " UNIFORM_NAME ", " ZIPF_NAME ". Default: " DEFAULT_DIST_NAME "."},
    {0,0,0,0,0,0}
};

double getRatio(const option::Option &option) {
    // Using static variables to memoize the result of this computation so that the ratio
    // need not be re-parsed for the same option both for error checking and for parsing.
    // (This is a bit of an ugly hack.)
    static const option::Option *last_option(NULL);
    static double last_ratio(nan(""));
    if (last_option == &option) {
        return last_ratio;
    }
    last_option = &option;

    if (!option) {
        return 0.5;
    }
        
    const char *arg = option.arg;
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

    last_ratio = gets / static_cast<double>(inserts + gets);
    return last_ratio;
}

option::ArgStatus CheckRatio(const option::Option& option, bool msg) {
    if (std::isnan(getRatio(option))) {
        return option::ARG_ILLEGAL;
    } else {
        return option::ARG_OK;
    }
}
