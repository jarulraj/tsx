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
#include "LockTableRWTxnManager.h"


// Reader/writer locks, static or dynamic working sets.
bool LockTableRWTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results, ThreadStats *stats) {
    if (dynamic) {
	bool abort = true;
	string result;

	while (abort) {
	    set<long> keys;
	    unordered_map<long, string> old_values;
	    abort = false;
	    LockMode requestMode;
	    for (const OpDescription &op : operations) {
		if (op.type == GET) {
		    requestMode = READ;
		} else {
		    requestMode = WRITE;
		}

		if (keys.count(op.key) == 0) {
		    lock_timed(&tableMutex, stats);
		    if (lockTable.count(op.key) == 0) {
			Lock *l = &lockTable[op.key]; // Creates lock object
			l->mode = requestMode;

			if (requestMode == READ) {
			    l->num_readers = 1;
			} else {
			    l->num_readers = 0;
			}

                        TIME_CODE(stats, unique_lock<mutex> lk(l->mutex));
			tableMutex.unlock();
		    } else {
			Lock *l = &lockTable[op.key];
			tableMutex.unlock();

	                TIME_CODE(stats, unique_lock<mutex> lk(l->mutex));

			thread::id id = this_thread::get_id();
			l->q.push(id);
			int wakeups = 0;
			while (l->q.front() != id || conflict(requestMode, l->mode)) {
			    TIME_CODE(stats, l->cv.wait_for(lk, chrono::milliseconds(10)));
			    wakeups++;
			    if (wakeups > 2) {
				// Remove this thread from the queue of waiting threads.
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
				if (get_results != NULL) {
				    get_results->clear();
				}
				break;
			    }
			}

			if (abort) {
			    break;
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
		    keys.insert(op.key);
		}

		// Run this op.
		ExecuteTxnOp(op, &result);
		if (op.type == GET) {
		    if (get_results != NULL) {
			get_results->push_back(result);
		    }
		} else if (old_values.count(op.key) == 0) {
		    old_values[op.key] = result;
		}
	    }

	    // Roll back changes if we aborted.
	    if (abort) {
		unordered_map<long, string>::iterator it;
		OpDescription op;
		op.type = INSERT;
		for (it = old_values.begin(); it != old_values.end(); it++) {
		    op.key = it->first;
		    op.value = it->second;
		    ExecuteTxnOp(op);
		}
	    }

	    // Unlock all keys in reverse order.
	    std::set<long>::reverse_iterator rit;
	    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
		lock_timed(&tableMutex, stats);
		Lock *l = &lockTable[*rit];
		tableMutex.unlock();
		{
	            TimedLockGuard guard(&l->mutex, stats);
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
		l->cv.notify_all();
	    }
	}
    } else {
	// Construct an ordered set of keys to lock, mapped to the type of lock we
	// need to grab.
	map<long, LockMode> keys;
	for (const OpDescription &op : operations) {
	    if (keys.count(op.key) == 1) {
		if (op.type != GET) {
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

	//std::cout << keys.size() << " ";

	// Lock keys in order.
	for (pair<long, LockMode> p : keys) {
	    long key = p.first;
	    LockMode requestMode = p.second;

	    Lock *l = &lockTable[key];

	    TIME_CODE(stats, unique_lock<mutex> lk(l->mutex));
	    thread::id id = this_thread::get_id();
	    l->q.push(id);
	    while (l->q.front() != id || conflict(requestMode, l->mode)) {
	    //while (conflict(requestMode, l->mode)) {
	      TIME_CODE(stats, l->cv.wait(lk));
	    }

	    if (requestMode == READ) {
	      assert(l->mode != WRITE);
	      l->mode = READ;
	      l->num_readers++;
	      if (l->num_readers > 3) {
		//std::cout << l->num_readers << "\n";
	      }
	    } else {
	      assert(l->mode == FREE && l->num_readers == 0);
	      l->mode = WRITE;
	    }
	    l->q.pop();
	}

	// Do transaction.
	ExecuteTxnOps(operations, get_results);

	// Unlock all keys in reverse order.
	std::map<long, LockMode>::reverse_iterator rit;
	for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
	    Lock *l = &lockTable[rit->first];
	    bool notify = false;
	    {
	        TimedLockGuard guard(&l->mutex, stats);
		if (l->mode == READ) {
		    assert(l->num_readers > 0);
		    l->num_readers--;
		    if (l->num_readers == 0) {
			l->mode = FREE;
			notify = true;
		    }
		} else {
		    assert(l->num_readers == 0);
		    assert(rit->second == WRITE);
		    l->mode = FREE;
		    notify = true;
		}
	    }
	    if (notify) {
	      l->cv.notify_all();
	    }
	}
    }
    return true;
    }
