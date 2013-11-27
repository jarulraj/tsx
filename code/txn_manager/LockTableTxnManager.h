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
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::thread::id> q;
    int num_readers;
    LockMode mode;
};
 

class LockTableTxnManager : public TxnManager {
public:
     LockTableTxnManager(std::unordered_map<long,std::string> *table,
			 bool _dynamic)
       : TxnManager(table),
  	 dynamic(_dynamic){}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<std::string> *get_results);

private:
    std::unordered_map<long, Lock> lockTable;
    // Prevents concurrent insertions to the lock table.
    std::mutex tableMutex;
    bool dynamic;
};

#endif /* _LOCK_TABLE_TXN_MANAGER_H_ */
