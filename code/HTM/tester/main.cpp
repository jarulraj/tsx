/*
 * main.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: jdunietz
 */

#include "../hash_tbl/hash_table.h"
#include "../txn_mgr/TxnManagers.h"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <thread>

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

// It probably doesn't really matter how long the value strings are...
const int VALUE_LENGTH = 10;
const int MAX_KEY = 10000;

void RunWorkloadThread(TxnManager *manager, int ops_per_txn, int txn_period_ms,
        int key_max, int seconds_to_run, double p_insert, double p_get,
        double p_delete) {
    double p_sum = p_insert + p_get + p_delete;
    double get_threshold = p_insert + p_get;

    default_random_engine generator;
    // Use p_sum to effectively make sure probabilities are normalized.
    uniform_real_distribution<double> operation_distribution(0.0, p_sum);
    uniform_int_distribution<int> key_distribution(0, key_max);

    // Initializing all vector and string values here essentially eliminates
    // contention for the STL allocator locks. We want there to be no memory
    // management happening inside the main loop of the thread.
    vector<OpDescription> txn_ops(ops_per_txn);
    for (OpDescription &op : txn_ops) {
        op.value.resize(VALUE_LENGTH);
    }

    time_t end_time = time(NULL) + seconds_to_run;
    do {
        for (int i = 0; i < ops_per_txn; ++i) {
            double action_chooser = operation_distribution(generator);
            // Modify operation description in place. (Values left around from
            // old insert actions will just be ignored.)
            OpDescription &next_op = txn_ops[i];
            next_op.key = key_distribution(generator);

            if (action_chooser < p_insert) {
                next_op.type = INSERT;
                GenRandomString(&next_op.value);
            } else if (action_chooser < get_threshold) {
                next_op.type = GET;
            } else {
                next_op.type = DELETE;
            }
        }

        manager->RunTxn(txn_ops);
    } while (time(NULL) <= end_time);
}

const char *INVALID_MSG =
            "Invalid arguments. Usage: htm-test [htm | pess] <num_threads>";
const char *HTM_TYPE = "htm";
const char *PESSIMISTIC_TYPE = "pess";

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        cerr << INVALID_MSG;
        exit(1);
    }

    string num_threads_str = argv[2];
    for (const char c : num_threads_str) {
        if (!isdigit(c)) {
            cerr << INVALID_MSG;
            exit(1);
        }
    }
    int num_threads = atoi(num_threads_str.c_str());

    string manager_type = argv[1];
    TxnManager *manager;
    HashTable table; // TODO: make this properly construct a HashTable

    // Make sure all the keys we'll be using are there so GETs don't fail
    for (uint64 i = 0; i <= MAX_KEY; ++i) {
        table.Insert(i, "");
    }

    if (manager_type == HTM_TYPE) {
        manager = new HTMTxnManager(&table);
    } else if(manager_type == PESSIMISTIC_TYPE) {
        manager = new PessimisticTxnManager(&table);
    } else {
        cerr << INVALID_MSG;
        exit(1);
    }

    vector<thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(
                thread(RunWorkloadThread, manager, 10, 1, MAX_KEY, 10, 1 / 3.0,
                        1 / 3.0, 1 / 3.0));
    }

    // TODO: Is this necessary? If main() exits and threads are still running,
    // does the program keep running?
    for (thread &t : threads) {
        t.join();
    }

    return 0;
}
