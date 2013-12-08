#ifndef _RTM_H_
#define _RTM_H_

/* 
 * Based on spinlock-rtm.c: A spinlock implementation with dynamic lock elision.
 * Copyright (c) 2013 Austin Seipp
 * Copyright (c) 2013 CMU
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <cpuid.h>

#include "immintrin.h"  /* for _mm_pause() */
#include <iostream>

using namespace std;

/* -------------------------------------------------------------------------- */
/* -- Utilities ------------------------------------------------------------- */

#define ALWAYS_INLINE __attribute__((__always_inline__)) inline
#define UNUSED        __attribute__((unused))
#define likely(x)     (__builtin_expect((x),1))
#define unlikely(x)   (__builtin_expect((x),0))

/* -------------------------------------------------------------------------- */
/* -- HLE/RTM compatibility code for older binutils/gcc etc ----------------- */

#define PREFIX_XACQUIRE ".byte 0xF2; "
#define PREFIX_XRELEASE ".byte 0xF3; "

#define CPUID_RTM (1 << 11)
#define CPUID_HLE (1 << 4)

int cpu_has_rtm(void) ;

int cpu_has_hle(void) ;

#define _RTM_MAX_TRIES         100
#define _RTM_MAX_ABORTS         30

#define _XBEGIN_STARTED         (~0u)
#define _XABORT_EXPLICIT        (1 << 0)
#define _XABORT_RETRY           (1 << 1)
#define _XABORT_CONFLICT        (1 << 2)
#define _XABORT_CAPACITY        (1 << 3)
#define _XABORT_DEBUG           (1 << 4)
#define _XABORT_NESTED          (1 << 5)
#define _XABORT_CODE(x)         (((x) >> 24) & 0xff)

#define _xbegin()                                       \
    ({                                                    \
     int ret = _XBEGIN_STARTED;                          \
     __asm__ __volatile__(".byte 0xc7,0xf8 ; .long 0"    \
         : "+a" (ret)                   \
         :                              \
         : "memory");                   \
     ret;                                                \
     })                                                    \

#define _xend()                                                 \
    ({                                                            \
     __asm__ __volatile__(".byte 0x0f,0x01,0xd5"                 \
         ::: "memory");                         \
     })

#define _xabort(status)                                         \
    ({                                                            \
     __asm__ __volatile__( ".byte 0xc6,0xf8,%P0"                 \
         :                                     \
         : "i" (status)                        \
         : "memory");                          \
     })

#define _xtest()                                                 \
    ({                                                             \
     unsigned char out;                                           \
     __asm__ __volatile__( ".byte 0x0f,0x01,0xd6 ; setnz %0"      \
         : "=r" (out)                           \
         :                                      \
         : "memory");                           \
     out;                                                         \
     })



/* Stats */
static int g_locks_elided = 0;
static int g_locks_failed = 0;
static int g_rtm_retries  = 0;

struct spinlock { bool v; }  __attribute__ ((aligned (64)));;
//struct spinlock { bool v; bool pad[63]; } ;

typedef spinlock spinlock_t ;

// HLE
static ALWAYS_INLINE void dyn_spinlock_init(spinlock_t* lock);

static ALWAYS_INLINE void hle_spinlock_acquire(spinlock_t* lock); 

static ALWAYS_INLINE bool hle_spinlock_isfree(spinlock_t* lock); 

static ALWAYS_INLINE void hle_spinlock_release(spinlock_t* lock); 

// RTM + HLE
/*
   static ALWAYS_INLINE bool rtm_spinlock_isfree(spinlock_t* lock); 

   static ALWAYS_INLINE void rtm_spinlock_acquire(spinlock_t* lock); 

   static ALWAYS_INLINE void rtm_spinlock_release(spinlock_t* lock); 
   */

// RTM + PTHREAD SPINLOCK
static ALWAYS_INLINE bool rtm_spinlock_isfree(pthread_spinlock_t* lock); 

static ALWAYS_INLINE void rtm_spinlock_acquire(pthread_spinlock_t* lock); 

static ALWAYS_INLINE void rtm_spinlock_release(pthread_spinlock_t* lock); 

