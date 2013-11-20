#ifndef _CMD_LINE_UTILS_H_
#define _CMD_LINE_UTILS_H_

#include <mutex>

#include "optionparser.h"

#define DEFAULT_SECONDS      10
#define DEFAULT_OPS_PER_TXN  10
#define DEFAULT_KEYS         1048576
#define DEFAULT_VALUE_LENGTH 10
#define HLE_NAME             "hle"
#define RTM_NAME             "rtm"
#define SPIN_NAME            "spin"
#define LOCK_TABLE_NAME      "tbl"

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
    SANITY_TEST
};
extern const option::Descriptor usage[];

inline int getArgWithDefault(const option::Option *options, optionIndex index, int defaultVal) {
    if (options[index]) {
        return atoi(options[index].arg);
    } else {
        return defaultVal;
    }
}

double getRatio(const option::Option *options);

#endif /* _CMD_LINE_UTILS_H_ */
