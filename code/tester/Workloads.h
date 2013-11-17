#ifndef _WORKLOADS_H_
#define _WORKLOADS_H_

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include "optionparser.h"
#include "hashtable.h"
#include "HLETxnManager.h"
#include "LockTableTxnManager.h"
#include "RTMTxnManager.h"
#include "SpinLockTxnManager.h"
#include "TxnManager.h"

#include "Util.h"

using namespace std;

// This thread performs a large number of single-operation transactions. It
// cycles through all possible keys, with each transaction writing to the next
// key.
void RunTestWriterThread(TxnManager *manager, long max_key,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(1);
    ops[0].value.resize(VALUE_LENGTH);

    long key = 0;
    int txn_counter = 0;
    do {
        ops[0].type = INSERT;
        ops[0].key = key;
        ops[0].value = std::to_string(key);
        
        //cout << "Write Txn Run : " << txn_counter << endl;
        manager->RunTxn(ops, NULL);
        key = (key + 1) % max_key;
        ++txn_counter;
        //cout << "Write Txn End : " << txn_counter << endl;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Write transactions: " << txn_counter << endl;
    global_cout_mutex.unlock();
}

// This thread performs a series of transactions in which the same key is read
// num_reads times. It complains if the reads are not repeatable. Like
// RunTestWriterThread, it cycles through all possible keys.
void RunTestReaderThread(TxnManager *manager, long max_key, int num_reads,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads);

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    long key = 0;
    int txn_counter = 0;
    do {
        for (int i = 0; i < num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        if (!manager->RunTxn(ops, &get_results)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed.";
            global_cout_mutex.unlock();
            return;
        }

        //cout << "Read Txn Data : " << txn_counter << endl;
        // We know there must be at least one result, because num_reads > 0.
        auto result_iter = get_results.begin();
        const string &result = *result_iter;
        //cout<<result<<" ";
        for (; result_iter != get_results.end(); ++result_iter) {
            const string &next_result = *result_iter;
            //cout<<next_result<<" ";
            if (result.compare(next_result) != 0) {
                global_cout_mutex.lock();
                cerr << "ERROR: all reads should have returned '" << result
                        << "'; got '" << next_result << "' instead" << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        get_results.clear();  // Doesn't actually change memory allocation
        key = (key + 1) % max_key;
        ++txn_counter;
        //cout << "Read Txn End : " << txn_counter << endl;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Read transactions: " << txn_counter << endl;
    global_cout_mutex.unlock();
}

// This thread performs a series of transactions in which the same key is
// written and then read num_reads times. It complains if the reads do not
// return the written value. Like the other threads, it cycles through keys.
// This is to test writer/writer conflicts for the read/write lock manager.
void RunTestReaderWriterThread(TxnManager *manager, long max_key, int num_reads,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads + 1);
    for (OpDescription &op : ops) {
        op.value.resize(VALUE_LENGTH);
    }

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    long key = 0;
    int txn_counter = 0;
    do {
        ops[0].type = INSERT;
        ops[0].key = key;
        ops[0].value = std::to_string(key);
        
        for (int i = 1; i <= num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        if (!manager->RunTxn(ops, &get_results)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed.";
            global_cout_mutex.unlock();
            return;
        }

        // We know there must be at least one result, because num_reads > 0.
        auto result_iter = get_results.begin();
        const string &result = ops[0].value;
        for (; result_iter != get_results.end(); ++result_iter) {
            const string &next_result = *result_iter;
            if (result.compare(next_result) != 0) {
                global_cout_mutex.lock();
                cerr << "ERROR: next read should have returned '" << result
                    << "'; got '" << next_result << "' instead" << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        get_results.clear();  // Doesn't actually change memory allocation
        key = (key + 1) % max_key;
        ++txn_counter;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Read/write transactions:  " << txn_counter << endl;
    global_cout_mutex.unlock();
}

void RunMultiKeyThread(TxnManager *manager, long max_key, int num_ops,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_ops + 1);
    for (OpDescription &op : ops) {
        op.value.resize(VALUE_LENGTH);
    }

    if (num_ops <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_ops);
    vector<string> inputs;
    inputs.reserve(num_ops);
    long key = 0;
    int txn_counter = 0;
    do {
        for (int i = 0; i < num_ops; i += 2) {
            ops[i].type = INSERT;
            ops[i].key = key;
            GenRandomString(&ops[i].value);
            inputs.push_back(ops[i].value);
            ops[i + 1].type = GET;
            ops[i + 1].key = key;
            key = (key + 1) % max_key;
        }
        if (!manager->RunTxn(ops, &get_results)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed.";
            global_cout_mutex.unlock();
            return;
        }

        for (auto result_iter = get_results.begin(), inputs_iter =
                inputs.begin(); result_iter != get_results.end();
                ++result_iter, ++inputs_iter) {
            const string &next_result = *result_iter;
            const string &next_input = *inputs_iter;
            if (next_input.compare(next_result) != 0) {
                global_cout_mutex.lock();
                cerr << "ERROR: read should have returned '" << next_input
                        << "'; got '" << next_result << "' instead" << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        inputs.clear();
        get_results.clear(); // Doesn't actually change memory allocation
        ++txn_counter;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Multikey transactions:  " << txn_counter << endl;
    global_cout_mutex.unlock();
}

void RunWorkloadThread(TxnManager *manager, int ops_per_txn,
        int key_max, int seconds_to_run, double get_to_put_ratio) {

    // Workload Generator

    // Seeding
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator (seed);

    // Distributions
    uniform_real_distribution<double> operation_distribution(0.0, 1.0);
    uniform_int_distribution<int> key_distribution(0, key_max);

    // Initializing all vector and string values here essentially eliminates
    // contention for the STL allocator locks. We want there to be no memory
    // management happening inside the main loop of the thread.
    vector<OpDescription> txn_ops(ops_per_txn);
    for (OpDescription &op : txn_ops) {
        op.value.resize(VALUE_LENGTH);
    }

    time_t end_time = time(NULL) + seconds_to_run;
    int txn_counter = 0;
    do {
        for (int i = 0; i < ops_per_txn; ++i) {
            double action_chooser = operation_distribution(generator);
            // Modify operation description in place. (Values left around from
            // old insert actions will just be ignored.)
            OpDescription &next_op = txn_ops[i];
            next_op.key = key_distribution(generator);

            if (action_chooser < get_to_put_ratio) {
                next_op.type = GET;
            } else {
                next_op.type = INSERT;
                GenRandomString(&next_op.value);
            }
        }

        if (!manager->RunTxn(txn_ops, NULL)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed.";
            global_cout_mutex.unlock();
            return;
        }

        ++txn_counter;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Thread " << this_thread::get_id() << ": " << txn_counter
        << " transactions" << endl;
    global_cout_mutex.unlock();
}

#endif /* _WORKLOADS_H__ */
