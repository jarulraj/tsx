#include <iostream>
#include <set>

#include "RTMTxnManager.h"       
#include "rtm.h"

spinlock_t RTMTxnManager::table_lock = { 0 } ;

RTMTxnManager::RTMTxnManager(HashTable *table)
    : TxnManager(table) {
               
        int rtm = cpu_has_rtm();

        if(rtm == 0){
            cerr << "RTM not found on machine " << endl;
            exit(-1);
        }
        else{
            cout << "RTM AVAILABLE" << endl;
        }

    }


bool RTMTxnManager::RunTxn(const std::vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    // Construct an ordered set of keys to lock.
    set<uint64_t> keys;
    for (const OpDescription &op : operations) {
        keys.insert(op.key);
    }

    // Lock keys in order.
    for (uint64_t key : keys) {
        rtm_spinlock_acquire(&table_lock);
        
        if (lockTable.count(key) == 0) {
            spinlock_t key_lock;
            dyn_spinlock_init(&key_lock);
            
            rtm_spinlock_acquire(&key_lock);

            lockTable[key] = &key_lock;

            rtm_spinlock_release(&table_lock);
        } else {
            rtm_spinlock_release(&table_lock);
            
            rtm_spinlock_acquire(lockTable[key]);
        }
    }

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    // Unlock all keys in reverse order.
    std::set<uint64_t>::reverse_iterator rit;
    for (rit = keys.rbegin(); rit != keys.rend(); ++rit) {
        rtm_spinlock_release(lockTable[*rit]);
    }

    return true;
}

void RTMTxnManager::getStats(){
    /** Stats **/
    std::cout<<"Stats"<<std::endl;    
    
}


