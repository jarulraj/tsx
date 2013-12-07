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


bool LockTableTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results, ThreadStats *stats) {

    // Construct an ordered set of keys to lock.
    set<long> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (long key : keys) {
      lockTable[key].lock();
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<long>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        lockTable[*rit].unlock();
    }

    return true;
}

