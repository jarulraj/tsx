#include <cassert>
#include <iostream>
#include <set>

#include "SpinLockTxnManager.h"
#include "tester/workload.h"

using namespace std;

const int MAX_TRIES = 10;

bool SpinLockTxnManager::RunTxn(const vector<OpDescription> &operations,
        vector<string> *get_results, ThreadStats *stats) {
    if (dynamic) {
	bool abort = true;
	string result;

	while (abort) {
	    set<long> keys;
	    unordered_map<long, string> old_values;
	    abort = false;
	    for (const OpDescription &op : operations) {
		if (keys.count(op.key) == 0) {
		    TIME_CODE(stats, tableMutex.lock());
		    if (lockTable.count(op.key) == 0) {
			atomic_flag *a = &lockTable[op.key];  // Inserts flag
			TIME_CODE(stats, a->test_and_set(memory_order_acquire));
                        tableMutex.unlock();
		    } else {
			atomic_flag *a = &lockTable[op.key];
			tableMutex.unlock();
			int tries = 0;
		        auto start = std::chrono::high_resolution_clock::now();
			while (a->test_and_set(memory_order_acquire)) {
                            auto end = std::chrono::high_resolution_clock::now();
                            stats->lock_acq_time += end - start;

			    tries++;
			    if (tries == MAX_TRIES) {
				abort = true;
				if (get_results != NULL) {
				    get_results->clear();
				}
				break;
			    }

			    // Prepare timing info for next iteration.
			    start = std::chrono::high_resolution_clock::now();
			}

			auto end = std::chrono::high_resolution_clock::now();
                        stats->lock_acq_time += end - start;

			if (abort) {
			    break;
			}
		    }
		    keys.insert(op.key);
		}

		// Run this op.
		ExecuteTxnOp(op, &result);
		if (op.type == GET) {
		    if (get_results != NULL) {
			get_results->push_back(result);
		    }
		} else if (old_values.count(op.key) == 0) {
		    old_values[op.key] = result;
		}
	    }

	    // Roll back changes if we aborted.
	    if (abort) {
		OpDescription op;
		op.type = INSERT;
		for (auto it = old_values.begin(); it != old_values.end(); it++) {
		    op.key = it->first;
		    op.value = it->second;
		    ExecuteTxnOp(op);
		}
	    }

	    // Unlock all keys in reverse order.
	    for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
		TIME_CODE(stats, tableMutex.lock());
		lockTable[*rit].clear(memory_order_release);
		tableMutex.unlock();
	    }
	}
    } else {
	// Construct an ordered set of keys to lock.
	set<long> keys;
	for (const OpDescription &op : operations) {
	    keys.insert(op.key);
	}

	// Lock keys in order.
	for (long key : keys) {
	  atomic_flag *a = &lockTable[key];
	  TIME_CODE(stats, while (a->test_and_set(memory_order_acquire)));
	}

	// Do transaction.
	ExecuteTxnOps(operations, get_results);

	// Unlock all keys in reverse order.
	for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	    atomic_flag *a = &lockTable[*rit];
	    a->clear(memory_order_release);
	}
    }

    return true;
}
 

