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
//#include "HLETxnManager.h"
//#include "LockTableTxnManager.h"
//#include "RTMTxnManager.h"
#include "SpinLockTxnManager.h"
#include "SpinLockSimpleTxnManager.h"
#include "TxnManager.h"

#include "cmdline-utils.h"
#include "workload.h"

using namespace std;
using namespace std::chrono;

// Stupid dummy function to make threading library link properly
void pause_thread(int n) {
  this_thread::sleep_for(seconds(n));
}

int main(int argc, const char* argv[]) {
    argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present
    option::Stats stats(usage, argc, argv);
    option::Option* options = new option::Option[stats.options_max];
    option::Option* buffer  = new option::Option[stats.buffer_max];
    option::Parser parse(usage, argc, argv, options, buffer);

    if (parse.error()) {
        // Error message has been output by the checker functions during
        // command line parsing.
        option::printUsage(std::cout, usage);
        return 1;
    }

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    if (parse.nonOptionsCount() != 1) {
        if (parse.nonOptionsCount() > 1) {
            cerr << "Too many non-option command line arguments" << endl;
        } else {
            cerr << "Missing concurrency control type argument" << endl;
        }
        option::printUsage(std::cout, usage);
        return 1;
    }

    string manager_type = parse.nonOption(0);
    string key_dist_type = getArgWithDefault(options, KEY_DIST, DEFAULT_DIST_NAME);
    int num_seconds_to_run = getArgWithDefault(options, NUM_SECONDS, DEFAULT_SECONDS);
    int num_keys = getArgWithDefault(options, NUM_KEYS, DEFAULT_KEYS);
    int value_length = getArgWithDefault(options, VALUE_LENGTH, DEFAULT_VALUE_LENGTH);
    int verbosity = getArgWithDefault(options, VERBOSITY, DEFAULT_VERBOSITY);
    bool sanity_test = options[SANITY_TEST];
    bool dynamic = options[DYNAMIC];
    int num_threads;
    int ops_per_txn;
    double ratio;
    if (!sanity_test) {
        num_threads = getArgWithDefault(options, NUM_THREADS, thread::hardware_concurrency());
        ops_per_txn = getArgWithDefault(options, OPS_PER_TXN, DEFAULT_OPS_PER_TXN);
        ratio = option::getRatio(options[RATIO]);
        if (std::isnan(ratio)) {
            // The ratio should have already been checked by the option parser.
            cerr << "WARNING: We should never have gotten here!" << endl;
            return 1;
        }
    }

    delete[] options;
    delete[] buffer;


    // Initialize hashtable
    HashTable table(static_cast<ht_flags>(HT_KEY_CONST | HT_VALUE_CONST), 0.05);

    // Use std::unordered_map as hashtable 
    // Making the underlying hashtable concurrency-safe is a different problem
    //unordered_map<long, string> table;


    TxnManager *manager;
    if (manager_type == HLE_NAME) {
      //manager = new HLETxnManager(&table);
    } else if (manager_type == RTM_NAME) {
      //manager = new RTMTxnManager(&table);
    } else if (manager_type == LOCK_TABLE_NAME) {
      //manager = new LockTableTxnManager(&table, dynamic);
    } else if (manager_type == SPIN_NAME) {
        manager = new SpinLockTxnManager(&table, dynamic);
    } else if (manager_type == SPIN_SIMPLE_NAME) {
        manager = new SpinLockSimpleTxnManager(&table);
    } else {
        cerr << "Invalid concurrency control mechanism: " << manager_type << endl;
        option::printUsage(std::cout, usage);
        return 1;
    }

    if (key_dist_type != UNIFORM_NAME && key_dist_type != ZIPF_NAME) {
        cerr << "Invalid key distribution type: " << key_dist_type << endl;
        option::printUsage(std::cout, usage);
        return 1;
    }

    // Make sure all the keys we'll be using are there so GETs don't fail
    for (long i = 0; i < num_keys; ++i) {
        string istr = std::to_string(i) ;
        //std::pair<long,std::string> entry (i,istr);
        //table.insert(entry);
	table.Insert(i, istr);
    }

    //for (auto& x: table)
    //    std::cout << x.first << ": " << x.second << std::endl;

    vector<thread> threads;
    vector<ThreadStats> thread_stats;

    if (verbosity >= 1) {
	cout << "Time (s):     " << num_seconds_to_run << endl;
	cout << "Num keys:     " << num_keys << endl;
	cout << "Value length: " << value_length << endl;
	cout << "CC type:      " << manager_type << endl;
    }

    if (sanity_test) {
        // Each transaction reads keys multiple times. It's pretty likely that a writer
        // will write to the value that's being read during the time of the transaction,
        // so we should notice non-repeatable reads if concurrency control is failing.
        cout << "Running sanity check with 1 reader/writer thread"
                " and 2 multi-key reader/writer threads"
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
	if (verbosity >= 1) {
	    cout << "Num threads:  " << num_threads << endl;
	    cout << "Ops per txn:  " << ops_per_txn << endl;
	    cout << "Ratio:        " << ratio << endl;
	    cout << "Key distrib:  " << key_dist_type << endl;
	}

        thread_stats.resize(num_threads);
        for (int i = 0; i < num_threads; ++i) {
            // Each thread gets its own key generator.
            Generator<int> *key_generator;
            time_t seed = time(NULL);
            if (key_dist_type == ZIPF_NAME) {
                key_generator = new ZipfianGenerator(0, num_keys - 1, seed);
            } else { // UNIFORM_NAME
                key_generator = new UniformGenerator(0, num_keys - 1);
            }
            threads.push_back(
                    thread(RunWorkloadThread, manager, &thread_stats[i], ops_per_txn, num_keys-1,
                        num_seconds_to_run, ratio, value_length, key_generator));
        }
    }

    for (thread &t : threads) {
        t.join();
    }

    ThreadStats overall;

	// Shouldn't need cout mutex anymore here -- all threads are done.
    for (const ThreadStats &stats : thread_stats) {
	if (verbosity >= 2) {
	    cout << "Thread " << stats.thread_id << ":\n"
		 << "  Transactions: " << stats.transactions << "\n"
		 << "  GETs: " << stats.gets << "\n"
		 << "  INSERTs: " << stats.inserts << "\n"
		 << "  Contention time: " << duration_cast<nanoseconds>(overall.contention_time).count()
		     << " ns" << endl;
	}

        overall.transactions += stats.transactions;
        overall.gets += stats.gets;
        overall.inserts += stats.inserts;
        overall.contention_time += stats.contention_time;
    }

    cout << "--------------------------------------------"<<endl;
    cout << "Overall stats:\n"
        << "  Total transactions: " << overall.transactions << "\n"
        << "  GETs: " << overall.gets << "\n"
        << "  INSERTs: " << overall.inserts << "\n"
        << "  Contention time: " << duration_cast<nanoseconds>(overall.contention_time).count()
            << " ns" << endl;

    
#ifdef DEBUG    
    if (manager_type == RTM_NAME)
        manager->printStats();
#endif  

    return 0;
}
