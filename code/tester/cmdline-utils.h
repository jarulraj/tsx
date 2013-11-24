#ifndef _CMD_LINE_UTILS_H_
#define _CMD_LINE_UTILS_H_

#include <mutex>
#include <string>

#include "optionparser.h"

#define DEFAULT_SECONDS      10
#define DEFAULT_OPS_PER_TXN  10
#define DEFAULT_KEYS         4096
#define DEFAULT_VALUE_LENGTH 4
#define HLE_NAME             "hle"
#define RTM_NAME             "rtm"
#define SPIN_NAME            "spin"
#define LOCK_TABLE_NAME      "tbl"
#define ZIPF_NAME            "zipf"
#define UNIFORM_NAME         "uniform"
#define DEFAULT_DIST_NAME    UNIFORM_NAME


extern std::mutex global_cout_mutex;

enum optionIndex {
    UNKNOWN,
    HELP,
    NUM_THREADS,
    NUM_SECONDS,
    OPS_PER_TXN,
    RATIO,
    NUM_KEYS,
    VALUE_LENGTH,
    SANITY_TEST,
    KEY_DIST
};
extern const option::Descriptor usage[];

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

double getRatio(const option::Option &options);
option::ArgStatus CheckRatio(const option::Option& option, bool msg);

#endif /* _CMD_LINE_UTILS_H_ */
