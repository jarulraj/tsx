#ifndef _LOCK_TABLE_TXN_MANAGER_H_
#define _LOCK_TABLE_TXN_MANAGER_H_

#include <iostream>
#include <queue>
#include <set>
#include <thread>
#include <condition_variable>

#include "TxnManager.h"

typedef enum LockMode {
    READ,
    WRITE,
    FREE
} LockMode;


struct Lock {
    std::mutex *mutex;
    std::condition_variable *cv;
    std::queue<std::thread::id> q;
    int num_readers;
    LockMode mode;
};
 

class LockTableTxnManager : public TxnManager {
public:
    LockTableTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    unordered_map<uint64_t, Lock*> lockTable;
    // Prevents concurrent insertions to the lock table.
    mutex tableMutex;
};

#endif /* _LOCK_TABLE_TXN_MANAGER_H_ */
