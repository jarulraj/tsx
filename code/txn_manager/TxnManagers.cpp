/*
 * TxnManagers.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: jdunietz
 */

#include <set>

#include "TxnManagers.h"

using namespace std;

bool TxnManager::ExecuteTxnOps(const vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    for (const OpDescription &op : operations) {
        if (op.type == INSERT) {
            table_->Insert((void*)&op.key, sizeof(op.key), (void*)op.value.c_str(), (op.value.length()+1) * sizeof(char));
        } else if (op.type == GET) {
            string result;
            //if (!table_->Get(op.key, &result)) {
            //    return false;
            //}
            if (get_results != NULL) {
                get_results->push_back(result);
            }
        } else { // op.type == DELETE
            //table_->Delete(op.key);
        }
    }

    return true;
}

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
