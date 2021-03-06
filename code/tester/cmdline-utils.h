#ifndef _CMD_LINE_UTILS_H_
#define _CMD_LINE_UTILS_H_

#include <mutex>
#include <string>

#include "optionparser.h"

#define DEFAULT_SECONDS      10
#define DEFAULT_OPS_PER_TXN  10
#define DEFAULT_KEYS_PER_TXN 10
#define DEFAULT_KEYS         16384
#define DEFAULT_VALUE_LENGTH 4
#define DEFAULT_VERBOSITY    1
#define HLE_NAME             "hle"
#define RTM_NAME             "rtm"
#define RTM_OPT_NAME         "rtmopt"
#define SPIN_NAME            "spin"
#define LOCK_TABLE_NAME      "tbl"
#define LOCK_TABLE_RW_NAME   "rwtbl"
#define SPIN_SIMPLE_NAME     "sspin"
#define ZIPF_NAME            "zipf"
#define UNIFORM_NAME         "uniform"
#define HOTSPOT_NAME         "hotspot"
#define DEFAULT_DIST_NAME    UNIFORM_NAME
#define DEFAULT_HS_FRAC      0.1
#define DEFAULT_HS_OP_FRAC   0.9


extern std::mutex global_cout_mutex;

enum optionIndex {
    UNKNOWN,
    HELP,
    NUM_THREADS,
    NUM_SECONDS,
    OPS_PER_TXN,
    KEYS_PER_TXN,
    RATIO,
    NUM_KEYS,
    VALUE_LENGTH,
    SANITY_TEST,
    KEY_DIST,
    VERBOSITY,
    DYNAMIC,
    HOTSPOT_FRAC,
    HOTSPOT_OP_FRAC
};
extern const option::Descriptor usage[];

inline double getArgWithDefault(const option::Option *options, optionIndex index, double defaultVal) {
    if (options[index]) {
        return atof(options[index].arg);
    } else {
        return defaultVal;
    }
}

inline int getArgWithDefault(const option::Option *options, optionIndex index, int defaultVal) {
    if (options[index]) {
        return atoi(options[index].arg);
    } else {
        return defaultVal;
    }
}

inline std::string getArgWithDefault(const option::Option *options,
        optionIndex index, const std::string &defaultVal) {
    if (options[index]) {
        if (options[index].arg) {
            return std::string(options[index].arg);
        } else {
            return std::string();
        }
    } else {
        return defaultVal;
    }
}

namespace option {

double getRatio(const option::Option &options);

option::ArgStatus Unknown(const option::Option& option, bool msg);
option::ArgStatus Integer(const option::Option& option, bool msg);
option::ArgStatus Double(const option::Option& option, bool msg);
option::ArgStatus Required(const option::Option& option, bool);
option::ArgStatus CheckRatio(const option::Option& option, bool msg);

}  // namespace option

#endif /* _CMD_LINE_UTILS_H_ */
