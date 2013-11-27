#include <cassert>
#include <iostream>
#include <set>

#include "SpinLockTxnManager.h"

const int MAX_TRIES = 10;

bool SpinLockTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    if (dynamic) {
	bool abort = true;
	string result;

	while (abort) {
	    set<long> keys;
	    unordered_map<long, string> old_values;
	    abort = false;
	    for (const OpDescription &op : operations) {
		if (keys.count(op.key) == 0) {
		    tableMutex.lock();
		    if (lockTable.count(op.key) == 0) {
			atomic_flag *a = &lockTable[op.key];  // Inserts flag
			a->test_and_set(memory_order_acquire);
			tableMutex.unlock();
		    } else {
			atomic_flag *a = &lockTable[op.key];
			tableMutex.unlock();
			int tries = 0;
			while (a->test_and_set(memory_order_acquire)) {
			    tries++;
			    if (tries == MAX_TRIES) {
				abort = true;
				if (get_results != NULL) {
				    get_results->clear();
				}
				break;
			    }
			}

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
		unordered_map<long, string>::iterator it;
		OpDescription op;
		op.type = INSERT;
		for (it = old_values.begin(); it != old_values.end(); it++) {
		    op.key = it->first;
		    op.value = it->second;
		    ExecuteTxnOp(op);
		}
	    }

	    // Unlock all keys in reverse order.
	    std::set<long>::reverse_iterator rit;
	    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
		tableMutex.lock();
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
	    tableMutex.lock();
	    if (lockTable.count(key) == 0) {
		atomic_flag *a = &lockTable[key];  // Creates flag
		tableMutex.unlock();
		a->test_and_set(memory_order_acquire);
	    } else {
		atomic_flag *a = &lockTable[key];
		tableMutex.unlock();
		while (a->test_and_set(memory_order_acquire));
	    }
	}

	// Do transaction.
	ExecuteTxnOps(operations, get_results);

	// Unlock all keys in reverse order.
	std::set<long>::reverse_iterator rit;
	for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	    tableMutex.lock();
	    lockTable[*rit].clear(memory_order_release);
	    tableMutex.unlock();
	}
    }

    return true;
}
 

