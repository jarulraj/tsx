#ifndef _RTM_OPTIMISTIC_TXN_MANAGER_H_
#define _RTM_OPTIMISTIC_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "tsx.h"

const int RTM_OPT_MAX_TRIES = 10   ;
const int RTM_OPT_SUBSETS   = 128  ;

class RTMOptimisticTxnManager : public TxnManager {
    public:
        RTMOptimisticTxnManager(std::unordered_map<long,std::string> *table , bool _dynamic, int num_keys)
            : TxnManager(table), dynamic(_dynamic) {

                int rtm = cpu_has_rtm();

                if(rtm == 0){
                    cerr << "RTM not found on machine " << endl;
                    exit(-1);
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
        
        bool dynamic; 
};


#endif /* _RTM_OPTIMISTIC_TXN_MANAGER_H_ */
