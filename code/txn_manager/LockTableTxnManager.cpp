#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include "TxnManager.h"
#include "LockTableTxnManager.h"

using namespace std;

bool conflict(LockMode mode1, LockMode mode2) {
    if ((mode1 == WRITE && mode2 != FREE) ||
	(mode2 == WRITE && mode1 != FREE)) {
	return true;
    }
    /*if (mode1 != FREE && mode2 != FREE) {
	return true;
	}*/
    return false;
}

bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock, mapped to the type of lock we
    // need to grab.
    map<uint64_t, LockMode> keys;
    for (const OpDescription &op : operations) {
	if (keys.count(op.key) == 1) {
	    if (op.type == GET && keys[op.key] != READ) {
		keys[op.key] = WRITE;
	    }
	} else {
	    if (op.type == GET) {
		keys[op.key] = READ;
	    } else {
		keys[op.key] = WRITE;
	    }
	}
    }

    // Lock keys in order.
    for (pair<uint64_t, LockMode> p : keys) {
	uint64_t key = p.first;
	LockMode requestMode = p.second;

	tableMutex.lock();
	if (lockTable.count(key) == 0) {
	    Lock *l = new Lock();
            mutex *m = new mutex();
	    l->mutex = m;
	    condition_variable *cv = new condition_variable();
	    l->cv = cv;
	    l->mode = requestMode;
	    if (requestMode == READ) {
		l->num_readers = 1;
	    } else {
		l->num_readers = 0;
	    }

	    unique_lock<mutex> lk(*m);
            lockTable[key] = l;
            tableMutex.unlock();
        } else {
            tableMutex.unlock();
	    Lock *l = lockTable[key];
            unique_lock<mutex> lk(*l->mutex);
	    thread::id id = this_thread::get_id();
	    l->q.push(id);
	    while (l->q.front() != id || conflict(requestMode, l->mode)) {
		//std::cout << id << " is going to sleep because requesting " << requestMode << " but " << l->mode << " conflicts " << conflict(requestMode, l->mode) << " and " << l->q.front() << "\n";
		l->cv->wait(lk);
		//std::cout << id << " is woken up\n";
	    }

	    if (requestMode == READ) {
		assert(l->mode != WRITE);
		l->mode = READ;
		l->num_readers++;
	    } else {
		assert(l->mode == FREE && l->num_readers == 0);
		l->mode = WRITE;
	    }
	    l->q.pop();
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::map<uint64_t, LockMode>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	Lock *l = lockTable[(*rit).first];
	{
	    lock_guard<mutex> lk(*l->mutex);
	    if (l->mode == READ) {
		assert(l->num_readers > 0);
		l->num_readers--;
		if (l->num_readers == 0) {
		    l->mode = FREE;
		}
	    } else {
		assert(l->num_readers == 0);
		l->mode = FREE;
	    }
	}
	l->cv->notify_all();
    }

    return true;
}

// Without reader/writer locks, keeping this for comparison.
/*bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock.
    set<uint64_t> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (uint64_t key : keys) {
        tableMutex.lock();
        if (lockTable.count(key) == 0) {
            mutex *m = new mutex();
            m->lock();
            lockTable[key] = m;
            tableMutex.unlock();
        } else {
            tableMutex.unlock();
            lockTable[key]->lock();
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        lockTable[*rit]->unlock();
    }

    return true;
    }*/

