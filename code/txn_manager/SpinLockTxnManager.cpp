#include <iostream>
#include <set>

#include "SpinLockTxnManager.h"       

bool SpinLockTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock.
    set<uint64_t> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (uint64_t key : keys) {
	tableMutex.lock();
        if (lockTable.count(key) == 0) {
	    atomic_flag *a = new atomic_flag();
	    lockTable[key] = a;
	    tableMutex.unlock();
            a->test_and_set(memory_order_acquire);
        } else {
	    atomic_flag *a = lockTable[key];
            tableMutex.unlock();
            while (a->test_and_set(memory_order_acquire));
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	tableMutex.lock();
        lockTable[*rit]->clear(memory_order_release);
	tableMutex.unlock();
    }

    return true;
}
 

