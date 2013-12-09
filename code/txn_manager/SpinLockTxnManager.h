#ifndef _SPIN_LOCK_TXN_MANAGER_H_
#define _SPIN_LOCK_TXN_MANAGER_H_

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <set>

#include "TxnManager.h"
 
class SpinLockTxnManager : public TxnManager {
    public:
        //SpinLockTxnManager(std::unordered_map<long,std::string> *table,
        SpinLockTxnManager(HashTable *table,
                bool _dynamic, int num_keys)
            : TxnManager(table), dynamic(_dynamic) {
                for (int i=0; i<num_keys; i++) {
                    std::atomic_flag *a = &lockTable[i];  // Creates flag
                }
            }
        virtual bool RunTxn(const std::vector<OpDescription> &operations,
                std::vector<std::string> *get_results, ThreadStats *stats);

    private:
        std::unordered_map<long, std::atomic_flag> lockTable;
        // Prevents concurrent insertions to the lock table.
        std::mutex tableMutex;
        bool dynamic;
};


#endif /* _SPIN_LOCK_TXN_MANAGER_H_ */
