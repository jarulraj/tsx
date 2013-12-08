#include <iostream>
#include <set>

#include "RTMTxnManager.h"
#include "tester/workload.h"

bool RTMTxnManager::RunTxn(const vector<OpDescription> &operations,
        vector<string> *get_results, ThreadStats *stats) {
    // STATIC KEY SET  
    if(!dynamic){
        // Construct an ordered set of keys to lock.
        set<long> keys;
        for (const OpDescription &op : operations) {
            keys.insert(op.key%RTM_SUBSETS);
        }

        // Lock keys in order.
        for (long key : keys) {
            TIME_CODE(stats,rtm_mutex_acquire(&(lockTable[key])));
        }

        // Do transaction.
        ExecuteTxnOps(operations, get_results);

        // Unlock all keys in reverse order.
        for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
            rtm_mutex_release(&(lockTable[*rit]));
        }
    }
    // DYNAMIC KEY SET 
    // Deadlock detection and txn termination after undoing effects 
    else{
        bool abort = true;
        string result;

        while (abort) {
            set<long> keys;
            unordered_map<long, string> old_values;
            abort = false;
            long modkey;

            for (const OpDescription &op : operations) {
                modkey = op.key%RTM_SUBSETS ; 

                if (keys.count(op.key) == 0) {

                    TIME_CODE(stats, rtm_mutex_acquire(&table_lock));

                    if (lockTable.count(modkey) == 0) {
                        pthread_mutex_t* a = &lockTable[modkey];
                        TIME_CODE(stats, rtm_mutex_acquire(a));
                        rtm_mutex_release(&table_lock);
                    } 
                    else {
                        pthread_mutex_t* a = &lockTable[modkey];
                        rtm_mutex_release(&table_lock);

                        if(rtm_mutex_try_acquire(a) == false)
                            break;
                    }

                    keys.insert(op.key);
                }

                // Run this op.
                ExecuteTxnOp(op, &result);

                if (op.type == GET) {
                    if (get_results != NULL) {
                        get_results->push_back(result);
                    }
                } else if (old_values.count(op.key) == 0) {
                    old_values[op.key] = result;
                }
            }

            // Roll back changes if we aborted.
            if (abort) {
                OpDescription op;
                op.type = INSERT;
                for (auto it = old_values.begin(); it != old_values.end(); it++) {
                    op.key = it->first;
                    op.value = it->second;
                    ExecuteTxnOp(op);
                }
            }

            // Unlock all keys in reverse order.
            for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
                TIME_CODE(stats, rtm_mutex_acquire(&table_lock));
                modkey = (*rit)%RTM_SUBSETS;
                rtm_mutex_release(&lockTable[modkey]);
                rtm_mutex_release(&table_lock);
            }
        }
    } 

    return true;
}

/*
   bool RTMTxnManager::RunTxn(const std::vector<OpDescription> &operations,
   std::vector<string> *get_results, ThreadStats *stats) {

   TIME_CODE(stats, rtm_mutex_acquire(&table_lock));

// Do transaction.
ExecuteTxnOps(operations, get_results);

rtm_mutex_release(&table_lock);

return true;
}
*/

void RTMTxnManager::printStats(){    
    cout<<"RTM Stats"<<endl;

    cout<<"Locks elided : "<<g_locks_elided<<endl;
    cout<<"Locks failed : "<<g_locks_failed<<endl;
    cout<<"RTM retries : "<<g_rtm_retries<<endl;

}
