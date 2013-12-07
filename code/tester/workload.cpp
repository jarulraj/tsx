#include <chrono>
#include <iostream>
#include <thread>

#include "cmdline-utils.h"
#include "workload.h"

using namespace std::chrono;

void RunTestWriterThread(TxnManager *manager, ThreadStats *stats, long num_keys,
        int seconds_to_run, size_t value_length) {
    stats->thread_id = this_thread::get_id();

    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(1);
    ops[0].value.resize(value_length);

    long key = 0;
    do {
        ops[0].type = INSERT;
        ops[0].key = key;
        ops[0].value = std::to_string(key);
        ++stats->inserts;

        manager->RunTxn(ops, NULL, stats);
        key = (key + 1) % num_keys;
        ++stats->transactions;
    } while (time(NULL) <= end_time);

}

void RunTestReaderThread(TxnManager *manager, ThreadStats *stats, long num_keys,
        int num_reads, int seconds_to_run) {
    stats->thread_id = this_thread::get_id();

    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads);

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    long key = 0;
    do {
        for (int i = 0; i < num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        stats->gets += num_reads;

        if (!manager->RunTxn(ops, &get_results, stats)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed in thread "
                    << this_thread::get_id();
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
                        << "'; got '" << next_result << "' instead (reader thread "
                        << this_thread::get_id() << ')' << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        get_results.clear();
        key = (key + 1) % num_keys;
        ++stats->transactions;
    } while (time(NULL) <= end_time);
}

void RunTestReaderWriterThread(TxnManager *manager, ThreadStats *stats,
        long num_keys, int num_reads, int seconds_to_run, size_t value_length) {
    stats->thread_id = this_thread::get_id();

    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads + 1);
    for (OpDescription &op : ops) {
        op.value.resize(value_length);
    }

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    long key = 0;
    do {
        ops[0].type = INSERT;
        ops[0].key = key;
        ops[0].value = std::to_string(key);
        ++stats->inserts;

        for (int i = 1; i <= num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        stats->gets += num_reads;

        if (!manager->RunTxn(ops, &get_results, stats)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed in reader/writer thread "
                    << this_thread::get_id();
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
                        << "'; got '" << next_result
                        << "' instead (reader/writer thread "
                        << this_thread::get_id() << ')' << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        get_results.clear();
        key = (key + 1) % num_keys;
        ++stats->transactions;
    } while (time(NULL) <= end_time);
}

void RunMultiKeyThread(TxnManager *manager, ThreadStats *stats, long num_keys,
        int num_ops, int seconds_to_run, size_t value_length) {
    stats->thread_id = this_thread::get_id();

    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_ops);
    // Only allocate memory for the ones that will be inserts containing values.
    for (int i = 0; i < num_ops; i += 2) {
        ops[i].value.resize(value_length);
    }

    if (num_ops <= 0) {
        return;
    } else if (num_ops % 2 == 1) {
        // Make sure we have an even number of operations.
        num_ops += 1;
    }

    vector<string> get_results;
    get_results.reserve(num_ops / 2);
    long key = 0;
    do {
        for (int i = 0; i < num_ops; i += 2) {
            ops[i].type = INSERT;
            ops[i].key = key;
            GenRandomString(&ops[i].value);
            ops[i + 1].type = GET;
            ops[i + 1].key = key;
            key = (key + 1) % num_keys;
        }
        stats->gets += num_ops / 2;
        stats->inserts += num_ops / 2;

        if (!manager->RunTxn(ops, &get_results, stats)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed in multi-key thread "
                    << this_thread::get_id();
            global_cout_mutex.unlock();
            return;
        }

        for (int i = 0; i < get_results.size(); ++i) {
            const string &next_result = get_results[i];
            const string &next_input = ops[2 * i].value;
            if (next_input.compare(next_result) != 0) {
                global_cout_mutex.lock();
                cerr << "ERROR: next read should have returned '" << next_input
                        << "'; got '" << next_result << "' instead (multi-key thread "
                        << this_thread::get_id() << ')' << endl;
                global_cout_mutex.unlock();
                break;
            }
        }

        get_results.clear();
        ++stats->transactions;
    } while (time(NULL) <= end_time);
}

// Main workload Generator
void RunWorkloadThread(TxnManager *manager, ThreadStats *stats, int ops_per_txn,
        int keys_per_txn, int key_max, int seconds_to_run, double get_to_put_ratio,
        size_t value_length, Generator<int> *key_generator) {
    stats->thread_id = this_thread::get_id();

    // Set up operation distribution
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    uniform_real_distribution<double> operation_distribution(0.0, 1.0);

    // Initializing all vector and string values here does all the memory allocation
    // ahead of time. We want there to be no memory management happening inside the
    // main loop of the thread.
    vector<OpDescription> txn_ops(ops_per_txn);
    for (OpDescription &op : txn_ops) {
        op.value.resize(value_length);
    }
        
    seconds s(seconds_to_run);
    nanoseconds duration = duration_cast<nanoseconds>(s);
    nanoseconds total(0);

    do {                
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        
        for (int i = 0; i < ops_per_txn; ++i) {
            double action_chooser = operation_distribution(generator);
            // Modify operation description in place. (Values left around from
            // old insert actions will just be ignored.)
            OpDescription &next_op = txn_ops[i];
	    if (i < keys_per_txn) {
	      next_op.key = key_generator->nextElement();
	    } else {
	      next_op.key = txn_ops[i - keys_per_txn].key;
	    }

            if (action_chooser < get_to_put_ratio) {
                next_op.type = GET;
                ++stats->gets;
            } else {
                next_op.type = INSERT;
                ++stats->inserts;
                GenRandomString(&next_op.value);
            }
	}
        
        // Timed code
        if (!manager->RunTxn(txn_ops, NULL, stats)) {
            global_cout_mutex.lock();
            cerr << "ERROR: transaction failed.";
            global_cout_mutex.unlock();
            return;
	}

        high_resolution_clock::time_point t2 = high_resolution_clock::now();
        nanoseconds gap = duration_cast<nanoseconds>(t2 - t1);
        total += gap;
        ++stats->transactions;
    } while (total <= duration);

    delete key_generator;
}
