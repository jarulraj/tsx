#ifndef _RTM_TXN_MANAGER_H_
#define _RTM_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "tsx.h"

class RTMTxnManager : public TxnManager {
    public:
        RTMTxnManager(std::unordered_map<long,std::string> *table)
            : TxnManager(table), table_lock(PTHREAD_MUTEX_INITIALIZER) {

                int rtm = cpu_has_rtm();

                if(rtm == 0){
                    cerr << "RTM not found on machine " << endl;
                    exit(-1);
                }

                //pthread_spin_init(&table_lock, PTHREAD_PROCESS_PRIVATE);       
                //table_lock.v = 0;

            }

        virtual bool RunTxn(const std::vector<OpDescription> &operations,
                std::vector<string> *get_results, ThreadStats *stats);

        void printStats();

    private:
        pthread_mutex_t table_lock;
        //pthread_spinlock_t table_lock;
        //spinlock_t table_lock;
};


#endif /* _RTM_TXN_MANAGER_H_ */