// RTM + PTHREAD MUTEX

static ALWAYS_INLINE bool rtm_mutex_isfree(pthread_mutex_t* lock); 

static ALWAYS_INLINE void rtm_mutex_acquire(pthread_mutex_t* lock); 

static ALWAYS_INLINE bool rtm_mutex_try_acquire(pthread_mutex_t* lock); 

static ALWAYS_INLINE void rtm_mutex_release(pthread_mutex_t* lock); 


/* ---- DEFINITIONS ---- */

static ALWAYS_INLINE void dyn_spinlock_init(spinlock_t* lock)
{
    lock->v = 0;
}

static ALWAYS_INLINE void hle_spinlock_acquire(spinlock_t* lock)
{            
    while (__atomic_exchange_n(&lock->v, true, __ATOMIC_ACQUIRE|__ATOMIC_HLE_ACQUIRE))
    { 
        // Wait for lock to become free again before retrying. 
        do { 
            _mm_pause();       
            //__asm__ __volatile__ ("pause" ::: "memory");
            // Abort speculation 
        } while (__atomic_load_n(&lock->v, __ATOMIC_CONSUME)); 
    } 
}

static ALWAYS_INLINE bool hle_spinlock_isfree(spinlock_t* lock)
{
    return (__sync_bool_compare_and_swap(&lock->v, false, false) ? true : false);
}

static ALWAYS_INLINE void hle_spinlock_release(spinlock_t* lock)
{
    __atomic_clear(&lock->v, __ATOMIC_RELEASE|__ATOMIC_HLE_RELEASE);
}

static ALWAYS_INLINE bool rtm_spinlock_isfree(pthread_spinlock_t* lock)
{
    // use lock value itself
    return (__sync_bool_compare_and_swap(&lock, 0, 0) ? true : false);
    //return (__sync_bool_compare_and_swap(&lock->v, 0, 0) ? true : false);
}


static ALWAYS_INLINE void rtm_spinlock_acquire(pthread_spinlock_t* lock)
{
    unsigned int tm_status = 0;
    int tries = 0, retries = 0;

tm_try:
    if(tries++ < _RTM_MAX_TRIES){
        if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
            // If the lock is free, speculatively elide acquisition and continue. 
            if (rtm_spinlock_isfree(lock)){ 
                return;
            }

            // Otherwise fall back to the spinlock by aborting. 
            // 0xff canonically denotes 'lock is taken'.  
            _xabort(0xff); 
        } 
        else {
            // _xbegin could have had a conflict, been aborted, etc 
            if (tm_status & _XABORT_RETRY) {
#ifdef DEBUG                
                __sync_add_and_fetch(&g_rtm_retries, 1);
#endif
                if(retries++ < _RTM_MAX_ABORTS)
                    goto tm_try; // Retry 
                else
                    goto tm_fail;
            }
            if (tm_status & _XABORT_EXPLICIT) {
                int code = _XABORT_CODE(tm_status);
                if (code == 0xff) 
                    goto tm_fail; // Lock was taken; fallback 
            }
        }
    }

    //fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
tm_fail:
#ifdef DEBUG
    __sync_add_and_fetch(&g_locks_failed, 1);
#endif
    pthread_spin_lock(lock);
    //hle_spinlock_acquire(lock);


}

static ALWAYS_INLINE void rtm_spinlock_release(pthread_spinlock_t* lock)
{
    // If the lock is still free, we'll assume it was elided. This implies we're in a transaction. 
    if (rtm_spinlock_isfree(lock)) {
#ifdef DEBUG
        g_locks_elided += 1;
#endif
        _xend(); // Commit transaction 
    } else {
        // Otherwise, the lock was taken by us, so release it too. 
        pthread_spin_unlock(lock);
        //hle_spinlock_release(lock);
    }
}

static ALWAYS_INLINE bool rtm_mutex_isfree(pthread_mutex_t* lock)
{
    // use lock value itself
    return (__sync_bool_compare_and_swap(&lock, 0, 0) ? true : false);
}



