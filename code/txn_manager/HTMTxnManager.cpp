#ifndef _HTM_TXN_MANAGER_CPP_
#define _HTM_TXN_MANAGER_CPP_

#include <iostream>
#include <set>

#include "HTMTxnManager.h"       

bool HTMTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {

    __transaction_relaxed {
        // Do transaction.
        ExecuteTxnOps(operations, NULL);
    }


    return true;
}



#endif /* _HTM_TXN_MANAGER_CPP_ */
