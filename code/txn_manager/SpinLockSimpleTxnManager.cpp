#include <cassert>
#include <iostream>
#include <set>

#include "SpinLockSimpleTxnManager.h"
#include "tester/workload.h"

using namespace std;
 
bool SpinLockSimpleTxnManager::RunTxn(const vector<OpDescription> &operations,
        vector<string> *get_results, ThreadStats *stats) {

    TIME_CODE(stats, pthread_spin_lock(&table_lock));

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    pthread_spin_unlock(&table_lock);

    return true;
}
