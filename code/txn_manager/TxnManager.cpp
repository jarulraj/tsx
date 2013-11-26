#ifndef _TXN_MANAGER_CPP_
#define _TXN_MANAGER_CPP_

#include <iostream>
#include <set>
#include <unordered_map>
#include <utility> 

#include "TxnManager.h"

using namespace std;

bool TxnManager::ExecuteTxnOps(const vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    string result;
    for (const OpDescription &op : operations) {
	result = ExecuteTxnOp(op);
	if (op.type == GET && get_results != NULL) {
	    get_results->push_back(result);
	}
    }

    return true;
}

string TxnManager::ExecuteTxnOp(const OpDescription &op)
{
    string result;
    if (op.type == INSERT) {
	result = (*table_)[op.key];

	// Overwrite value if already present.
	(*table_)[op.key] = op.value;
    }
    else if (op.type == GET) {
	//at function throws an out_of_range exception if key not found
	result = table_->at(op.key);
    }
    else { // op.type == DELETE
	result = (*table_)[op.key];

	table_->erase(op.key);
    }

    return result;
}

#endif /* _TXN_MANAGER_CPP */
