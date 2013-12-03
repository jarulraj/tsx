#include <cassert>
#include <iostream>
#include <set>
#include <pthread.h>

#include "SpinLockSimpleTxnManager.h"

using namespace std;
 
pthread_spinlock_t SpinLockSimpleTxnManager::table_lock ;

SpinLockSimpleTxnManager::SpinLockSimpleTxnManager(std::unordered_map<long,std::string>* table)
    : TxnManager(table) {

        pthread_spin_init(&table_lock, PTHREAD_PROCESS_PRIVATE);

    }


bool SpinLockSimpleTxnManager::RunTxn(const vector<OpDescription> &operations,
        vector<string> *get_results) {


    pthread_spin_lock(&table_lock);

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    pthread_spin_unlock(&table_lock);

    return true;
}


