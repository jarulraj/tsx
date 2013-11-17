#include <cassert>
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>

#include "TxnManager.h"
#include "LockTableTxnManager.h"

using namespace std;

bool conflict(LockMode mode1, LockMode mode2) {
    if ((mode1 == WRITE && mode2 != FREE) ||
	(mode2 == WRITE && mode1 != FREE)) {
	return true;
    }

    return false;
}

bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    bool finished = false;
    while (!finished) {
	// Construct an ordered set of keys to lock, mapped to the type of lock we
	// need to grab.
	unordered_map<long, LockMode> keys;
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

	list<pair<long, LockMode> > l;
	for (pair<long, LockMode> p : keys) {
	    l.push_back(p);
	}

	// Randomize the order of keys to generate deadlock.
	for (int i = 0; i < rand() % l.size(); i++) {
	    l.push_back(l.front());
	    l.pop_front();
	}

	bool abort = false;
        unordered_map<long, LockMode> locked;
	for (pair<long, LockMode> p : l) {
	    assert(!abort);
	    long key = p.first;
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
		locked[key] = requestMode;
		unique_lock<mutex> lk(*m);
		lockTable[key] = l;
		tableMutex.unlock();
	    } else {
		tableMutex.unlock();
		Lock *l = lockTable[key];
		unique_lock<mutex> lk(*l->mutex);
		thread::id id = this_thread::get_id();
		l->q.push(id);
		int wakeups = 0;
		while (l->q.front() != id || conflict(requestMode, l->mode)) {
		    l->cv->wait_for(lk, chrono::milliseconds(10));
		    wakeups++;
		    if (wakeups > 10) {
			queue<thread::id> q;
			while (l->q.size() > 0) {
			    thread::id i = l->q.front();
			    l->q.pop();
			    if (i != id) {
				q.push(i);
			    }
			}
			l->q.swap(q);
			abort = true;
			break;
			}
		}

		if (abort) {
		    break;
		}

		locked[key] = requestMode;

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

	if (!abort) {
	    // Do transaction.
	    finished = true;
	    ExecuteTxnOps(operations, get_results);
	}

	// Unlock the keys that were successfully locked.
	std::unordered_map<long, LockMode>::iterator rit;
	for (rit = locked.begin(); rit != locked.end(); rit++) {
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
    }

    return true;
}


// Reader/writer locks, static working sets.
/*bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock, mapped to the type of lock we
    // need to grab.
    map<long, LockMode> keys;
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
    for (pair<long, LockMode> p : keys) {
	long key = p.first;
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
		l->cv->wait(lk);
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
    std::map<long, LockMode>::reverse_iterator rit;
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
    }*/

// Without reader/writer locks, keeping this for comparison.
/*bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock.
    set<long> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (long key : keys) {
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
    std::set<long>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        lockTable[*rit]->unlock();
    }

    return true;
    }*/

