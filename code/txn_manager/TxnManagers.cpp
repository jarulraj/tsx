/*
 * TxnManagers.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: jdunietz
 */

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
