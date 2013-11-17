#include <iostream>
#include <set>

#include "HLETxnManager.h"       
#include "rtm.h"

spinlock_t HLETxnManager::table_lock = { 0 } ;

HLETxnManager::HLETxnManager(HashTable *table)
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

    hle_spinlock_acquire(&table_lock);

    // Do transaction.
    ExecuteTxnOps(operations, get_results);

    hle_spinlock_release(&table_lock);

    return true;
}

void HLETxnManager::getStats(){
    /** Stats **/
    std::cout<<"Stats"<<std::endl;    

}


