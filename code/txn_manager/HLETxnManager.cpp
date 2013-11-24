#include <iostream>
#include <set>

#include "HLETxnManager.h"       

spinlock_t HLETxnManager::table_lock = { 0 } ;

HLETxnManager::HLETxnManager(std::unordered_map<long,std::string> *table)
    : TxnManager(table) {
         
        int hle = cpu_has_hle();

        if(hle == 0) {
            cerr << "HLE not found on machine " << endl;
            exit(-1);
        }
        else {
            cout << "HLE AVAILABLE" << endl;
        }
 
    }


bool HLETxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {

    /*
    // Construct an ordered set of keys to lock.
    set<uint64_t> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }
    
    // Lock keys in order.
    for (uint64_t key : keys) {
        hle_spinlock_acquire(&table_lock);
        if (lockTable.count(key) == 0) {
            spinlock_t lock ;
            lock.v = 0;

            lockTable[key] = &lock;
            
            hle_spinlock_release(&table_lock);

            hle_spinlock_acquire(&lock);
        } else {
            spinlock_t* lockPtr = lockTable[key];

            hle_spinlock_release(&table_lock);

            hle_spinlock_acquire(lockPtr);
        }
    }
    */

    hle_spinlock_acquire(&table_lock);

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    hle_spinlock_release(&table_lock);

    /*
    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        hle_spinlock_acquire(&table_lock);
        
        hle_spinlock_release(lockTable[*rit]);
        
        hle_spinlock_release(&table_lock);
    }
    */

    return true;
}


