#ifndef _HLE_TXN_MANAGER_H_
#define _HLE_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "hashtable.h"
#include "TxnManager.h"       
#include "tsx.h"

const int HLE_MAX_TRIES = 10 ;
const int HLE_SUBSETS  =  8 ;

class HLETxnManager : public TxnManager {
    public:
        HLETxnManager(std::unordered_map<long,std::string> *table, bool _dynamic, int num_keys)
            : TxnManager(table), dynamic(_dynamic) {

                int hle = cpu_has_hle();

                if(hle == 0) {
                    cerr << "HLE not found on machine " << endl;
                    exit(-1);
                }

                subsets = HLE_SUBSETS;

                // Initiliaze lock table
                for (int i=0; i<HLE_SUBSETS; i++) {
                    spinlock_t key_lock = { 0 };
                    lockTable[i] = key_lock;  
                }

                table_lock = { 0 } ;

            }

        virtual bool RunTxn(const std::vector<OpDescription> &operations,
                std::vector<string> *get_results, ThreadStats *stats);

    private:
        // Prevents concurrent insertions to the lock table.
        spinlock_t table_lock;
        std::unordered_map<long, spinlock_t> lockTable;
        
        int subsets;
        bool dynamic;    
};


#endif /* _HLE_TXN_MANAGER_H_ */
