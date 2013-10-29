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

inline string GenRandomString(int length) {
    static const char ALPHANUM_CHARS[] = "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    string result(length, '\0');
    for (int i = 0; i < length; ++i) {
        result[i] = ALPHANUM_CHARS[rand() % (sizeof(ALPHANUM_CHARS) - 1)];
    }

    return result;
}

// It probably doesn't really matter how long the value strings are...
const int VALUE_LENGTH = 10;

void RunWorkloadThread(TxnManager *manager, int ops_per_txn, int txn_period_ms,
        int key_max, int seconds_to_run, double p_insert, double p_get,
        double p_delete) {
    double p_sum = p_insert + p_get + p_delete;
    double get_threshold = p_insert + p_get;

    default_random_engine generator;
    // Use p_sum to effectively make sure probabilities are normalized.
    uniform_real_distribution<double> normed_distribution(0.0, p_sum);
    uniform_int_distribution<int> key_distribution(0, key_max);

    time_t end_time = time(NULL) + seconds_to_run;
    do {
        vector<OpDescription> txn_ops;
        for (int i = 0; i < ops_per_txn; ++i) {
            double action_chooser = normed_distribution(generator);
            OpDescription next_op;
            next_op.key = key_distribution(generator);

            if (action_chooser < p_insert) {
                next_op.type = INSERT;
                next_op.value = GenRandomString(VALUE_LENGTH);
            } else if (action_chooser < get_threshold) {
                next_op.type = GET;
            } else {
                next_op.type = DELETE;
            }
            txn_ops.push_back(next_op);
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
                thread(RunWorkloadThread, manager, 10, 1, 10000, 10, 1 / 3.0,
                        1 / 3.0, 1 / 3.0));
    }

    // TODO: Is this necessary? If main() exits and threads are still running,
    // does the program keep running?
    for (thread &t : threads) {
        t.join();
    }

    return 0;
}
