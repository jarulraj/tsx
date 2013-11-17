#ifndef _WORKLOAD_H_
#define _WORKLOAD_H_

#include <cstdlib>
#include <random>

#include "hashtable.h"
#include "TxnManager.h"

using namespace std;

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

// This thread performs a large number of single-operation transactions. It
// cycles through all possible keys, with each transaction writing to the next
// key.
void RunTestWriterThread(TxnManager *manager, long max_key,
        int seconds_to_run);

// This thread performs a series of transactions in which the same key is read
// num_reads times. It complains if the reads are not repeatable. Like
// RunTestWriterThread, it cycles through all possible keys.
void RunTestReaderThread(TxnManager *manager, long max_key, int num_reads,
        int seconds_to_run);

// This thread performs a series of transactions in which the same key is
// written and then read num_reads times. It complains if the reads do not
// return the written value. Like the other threads, it cycles through keys.
// This is to test writer/writer conflicts for the read/write lock manager.
void RunTestReaderWriterThread(TxnManager *manager, long max_key, int num_reads,
        int seconds_to_run);

void RunMultiKeyThread(TxnManager *manager, long max_key, int num_ops,
        int seconds_to_run);

void RunWorkloadThread(TxnManager *manager, int ops_per_txn,
        int key_max, int seconds_to_run, double get_to_put_ratio);

#endif /* _WORKLOAD_H__ */
