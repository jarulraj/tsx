#ifndef _LOCK_TABLE_TXN_MANAGER_H_
#define _LOCK_TABLE_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"
 
class LockTableTxnManager : public TxnManager {
public:
    LockTableTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    unordered_map<uint64_t, mutex*> lockTable;
    // Prevents concurrent insertions to the lock table.
    mutex tableMutex;
};

#endif /* _LOCK_TABLE_TXN_MANAGER_H_ */
