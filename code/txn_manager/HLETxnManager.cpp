#include <iostream>
#include <set>

#include "HLETxnManager.h"
#include "tester/workload.h"

const int MAX_TRIES = 10;

/*
bool HLETxnManager::RunTxn(const vector<OpDescription> &operations,
        vector<string> *get_results, ThreadStats *stats) {
    // DYNAMIC KEY SET 
    // Deadlock detection and txn termination after undoing effects 
    if (dynamic) {
        bool abort = true;
        string result;

        while (abort) {
            set<long> keys;
            unordered_map<long, string> old_values;
            abort = false;

            for (const OpDescription &op : operations) {
                if (keys.count(op.key) == 0) {

                    TIME_CODE(stats, hle_spinlock_acquire(&table_lock));

                    if (lockTable.count(op.key) == 0) {
                        spinlock_t* a = &lockTable[op.key];
                        TIME_CODE(stats, hle_spinlock_acquire(a));
                        hle_spinlock_release(&table_lock);
                    } 
                    else {
                        spinlock_t* a = &lockTable[op.key];
                        hle_spinlock_release(&table_lock);

                        int tries = 0;
                        
                        while (__atomic_exchange_n(&a->v, 1, __ATOMIC_ACQUIRE|__ATOMIC_HLE_ACQUIRE) != 0 && abort == false) { 
                            int val; 
                            //  Wait for lock to become free again before retrying. 
                            do { 
                                _mm_pause(); 
                                // Abort speculation  
                                val = __atomic_load_n(&a->v, __ATOMIC_CONSUME);

                                tries++;
                                if(tries == MAX_TRIES) {
                                    abort = true;
                                    if (get_results != NULL) {
                                        get_results->clear();
                                    }
                                    break;
                                }                        

                            } while (val == 1); 
                        } 

                        if (abort) {
                            break;
                        }
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
                TIME_CODE(stats, hle_spinlock_acquire(&table_lock));
                hle_spinlock_release(&lockTable[*rit]);
                hle_spinlock_release(&table_lock);
            }
        }
    } 
    // STATIC KEY SET  
    else {
        // Construct an ordered set of keys to lock.
        set<long> keys;
        for (const OpDescription &op : operations) {
            keys.insert(op.key);
        }

        // Lock keys in order.
        for (long key : keys) {
            TIME_CODE(stats, hle_spinlock_acquire(&lockTable[key]));
        }

        // Do transaction.
        ExecuteTxnOps(operations, get_results);

        // Unlock all keys in reverse order.
        for (auto rit = keys.rbegin(); rit != keys.rend(); ++rit) {
            hle_spinlock_release(&lockTable[*rit]);
        }
    }

    return true;
}
*/

bool HLETxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results, ThreadStats *stats) {

    TIME_CODE(stats, hle_spinlock_acquire(&table_lock));

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    hle_spinlock_release(&table_lock);

    return true;
}

