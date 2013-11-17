#ifndef _UTIL_H_
#define _UTIL_H_

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include "optionparser.h"

using namespace std;

std::mutex global_cout_mutex;

// Replaces every character in the provided string with a random alphanumeric
// character.
inline void GenRandomString(string *result) {
    static const char ALPHANUM_CHARS[] = "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    for (size_t i = 0; i < result->size(); ++i) {
        (*result)[i] = ALPHANUM_CHARS[rand() % (sizeof(ALPHANUM_CHARS) - 1)];
    }
}

constexpr int VALUE_LENGTH = 10;
constexpr int NUM_KEYS = 1024;

// Stupid dummy function to make threading library link properly
void pause_thread(int n) {
  std::this_thread::sleep_for(std::chrono::seconds(n));
}


#define STR_VALUE(arg)      #arg
#define STRINGIFY(arg)      STR_VALUE(arg) /* Weird macro magic */
#define DEFAULT_SECONDS     10
#define DEFAULT_OPS_PER_TXN 10
#define HLE_NAME            "hle"
#define RTM_NAME            "rtm"
#define SPIN_NAME           "spin"
#define LOCK_TABLE_NAME     "tbl"
enum  optionIndex {UNKNOWN, HELP, NUM_THREADS, NUM_SECONDS, OPS_PER_TXN, RATIO};
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

inline int getArgWithDefault(const option::Option *options, optionIndex index, int defaultVal) {
    if (options[index]) {
        return atoi(options[index].arg);
    } else {
        return defaultVal;
    }
}

double getRatio(const option::Option *options) {
    if (!options[RATIO]) {
        return 1.0;
    }

    const char *arg = options[RATIO].arg;
    istringstream argStream(arg);
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

#endif /* _UTIL_H_ */
