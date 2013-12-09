#include <cassert>
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>

#include "TxnManager.h"
#include "LockTableTxnManager.h"

using namespace std;

const int MAX_TRIES = 10;

bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results, ThreadStats *stats) {
    if (dynamic) {
        bool abort = true;
        string result;

        while (abort) {
            set<long> keys;
            unordered_map<long, string> old_values;
            abort = false;
            
            for (const OpDescription &op : operations) {
                if (keys.count(op.key) == 0) {                    
		    mutex *m = &lockTable[op.key];
		    int tries = 0;
		    auto start = std::chrono::high_resolution_clock::now();

		    while (!m->try_lock()) {
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
                lockTable[*rit].unlock();
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
	    TIME_CODE(stats, lockTable[key].lock());
	}

	// Do transaction.
	ExecuteTxnOps(operations, get_results);

	// Unlock all keys in reverse order.
	for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	    lockTable[*rit].unlock();
	}
    }

    return true;
}

