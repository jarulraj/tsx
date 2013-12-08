#ifndef _HLE_TXN_MANAGER_H_
#define _HLE_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "hashtable.h"
#include "TxnManager.h"       
#include "tsx.h"

class HLETxnManager : public TxnManager {
public:
    HLETxnManager(HashTable *table, bool _dynamic, int num_keys);

    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results, ThreadStats *stats);

private:
    // Prevents concurrent insertions to the lock table.
    spinlock_t table_lock;
    std::unordered_map<long, spinlock_t> lockTable;
    bool dynamic;    
};


#endif /* _HLE_TXN_MANAGER_H_ */
