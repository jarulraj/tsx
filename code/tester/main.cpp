#ifndef _MAIN_CPP_
#define _MAIN_CPP_

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <mutex>
#include <thread>

#include "hashtable.h"
#include "TxnManager.h"

#include "RTMTxnManager.h"
#include "HLETxnManager.h"
#include "SpinLockTxnManager.h"
#include "LockTableTxnManager.h"

using namespace std;

std::mutex global_cout_mutex;

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

// Stupid dummy function to make threading library link properly
void pause_thread(int n) {
  std::this_thread::sleep_for(std::chrono::seconds(n));
}

// It probably doesn't really matter how long the value strings are...
constexpr int VALUE_LENGTH = 10;
constexpr int NUM_KEYS = 16;

// This thread performs a large number of single-operation transactions. It
// cycles through all possible keys, with each transaction writing to the next
// key.
void RunTestWriterThread(TxnManager *manager, uint64_t max_key,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(1);
    ops[0].value.resize(VALUE_LENGTH);

    uint64_t key = 0;
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
void RunTestReaderThread(TxnManager *manager, uint64_t max_key, int num_reads,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads);

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    uint64_t key = 0;
    int txn_counter = 0;
    do {
        for (int i = 0; i < num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        if (!manager->RunTxn(ops, &get_results)) {
            cerr << "ERROR: transaction failed.";
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
            if (result != next_result) {
                cerr << "ERROR: all reads should have returned '" << result
                        << "'; got '" << next_result << "' instead" << endl;
                break;
            }
        }

        get_results.clear();  // Doesn't actually change allocation
        key = (key + 1) % max_key;
        ++txn_counter;
        //cout << "Read Txn End : " << txn_counter << endl;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    cout << "Read transactions:  " << txn_counter << endl;
    global_cout_mutex.unlock();
}

// This thread performs a series of transactions in which the same key is
// written and then read num_reads times. It complains if the reads do not
// return the written value. Like the other threads, it cycles through keys.
// This is to test writer/writer conflicts for the read/write lock manager.
void RunTestReaderWriterThread(TxnManager *manager, uint64_t max_key, int num_reads,
        int seconds_to_run) {
    time_t end_time = time(NULL) + seconds_to_run;
    vector<OpDescription> ops(num_reads + 1);
    ops[0].value.resize(VALUE_LENGTH);

    if (num_reads <= 0) {
        return;
    }

    vector<string> get_results;
    get_results.reserve(num_reads);
    uint64_t key = 0;
    int txn_counter = 0;
    do {
        ops[0].type = INSERT;
        ops[0].key = key;
        ops[0].value = std::to_string(txn_counter);
        
        for (int i = 1; i <= num_reads; ++i) {
            ops[i].type = GET;
            ops[i].key = key;
        }
        if (!manager->RunTxn(ops, &get_results)) {
            cerr << "ERROR: transaction failed.";
            return;
        }

        // We know there must be at least one result, because num_reads > 0.
        auto result_iter = get_results.begin();
        const string &result = ops[0].value;
        for (; result_iter != get_results.end(); ++result_iter) {
            const string &next_result = *result_iter;
            if (result.compare(next_result) == 0) {
                cerr << "ERROR: all reads should have returned '" << result
                    << "'; got '" << next_result << "' instead" << endl;
                break;
            }
        }

        get_results.clear();  // Doesn't actually change allocation
        key = (key + 1) % max_key;
        ++txn_counter;
    } while (time(NULL) <= end_time);

    global_cout_mutex.lock();
    global_cout_mutex.unlock();
}

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

        if (!manager->RunTxn(txn_ops, NULL)) {
            cerr << "ERROR: transaction failed.";
            return;
        }
    } while (time(NULL) <= end_time);
}


const int HLE_TYPE = 0;
const int RTM_TYPE = 1;
const int LOCK_TABLE_TYPE = 2;
const int SPIN_LOCK_TYPE = 3;
const string INVALID_MSG = string("Invalid arguments. Usage: htm-test <manager_type>[ ")
+ std::to_string(HLE_TYPE)        + " (hle) | " 
+ std::to_string(RTM_TYPE)        + " (rtm) | " 
+ std::to_string(LOCK_TABLE_TYPE) + " (locktbl) | " 
+ std::to_string(SPIN_LOCK_TYPE)  + " (spinlock) ] " 
+" <num_threads> <num_seconds_to_run>\n";

void checkArg(const char* arg){
    string str = arg;
    for (const char c : str) {
        if (!isdigit(c)) {
            cerr << INVALID_MSG;
            exit(1);
        }
    }
}

int main(int argc, const char* argv[]) {
    if (argc != 4) {
        cerr << INVALID_MSG;
        exit(1);
    }

    checkArg(argv[1]);
    checkArg(argv[2]);
    checkArg(argv[3]);

    int manager_type = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    int num_seconds_to_run = atoi(argv[3]);

    // Initialize hashtable
    HashTable table(static_cast<ht_flags>(HT_KEY_CONST | HT_VALUE_CONST), 0.05);
    TxnManager *manager;
    if (manager_type == HLE_TYPE) {
        manager = new HLETxnManager(&table);
    }else if (manager_type == RTM_TYPE) {
        manager = new RTMTxnManager(&table);
    } else if(manager_type == LOCK_TABLE_TYPE) {
        manager = new LockTableTxnManager(&table);
    } else if(manager_type == SPIN_LOCK_TYPE) {
        manager = new SpinLockTxnManager(&table);
    } else {
        cerr << INVALID_MSG;
        exit(1);
    }

    table.display();
    // Make sure all the keys we'll be using are there so GETs don't fail
    for (uint64_t i = 0; i < NUM_KEYS; ++i) {
        table.Insert(i, std::to_string(i));
    }

    cout << "Keys inserted: " << table.GetSize() << endl;
    table.display();

    vector<thread> threads;
    threads.push_back(thread(RunTestWriterThread, manager, NUM_KEYS, num_seconds_to_run));
    // Each transaction reads keys multiple times. Since the
    // writer thread is cycling through keys one per transaction, it's pretty
    // likely that it'll write to the value that's being read during the time of
    // the transaction, so we should notice non-repeatable reads if concurrency
    // control is failing.
    threads.push_back(thread(RunTestReaderThread, manager, NUM_KEYS, 10 * NUM_KEYS, num_seconds_to_run));
    //threads.push_back(thread(RunTestReaderWriterThread, manager, NUM_KEYS, 10 * NUM_KEYS, num_seconds_to_run));
    /*
       for (int i = 0; i < num_threads; ++i) {
       threads.push_back(
       thread(RunWorkloadThread, manager, 10, 1, MAX_KEY, 10, 1 / 3.0,
       1 / 3.0, 1 / 3.0));
       }
       */

    for (thread &t : threads) {
        t.join();
    }

    if(manager_type == HLE_TYPE){
        manager->getStats();
    }

    return 0;
}

#endif /* _MAIN_CPP_ */
