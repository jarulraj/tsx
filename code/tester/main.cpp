#ifndef _MAIN_CPP_
#define _MAIN_CPP_

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
constexpr int NUM_KEYS = 1024;

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
void RunTestReaderWriterThread(TxnManager *manager, uint64_t max_key, int num_reads,
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
    uint64_t key = 0;
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

void RunTestMultiKeyThread(TxnManager *manager, uint64_t max_key, int num_ops,
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
    uint64_t key = 0;
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
    default_random_engine generator;
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

#define STR_VALUE(arg)      #arg
#define STRINGIFY(arg)      STR_VALUE(arg) /* Weird macro magic */
#define DEFAULT_SECONDS     10
#define DEFAULT_OPS_PER_TXN 10
#define HLE_NAME            "hle"
#define RTM_NAME            "rtm"
#define SPIN_NAME           "spin"
#define LOCK_TABLE_NAME     "tbl"
enum  optionIndex {UNKNOWN, HELP, NUM_THREADS, NUM_SECONDS, OPS_PER_TXN, RATIO};
const option::Descriptor usage[] =
{
 {UNKNOWN,     0, "" , "",        option::Arg::None,     "Usage: htm-test [options] "
                                                         HLE_NAME "|" RTM_NAME "|" LOCK_TABLE_NAME "|" SPIN_NAME "\n\n"
                                                         "Options:" },
 {HELP,        0, "" , "help",    option::Arg::None,     "  --help  \tPrint usage and exit." },
 {NUM_THREADS, 0, "t", "threads", option::Arg::Integer,  "  --threads, -t  \tNumber of threads to run with."
                                                         " Default: max supported by hardware." },
 {NUM_SECONDS, 0, "s", "seconds", option::Arg::Integer,  "  --seconds, -s  \tNumber of seconds to run for."
                                                         " Default: " STRINGIFY(DEFAULT_SECONDS) "." },
 {OPS_PER_TXN, 0, "o", "txn_ops", option::Arg::Integer,  "  --txn_ops, -o  \tOperations per transaction."
                                                         " Default: " STRINGIFY(DEFAULT_OPS_PER_TXN) "." },
 {RATIO,      0,  "r", "ratio",   option::Arg::Required, "  --ratio,   -r  \tRatio of gets to puts in each transaction,"
                                                         " in the format gets:puts. Default: 1:1." },
 {0,0,0,0,0,0}
};

inline int getArgWithDefault(const option::Option *options, optionIndex index, int defaultVal) {
    if (options[index]) {
        return atoi(options[index].arg);
    } else {
        return defaultVal;
    }
}

double getRatio(const option::Option *options) {
    if (!options[RATIO]) {
        return 1.0;
    }

    const char *arg = options[RATIO].arg;
    istringstream argStream(arg);
    int gets;
    int inserts;
    argStream >> gets;
    if (argStream.bad()) {
        return nan("");
    }
    if (argStream.get() != ':') {
        return nan("");
    }
    argStream >> inserts;
    if (argStream.bad()) {
        return nan("");
    }
    return gets / static_cast<double>(inserts);
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
    int num_threads = getArgWithDefault(options, NUM_THREADS, thread::hardware_concurrency());
    int num_seconds_to_run = getArgWithDefault(options, NUM_SECONDS, DEFAULT_SECONDS);
    int ops_per_txn = getArgWithDefault(options, OPS_PER_TXN, DEFAULT_OPS_PER_TXN);
    double ratio = getRatio(options);
    if (std::isnan(ratio)) {
        cerr << "Ratio must be in the format <int>:<int>" << endl;
        return 1;
    }
    
    delete[] options;
    delete[] buffer;


    // Initialize hashtable
    HashTable table(static_cast<ht_flags>(HT_KEY_CONST | HT_VALUE_CONST), 0.05);
    TxnManager *manager;
    if (manager_type == HLE_NAME) {
        manager = new HLETxnManager(&table);
    }else if (manager_type == RTM_NAME) {
        manager = new RTMTxnManager(&table);
    } else if(manager_type == LOCK_TABLE_NAME) {
        manager = new LockTableTxnManager(&table);
    } else if(manager_type == SPIN_NAME) {
        manager = new SpinLockTxnManager(&table);
    } else {
        option::printUsage(std::cout, usage);
        return 1;
    }

    //table.display();
    // Make sure all the keys we'll be using are there so GETs don't fail
    for (uint64_t i = 0; i < NUM_KEYS; ++i) {
        table.Insert(i, std::to_string(i));
    }

    cout << "Keys inserted: " << table.GetSize() << endl;
    //table.display();

    vector<thread> threads;
    //threads.push_back(thread(RunMultiKeyThread, manager, NUM_KEYS, num_seconds_to_run));

    /*
    // SANITY-CHECKING CODE
    //
    // Each transaction reads keys multiple times. Since the
    // writer thread is cycling through keys one per transaction, it's pretty
    // likely that it'll write to the value that's being read during the time of
    // the transaction, so we should notice non-repeatable reads if concurrency
    // control is failing.
    threads.push_back(thread(RunMultiKeyThread, manager, NUM_KEYS, 10 * NUM_KEYS, num_seconds_to_run));
    threads.push_back(thread(RunMultiKeyThread, manager, NUM_KEYS, 10 * NUM_KEYS, num_seconds_to_run));
    //threads.push_back(thread(RunTestReaderWriterThread, manager, NUM_KEYS, 10 * NUM_KEYS, num_seconds_to_run));
    */

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(
                thread(RunWorkloadThread, manager, ops_per_txn, NUM_KEYS,
                        num_seconds_to_run, ratio));
    }

    for (thread &t : threads) {
        t.join();
    }

    if(manager_type == HLE_NAME){
        manager->getStats();
    }

    return 0;
}

#endif /* _MAIN_CPP_ */
