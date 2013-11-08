#ifndef _TXN_MANAGER_CPP_
#define _TXN_MANAGER_CPP_

#include <iostream>
#include <set>

#include "TxnManager.h"

using namespace std;

bool TxnManager::ExecuteTxnOps(const vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    for (const OpDescription &op : operations) {
        if (op.type == INSERT) {
            table_->Insert(op.key, op.value);
        } 
        else if (op.type == GET) {
            string result;
            if (!table_->Get(op.key, &result)) {
                return false;
            }
      
            if (get_results != NULL) {
                get_results->push_back(result);
            }
        } 
        else { // op.type == DELETE
            table_->Delete(op.key);
        }
    }

    return true;
}

void TxnManager::getStats(){
    std:cout<<"Print Stats"<<std::endl;
}

#endif /* _TXN_MANAGER_CPP */
