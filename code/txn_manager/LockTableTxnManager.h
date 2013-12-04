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

#endif /* _LOCK_TABLE_TXN_MANAGER_H_ */
