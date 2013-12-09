#ifndef _RTM_TXN_MANAGER_H_
#define _RTM_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "tsx.h"

const int RTM_MAX_TRIES = 8    ;
const int RTM_SUBSETS   = 128  ;

class RTMTxnManager : public TxnManager {
    public:
        RTMTxnManager(std::unordered_map<long,std::string> *table , bool _dynamic, int num_keys)
        //RTMTxnManager(HashTable *table , bool _dynamic, int num_keys)
            : TxnManager(table), dynamic(_dynamic) {

                int rtm = cpu_has_rtm();

                if(rtm == 0){
                    cerr << "RTM not found on machine " << endl;
                    exit(-1);
                }

               subsets = RTM_SUBSETS;
               //subsets = num_keys;

                // Initiliaze lock table
                for (int i=0; i<RTM_SUBSETS; i++) {
                    pthread_mutex_t key_lock = PTHREAD_MUTEX_INITIALIZER ;
                    lockTable[i] = key_lock;  
                }

                table_lock = PTHREAD_MUTEX_INITIALIZER ;
 
                //pthread_spin_init(&table_lock, PTHREAD_PROCESS_PRIVATE);       
                //table_lock.v = 0;

            }

        virtual bool RunTxn(const std::vector<OpDescription> &operations,
                std::vector<string> *get_results, ThreadStats *stats);

        void printStats();

    private:
        pthread_mutex_t table_lock;
        std::unordered_map<long, pthread_mutex_t> lockTable;
        
        int subsets;
        bool dynamic; 
};


#endif /* _RTM_TXN_MANAGER_H_ */
