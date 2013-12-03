#ifndef _SPIN_LOCK_SIMPLE_TXN_MANAGER_H_
#define _SPIN_LOCK_SIMPLE_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"
 
class SpinLockSimpleTxnManager : public TxnManager {
public:
    SpinLockSimpleTxnManager(std::unordered_map<long,std::string> *table);
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<std::string> *get_results);

private:
    // Prevents concurrent insertions to the lock table.
    static pthread_spinlock_t table_lock;
};

#endif /* _SPIN_LOCK_SIMPLE_TXN_MANAGER_H_ */