static ALWAYS_INLINE void rtm_mutex_acquire(pthread_mutex_t* lock)
{
    unsigned int tm_status = 0;
    int tries = 0, retries = 0;

tm_try:
    if(tries++ < _RTM_MAX_TRIES){
        if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
            // If the lock is free, speculatively elide acquisition and continue. 
            if (rtm_mutex_isfree(lock)){ 
                return;
            }

            // Otherwise fall back to the spinlock by aborting. 
            // 0xff canonically denotes 'lock is taken'.  
            _xabort(0xff); 
        } 
        else {
            // _xbegin could have had a conflict, been aborted, etc 
            if (tm_status & _XABORT_RETRY) {
#ifdef DEBUG                
                __sync_add_and_fetch(&g_rtm_retries, 1);
#endif
                if(retries++ < _RTM_MAX_ABORTS)
                    goto tm_try; // Retry 
                else
                    goto tm_fail;
            }
            if (tm_status & _XABORT_EXPLICIT) {
                if (_XABORT_CODE(tm_status) == 0xff) 
                    goto tm_fail; // Lock was taken; fallback 
            }
        }
    }

    //fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
tm_fail:
#ifdef DEBUG
    __sync_add_and_fetch(&g_locks_failed, 1);
#endif
    pthread_mutex_lock(lock);

}

static ALWAYS_INLINE bool rtm_mutex_try_acquire(pthread_mutex_t* lock)
{
    unsigned int tm_status = 0;
    int tries = 0, retries = 0;

tm_try:
    if(tries++ < _RTM_MAX_TRIES){
        if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
            // If the lock is free, speculatively elide acquisition and continue. 
            if (rtm_mutex_isfree(lock)){ 
                return true;
            }

            // Otherwise fall back to the spinlock by aborting. 
            // 0xff canonically denotes 'lock is taken'.  
            _xabort(0xff); 
        } 
        else {
            // _xbegin could have had a conflict, been aborted, etc 
            if (tm_status & _XABORT_RETRY) {
#ifdef DEBUG                
                __sync_add_and_fetch(&g_rtm_retries, 1);
#endif
                if(retries++ < _RTM_MAX_ABORTS)
                    goto tm_try; // Retry 
                else
                    goto tm_fail;
            }
            if (tm_status & _XABORT_EXPLICIT) {
                if (_XABORT_CODE(tm_status) == 0xff) 
                    goto tm_fail; // Lock was taken; fallback 
            }
        }
    }

    //fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
tm_fail:
    pthread_mutex_lock(lock);
    return false;
}

static ALWAYS_INLINE void rtm_mutex_release(pthread_mutex_t* lock)
{
    // If the lock is still free, we'll assume it was elided. This implies we're in a transaction. 
    if (rtm_mutex_isfree(lock)) {
#ifdef DEBUG
        g_locks_elided += 1;
#endif
        _xend(); // Commit transaction 
    } else {
        // Otherwise, the lock was taken by us, so release it too. 
        pthread_mutex_unlock(lock);
    }
}

static ALWAYS_INLINE bool rtm_optimistic_acquire(pthread_mutex_t* lock)
{
    unsigned int tm_status = 0;
    int tries = 0, retries = 0;
    pthread_mutex_t* val;

tm_try:
    if(tries++ < _RTM_MAX_TRIES){
        if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
            val = lock; // Just read
            return true;
        } 
        else {
            // _xbegin could have had a conflict, been aborted, etc 
            if (tm_status & _XABORT_RETRY) {
              if(retries++ < _RTM_MAX_ABORTS)
                  goto tm_try; // Retry 
              else
                  goto tm_fail;
            }
            if (tm_status & _XABORT_EXPLICIT) {
                if (_XABORT_CODE(tm_status) == 0xff) 
                    goto tm_fail; // Lock was taken; fallback 
            }  
            else
                goto tm_try;
        }
    }

tm_fail:
    pthread_mutex_lock(lock);
    return false;

}

static ALWAYS_INLINE void rtm_optimistic_release(pthread_mutex_t* lock, bool noLock)
{
    if (noLock) {
        _xend(); // Commit transaction 
    } else {
        pthread_mutex_unlock(lock);
    }
}


#endif /* _RTM_H_ */
