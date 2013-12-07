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
    {UNKNOWN,     0, "" , "",        option::Arg::None,     "Usage: tester [options] "
        HLE_NAME "|" RTM_NAME "|" LOCK_TABLE_NAME "|" LOCK_TABLE_RW_NAME "|" SPIN_NAME "|" SPIN_SIMPLE_NAME " \n\n"
            "Options:" },
    {HELP,        0, "" , "help",    option::Arg::None,     "  --help  \tPrint usage and exit." },
    {NUM_THREADS, 0, "t", "threads", option::Integer,  "  --threads, -t  \tNumber of threads to run with."
        " Default: max supported by hardware." },
    {NUM_SECONDS, 0, "s", "seconds", option::Integer,  "  --seconds, -s  \tNumber of seconds to run for."
        " Default: " STRINGIFY(DEFAULT_SECONDS) "." },
    {OPS_PER_TXN, 0, "o", "txn_ops", option::Integer,  "  --txn_ops, -o  \tOperations per transaction."
        " Default: " STRINGIFY(DEFAULT_OPS_PER_TXN) "." },
    {KEYS_PER_TXN, 0, "y", "txn_keys", option::Integer,  "  --txn_keys, -y  \tKeys per transaction."
        " Default: " STRINGIFY(DEFAULT_KEYS_PER_TXN) "." },
    {RATIO,       0, "r", "ratio",   option::CheckRatio, "  --ratio,   -r  \tRatio of gets to puts in each"
        " transaction, in the format gets:puts. Default: 1:1." },
    {NUM_KEYS,    0, "n", "keys",    option::Integer,  "  --keys,    -n  \tNumber of keys in the database."
        " Default: " STRINGIFY(DEFAULT_KEYS) "." },
    {VALUE_LENGTH,0, "v", "val_len", option::Integer,  "  --val_len, -v  \tLength of each value string."
        " Default: " STRINGIFY(DEFAULT_VALUE_LENGTH) "." },
    {SANITY_TEST, 0, "a", "sanity",  option::Arg::None,     "  --sanity,  -a  \tRun a sanity test to check"
        " validity of CC schemes, instead of real workloads. Disables -t, -o, -r, and -d flags."},
    {KEY_DIST,    0, "k", "keydist", option::Required, "  --keydist, -k  \tDistribution of keys to use."
        " Permitted values: " UNIFORM_NAME ", " ZIPF_NAME ". Default: " DEFAULT_DIST_NAME "."},
    {VERBOSITY,   0, "e", "verbosity", option::Integer, "  --verbosity, -e \tRelative amount of detail to"
        " print out. Default: " STRINGIFY(DEFAULT_VERBOSITY) "."},
    {DYNAMIC,     0, "d", "dynamic", option::Arg::None, "  --dynamic, -d \tUse dynamic read/write sets."},
    {0,0,0,0,0,0}
};

namespace option {

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
        if (msg) {
            cerr << "ERROR: Ratio option must be in the format <int>:<int>" << endl;
        }
        return option::ARG_ILLEGAL;
    } else {
        return option::ARG_OK;
    }
}

inline string option_name(const option::Option& option) {
    return string(option.name, option.namelen);
}

option::ArgStatus Unknown(const option::Option& option, bool msg) {
    if (msg) {
        cerr << "ERROR: Unknown option " << option_name(option)
                << endl;
    }
    return option::ARG_ILLEGAL;
}

option::ArgStatus Integer(const option::Option& option, bool msg) {
    char* endptr = 0;
    if (option.arg != 0 && strtol(option.arg, &endptr, 10)) {};
    if (endptr != option.arg && *endptr == 0)
        return option::ARG_OK;

    if (msg) {
        cerr << "ERROR: Option " << option_name(option)
                << " requires an integer argument" << endl;
    }
    return option::ARG_ILLEGAL;
}

option::ArgStatus Double(const option::Option& option, bool msg) {
    char* endptr = 0;
    if (option.arg != 0 && strtod(option.arg, &endptr)) {};
    if (endptr != option.arg && *endptr == 0)
        return option::ARG_OK;

    if (msg)
        cerr << "ERROR: Option " << option_name(option)
                << " requires a floating-point argument" << endl;
    return option::ARG_ILLEGAL;
}

option::ArgStatus Required(const option::Option& option, bool msg) {
    if (option.arg)
        return option::ARG_OK;
    else {
        if (msg) {
            cerr << "ERROR: Option " << option_name(option) << " missing required argument" << endl;
        }
        return option::ARG_ILLEGAL;
    }
}

}  // namespace option

