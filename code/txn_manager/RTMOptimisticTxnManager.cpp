#include <iostream>
#include <set>

#include "RTMOptimisticTxnManager.h"
#include "tester/workload.h"

bool RTMOptimisticTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results, ThreadStats *stats) {

    //TIME_CODE(stats, rtm_mutex_acquire(&table_lock));
    bool noLock = rtm_optimistic_acquire(&table_lock);

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    //rtm_mutex_release(&table_lock);
    rtm_optimistic_release(&table_lock, noLock);

    return true;
}

void RTMOptimisticTxnManager::printStats(){    
    cout<<"RTM Stats"<<endl;

    cout<<"Locks elided : "<<g_locks_elided<<endl;
    cout<<"Locks failed : "<<g_locks_failed<<endl;
    cout<<"RTM retries : "<<g_rtm_retries<<endl;

}
