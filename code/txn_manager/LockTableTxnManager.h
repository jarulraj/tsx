#ifndef _LOCK_TABLE_TXN_MANAGER_H_
#define _LOCK_TABLE_TXN_MANAGER_H_

#include <chrono>
#include <iostream>
#include <queue>
#include <set>
#include <thread>
#include <condition_variable>

#include "tester/workload.h"
#include "TxnManager.h"


class LockTableTxnManager : public TxnManager {
public:
     LockTableTxnManager(std::unordered_map<long,std::string> *table,
			 bool _dynamic, int num_keys)
       : TxnManager(table),
  	 dynamic(_dynamic){

	   for (int i=0; i < num_keys; i++) {
	     std::mutex *m = &lockTable[i]; // Creates Lock object
	   }
	 }

     virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<std::string> *get_results, ThreadStats *stats);

private:
    // Based on std::lock_guard<mutex>, but with support for timing.
    class TimedLockGuard {
    public:
        explicit inline TimedLockGuard(std::mutex *mut, ThreadStats *stats)
            : mutex(mut) {
            TIME_CODE(stats, mut->lock());
        }
        inline ~TimedLockGuard() {
            mutex->unlock();
        }
    private:
        std::mutex *mutex;
    };

    std::unordered_map<long, std::mutex> lockTable;
    // Prevents concurrent insertions to the lock table.
    std::mutex tableMutex;
    bool dynamic;
};

#endif /* _LOCK_TABLE_TXN_MANAGER_H_ */
