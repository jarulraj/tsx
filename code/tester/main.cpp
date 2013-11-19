#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <utility>

#include "optionparser.h"
#include "hashtable.h"
#include "HLETxnManager.h"
#include "LockTableTxnManager.h"
#include "RTMTxnManager.h"
#include "SpinLockTxnManager.h"
#include "TxnManager.h"

#include "cmdline-utils.h"
#include "workload.h"

using namespace std;

// Stupid dummy function to make threading library link properly
void pause_thread(int n) {
  this_thread::sleep_for(std::chrono::seconds(n));
}

int main(int argc, const char* argv[]) {
    argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present
    option::Stats stats(usage, argc, argv);
    option::Option* options = new option::Option[stats.options_max];
    option::Option* buffer  = new option::Option[stats.buffer_max];
    option::Parser parse(usage, argc, argv, options, buffer);

    if (parse.error())
        return 1;

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    if (parse.nonOptionsCount() != 1) {
        option::printUsage(std::cout, usage);
        return 1;
    }

    string manager_type = parse.nonOption(0);
    int num_seconds_to_run = getArgWithDefault(options, NUM_SECONDS, DEFAULT_SECONDS);
    int num_keys = getArgWithDefault(options, NUM_KEYS, DEFAULT_KEYS);
    int value_length = getArgWithDefault(options, VALUE_LENGTH, DEFAULT_VALUE_LENGTH);
    bool sanity_test = options[SANITY_TEST];
    int num_threads;
    int ops_per_txn;
    double ratio;
    if (!sanity_test) {
        num_threads = getArgWithDefault(options, NUM_THREADS, thread::hardware_concurrency());
        ops_per_txn = getArgWithDefault(options, OPS_PER_TXN, DEFAULT_OPS_PER_TXN);
        ratio = getRatio(options);
        if (std::isnan(ratio)) {
            return 1;
        }
    }

    delete[] options;
    delete[] buffer;


    // Initialize hashtable
    // HashTable table(static_cast<ht_flags>(HT_KEY_CONST | HT_VALUE_CONST), 0.05);

    // Use std::unordered_map as hashtable 
    // Making the underlying hashtable concurrency-safe is a different problem
    std::unordered_map<long,std::string> table; 


    TxnManager *manager;
    if (manager_type == HLE_NAME) {
        manager = new HLETxnManager(&table);
    }else if (manager_type == RTM_NAME) {
        manager = new RTMTxnManager(&table);
    } else if (manager_type == LOCK_TABLE_NAME) {
        manager = new LockTableTxnManager(&table);
    } else if (manager_type == SPIN_NAME) {
        manager = new SpinLockTxnManager(&table);
    } else {
        option::printUsage(std::cout, usage);
        return 1;
    }

    // Make sure all the keys we'll be using are there so GETs don't fail
    for (long i = 0; i < num_keys; ++i) {
        string istr = std::to_string(i) ;
        std::pair<long,std::string> entry (i,istr);
        table.insert(entry);
    }

    //for (auto& x: table)
    //    std::cout << x.first << ": " << x.second << std::endl;

    vector<thread> threads;
    vector<ThreadStats> thread_stats;

    if (sanity_test) {
        // Each transaction reads keys multiple times. It's pretty likely that a writer
        // will write to the value that's being read during the time of the transaction,
        // so we should notice non-repeatable reads if concurrency control is failing.
        cout << "Running sanity check with 1 reader/writer thread"
                " and 1 multi-key reader/writer thread"
             << endl;
        thread_stats.resize(3);
        threads.push_back(
                thread(RunTestReaderWriterThread, manager, &thread_stats[0],
                        num_keys, 10 * num_keys, num_seconds_to_run, value_length));
        threads.push_back(
                thread(RunMultiKeyThread, manager, &thread_stats[1], num_keys,
                        10 * num_keys, num_seconds_to_run, value_length));
        threads.push_back(
                thread(RunMultiKeyThread, manager, &thread_stats[2], num_keys,
                        10 * num_keys, num_seconds_to_run, value_length));
    } else {
        cout << "Num threads:  " << num_threads << endl;
        cout << "Time (s):     " << num_seconds_to_run << endl;
        cout << "Ops per txn:  " << ops_per_txn << endl;
        cout << "Ratio:        " << ratio << endl;
        cout << "Num keys:     " << num_keys << endl;
        cout << "Value length: " << value_length << endl;

        thread_stats.resize(num_threads);
        for (int i = 0; i < num_threads; ++i) {
            threads.push_back(
                    thread(RunWorkloadThread, manager, &thread_stats[i], ops_per_txn, num_keys-1,
                        num_seconds_to_run, ratio, value_length));
        }
    }

    for (thread &t : threads) {
        t.join();
    }

	// Shouldn't need cout mutex anymore here -- all threads are done.
    for (const ThreadStats &stats : thread_stats) {
        cout << "Thread " << stats.thread_id << ":\n"
                << "  Total transactions: " << stats.transactions << "\n"
                << "  GETs: " << stats.gets << "\n"
                << "  INSERTs: " << stats.inserts << endl;
    }

    return 0;
}
