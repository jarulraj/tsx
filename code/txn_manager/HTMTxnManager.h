#ifndef _HTM_TXN_MANAGER_H_
#define _HTM_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "rtm.h"

class HTMTxnManager : public TxnManager {
public:
    HTMTxnManager(HashTable *table);

    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    unordered_map<uint64_t, spinlock_t*> spinlockTable;
    // Prevents concurrent insertions to the lock table.
    static spinlock_t table_lock;
    
    unordered_map<uint64_t, atomic_flag*> lockTable;
    mutex tableMutex ;
};


#endif /* _HTM_TXN_MANAGER_H_ */
