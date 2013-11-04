#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <mutex>
#include <thread>

#include "hashtable.h"
#include "TxnManagers.h"

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
constexpr int NUM_KEYS = 10;

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
        // NOTE: THIS IS A TERRIBLE HACK. This is only safe because we resized
        // this buffer above. The only reason to tolerate this sort of code is
        // that it allows reusing the buffer without extra reallocations or
        // copies.
        snprintf(const_cast<char*>(ops[0].value.c_str()), VALUE_LENGTH, "%d",
                txn_counter);
        manager->RunTxn(ops, NULL);
        key = (key + 1) % max_key;
        ++txn_counter;
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

        // We know there must be at least one result, because num_reads > 0.
        auto result_iter = get_results.begin();
        const string &result = *result_iter;
        for (; result_iter != get_results.end(); ++result_iter) {
            const string &next_result = *result_iter;
            if (result != next_result) {
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
    cout << "Read transactions:  " << txn_counter << endl;
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

const char *HTM_TYPE = "htm";
const char *LOCK_TABLE_TYPE = "locktbl";
const char *SPIN_LOCK_TYPE = "spinlock";
const string INVALID_MSG = string("Invalid arguments. Usage: htm-test [")
        + HTM_TYPE + " | " + LOCK_TABLE_TYPE + " | " + SPIN_LOCK_TYPE
        + "] <num_threads> \n";

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

    // Initialize hashtable
    HashTable table(static_cast<ht_flags>(HT_KEY_CONST | HT_VALUE_CONST), 0.05);
    TxnManager *manager;
    if (manager_type == HTM_TYPE) {
        //manager = new HTMTxnManager(&table);
    } else if(manager_type == LOCK_TABLE_TYPE) {
        manager = new LockTableTxnManager(&table);
    } else if(manager_type == SPIN_LOCK_TYPE) {
        // TODO: Add spin lock manager
    } else {
        cerr << INVALID_MSG;
        exit(1);
    }

    table.display();
    // Make sure all the keys we'll be using are there so GETs don't fail
    std::string initvalue = "test";
    for (uint64_t i = 0; i < NUM_KEYS; ++i) {
        table.Insert(i,initvalue);
    }

    cout << "Keys inserted: " << table.GetSize() << endl;
    table.display();

    vector<thread> threads;
    threads.push_back(thread(RunTestWriterThread, manager, NUM_KEYS, 10));
    // Each transaction reads twice as many times as there are keys. Since the
    // writer thread is cycling through keys one per transaction, it's pretty
    // likely that it'll write to the value that's being read during the time of
    // the transaction, so we should notice non-repeatable reads if concurrency
    // control is failing.
    threads.push_back(thread(RunTestReaderThread, manager, NUM_KEYS, 10 * NUM_KEYS, 10));
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

    return 0;
}
