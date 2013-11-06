#ifndef _SPIN_LOCK_TXN_MANAGER_CPP_
#define _SPIN_LOCK_TXN_MANAGER_CPP_

#include <iostream>
#include <set>

#include "TxnManagers.h"       

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
            a->test_and_set(memory_order_acquire);
            lockTable[key] = a;
            tableMutex.unlock();
        } else {
            tableMutex.unlock();
            while (lockTable[key]->test_and_set(memory_order_acquire));
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        lockTable[*rit]->clear(memory_order_release);
    }
    
    return true;
}
 


#endif /* _SPIN_LOCK_TXN_MANAGER_CPP_ */
