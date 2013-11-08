#include <iostream>
#include <set>

#include "HLETxnManager.h"       
#include "rtm.h"

spinlock_t HLETxnManager::table_lock = { 0 } ;

HLETxnManager::HLETxnManager(HashTable *table)
    : TxnManager(table) {
         
        int hle = cpu_has_hle();

        if(hle == 0){
            cout<<"HLE not found on machine "<<endl;
            exit(-1);
        }
        else{
            cout<<"HLE AVAILABLE" <<endl;
        }
 
    }


bool HLETxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock.
    set<uint64_t> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (uint64_t key : keys) {
        hle_spinlock_acquire(&table_lock);
        
        if (lockTable.count(key) == 0) {
            spinlock_t key_lock;
            dyn_spinlock_init(&key_lock);
            
            hle_spinlock_acquire(&key_lock);

            lockTable[key] = &key_lock;

            hle_spinlock_release(&table_lock);
        } else {
            hle_spinlock_release(&table_lock);
            
            hle_spinlock_acquire(lockTable[key]);
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        hle_spinlock_release(lockTable[*rit]);
    }

    return true;
}

void HLETxnManager::getStats(){
    /** Stats **/
    std::cout<<"Stats"<<std::endl;    
    
}


