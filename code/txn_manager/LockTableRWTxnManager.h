#ifndef _LOCK_TABLE_RW_TXN_MANAGER_H_
#define _LOCK_TABLE_RW_TXN_MANAGER_H_

#include <chrono>
#include <iostream>
#include <queue>
#include <set>
#include <thread>
#include <condition_variable>

#include "tester/workload.h"
#include "TxnManager.h"

struct Lock {
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::thread::id> q;
    int num_readers;
    LockMode mode;
};


class LockTableRWTxnManager : public TxnManager {
public:
     LockTableRWTxnManager(HashTable *table,
			 bool _dynamic, int num_keys)
       : TxnManager(table),
  	 dynamic(_dynamic){

	   for (int i=0; i < num_keys; i++) {
	     Lock *l = &lockTable[i]; // Creates Lock object
	     l->mode = FREE;
	     l->num_readers = 0;
	   }
	 }

     virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<std::string> *get_results, ThreadStats *stats);

private:
    static inline void lock_timed(std::mutex *mutex, ThreadStats *stats) {
        TIME_CODE(stats, mutex->lock());
    }

    // Based on std::lock_guard<mutex>, but with support for timing.
    class TimedLockGuard {
    public:
        explicit inline TimedLockGuard(std::mutex *mut, ThreadStats *stats)
            : mutex(mut) {
            lock_timed(mut, stats);
        }
        inline ~TimedLockGuard() {
            mutex->unlock();
        }
    private:
        std::mutex *mutex;
    };

    std::unordered_map<long, Lock> lockTable;
    // Prevents concurrent insertions to the lock table.
    std::mutex tableMutex;
    bool dynamic;
};

#endif /* _LOCK_TABLE_RW_TXN_MANAGER_H_ */
