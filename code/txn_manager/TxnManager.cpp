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
    for (const OpDescription &op : operations) {
        if (op.type == INSERT) {
            (*table_)[op.key] = op.value;
        } 
        else if (op.type == GET) {
            string result;
            //at function throws an out_of_range exception if key not found
            result = table_->at(op.key);

            if (get_results != NULL) {
                get_results->push_back(result);
            }
        } 
        else { // op.type == DELETE
            table_->erase(op.key);
        }
    }

    return true;
}

#endif /* _TXN_MANAGER_CPP */
