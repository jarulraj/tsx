#ifndef _LOCK_TABLE_TXN_MANAGER_CPP_
#define _LOCK_TABLE_TXN_MANAGER_CPP_

#include <iostream>
#include <set>

#include "TxnManagers.h"
                    
bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
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
            mutex *m = new mutex();
            m->lock();
            lockTable[key] = m;
            tableMutex.unlock();
        } else {
            tableMutex.unlock();
            lockTable[key]->lock();
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        lockTable[*rit]->unlock();
    }

    return true;
}        

#endif /* _LOCK_TABLE_TXN_MANAGER_CPP_ */
