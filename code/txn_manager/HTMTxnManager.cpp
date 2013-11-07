#ifndef _HTM_TXN_MANAGER_CPP_
#define _HTM_TXN_MANAGER_CPP_

#include <iostream>
#include <set>

#include "HTMTxnManager.h"       
#include "rtm.h"

HTMTxnManager::HTMTxnManager(HashTable *table)
    : TxnManager(table) {
               
        int rtm = cpu_has_rtm();
        int hle = cpu_has_hle();
        printf("TSX HLE: %s\nTSX RTM: %s\n", hle ? "YES" : "NO", rtm ? "YES" : "NO");

        if(rtm == 0){
            cout<<"RTM not found on machine "<<endl;
            exit(-1);
        }
    }

bool HTMTxnManager::RunTxn(const std::vector<OpDescription> &operations,
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



#endif /* _HTM_TXN_MANAGER_CPP_ */
