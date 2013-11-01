/*
 * TxnManagers.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: jdunietz
 */

#include <set>

#include "TxnManagers.h"

using namespace std;

bool TxnManager::ExecuteTxnOps(const vector<OpDescription> &operations) {
    for (const OpDescription &op : operations) {
        if (op.type == INSERT) {
            //table_->Insert(op.key, op.value);
        } else if (op.type == GET) {
            // TODO: Should we do something with the value here?
            string result;
            //if (!table_->Get(op.key, &result)) {
            //    return false;
            //}
        } else { // op.type == DELETE
            //table_->Delete(op.key);
        }
    }

    return true;
}

// Implementations of other transaction managers go here.

bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations) {
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
    ExecuteTxnOps(operations);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	lockTable[*rit]->unlock();
    }

    return true;
}
