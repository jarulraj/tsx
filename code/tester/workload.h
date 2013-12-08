#ifndef _WORKLOAD_H_
#define _WORKLOAD_H_

#include <chrono>
#include <cstdlib>
#include <random>
#include <thread>

#include "hashtable.h"
#include "generators.h"
#include "TxnManager.h"

using namespace std;


#define TIME_CODE(stats, code) \
    auto __timing_start = std::chrono::high_resolution_clock::now();\
    code;\
    \
    auto __timing_end = std::chrono::high_resolution_clock::now();\
    stats->lock_acq_time += __timing_end - __timing_start;


struct ThreadStats {
    thread::id thread_id;
    int transactions;
    int gets;
    int inserts;
    std::chrono::high_resolution_clock::duration lock_acq_time;
    ThreadStats()
            : gets(0), transactions(0), inserts(0), lock_acq_time(0) {}
};

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
void RunTestWriterThread(TxnManager *manager, ThreadStats *stats, long max_key,
        int seconds_to_run, size_t value_length);

// This thread performs a series of transactions in which the same key is read
// num_reads times. It complains if the reads are not repeatable. Like
// RunTestWriterThread, it cycles through all possible keys.
void RunTestReaderThread(TxnManager *manager, ThreadStats *stats, long max_key,
        int num_reads, int seconds_to_run, size_t value_length);

// This thread performs a series of transactions in which the same key is
// written and then read num_reads times. It complains if the reads do not
// return the written value. Like the other threads, it cycles through keys.
// This is to test writer/writer conflicts for the read/write lock manager.
void RunTestReaderWriterThread(TxnManager *manager, ThreadStats *stats,
        long max_key, int num_reads, int seconds_to_run, size_t value_length);

void RunMultiKeyThread(TxnManager *manager, ThreadStats *stats, long max_key,
        int num_ops, int seconds_to_run, size_t value_length);

void RunWorkloadThread(TxnManager *manager, ThreadStats *stats, int ops_per_txn,
	int keys_per_txn, int key_max, int seconds_to_run, double get_to_put_ratio,
        size_t value_length, Generator<long> *key_generator);

#endif /* _WORKLOAD_H__ */
