#ifndef _RTM_H_
#define _RTM_H_

/* 
 * spinlock-rtm.c: A spinlock implementation with dynamic lock elision.
 * Copyright (c) 2013 Austin Seipp
 * Copyright (c) 2012,2013 Intel Corporation
 * Author: Andi Kleen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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

/* Clang compatibility for GCC */
#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if defined(__clang__)
#define LOCKABLE            __attribute__ ((lockable))
#define SCOPED_LOCKABLE     __attribute__ ((scoped_lockable))
#define GUARDED_BY(x)       __attribute__ ((guarded_by(x)))
#define GUARDED_VAR         __attribute__ ((guarded_var))
#define PT_GUARDED_BY(x)    __attribute__ ((pt_guarded_by(x)))
#define PT_GUARDED_VAR      __attribute__ ((pt_guarded_var))
#define ACQUIRED_AFTER(...) __attribute__ ((acquired_after(__VA_ARGS__)))
#define ACQUIRED_BEFORE(...) __attribute__ ((acquired_before(__VA_ARGS__)))
#define EXCLUSIVE_LOCK_FUNCTION(...)    __attribute__ ((exclusive_lock_function(__VA_ARGS__)))
#define SHARED_LOCK_FUNCTION(...)       __attribute__ ((shared_lock_function(__VA_ARGS__)))
#define ASSERT_EXCLUSIVE_LOCK(...)      __attribute__ ((assert_exclusive_lock(__VA_ARGS__)))
#define ASSERT_SHARED_LOCK(...)         __attribute__ ((assert_shared_lock(__VA_ARGS__)))
#define EXCLUSIVE_TRYLOCK_FUNCTION(...) __attribute__ ((exclusive_trylock_function(__VA_ARGS__)))
#define SHARED_TRYLOCK_FUNCTION(...)    __attribute__ ((shared_trylock_function(__VA_ARGS__)))
#define UNLOCK_FUNCTION(...)            __attribute__ ((unlock_function(__VA_ARGS__)))
#define LOCK_RETURNED(x)    __attribute__ ((lock_returned(x)))
#define LOCKS_EXCLUDED(...) __attribute__ ((locks_excluded(__VA_ARGS__)))
#define EXCLUSIVE_LOCKS_REQUIRED(...) \
  __attribute__ ((exclusive_locks_required(__VA_ARGS__)))
#define SHARED_LOCKS_REQUIRED(...) \
  __attribute__ ((shared_locks_required(__VA_ARGS__)))
#define NO_THREAD_SAFETY_ANALYSIS  __attribute__ ((no_thread_safety_analysis))
#else
#define LOCKABLE
#define SCOPED_LOCKABLE
#define GUARDED_BY(x)
#define GUARDED_VAR
#define PT_GUARDED_BY(x)
#define PT_GUARDED_VAR
#define ACQUIRED_AFTER(...)
#define ACQUIRED_BEFORE(...)
#define EXCLUSIVE_LOCK_FUNCTION(...)
#define SHARED_LOCK_FUNCTION(...)
#define ASSERT_EXCLUSIVE_LOCK(...)
#define ASSERT_SHARED_LOCK(...)
#define EXCLUSIVE_TRYLOCK_FUNCTION(...)
#define SHARED_TRYLOCK_FUNCTION(...)
#define UNLOCK_FUNCTION(...)
#define LOCKS_RETURNED(x)
#define LOCKS_EXCLUDED(...)
#define EXCLUSIVE_LOCKS_REQUIRED(...)
#define SHARED_LOCKS_REQUIRED(...)
#define NO_THREAD_SAFETY_ANALYSIS
#endif /* defined(__clang__) */

/* -------------------------------------------------------------------------- */
/* -- HLE/RTM compatibility code for older binutils/gcc etc ----------------- */

#define PREFIX_XACQUIRE ".byte 0xF2; "
#define PREFIX_XRELEASE ".byte 0xF3; "

#define CPUID_RTM (1 << 11)
#define CPUID_HLE (1 << 4)

int cpu_has_rtm(void) ;

int cpu_has_hle(void) ;


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

/* -------------------------------------------------------------------------- */
/* -- A simple spinlock implementation with lock elision -------------------- */

/* Statistics */
static int g_locks_elided = 0;
static int g_locks_failed = 0;
static int g_rtm_retries  = 0;

typedef struct LOCKABLE spinlock { int v; } spinlock_t;

static ALWAYS_INLINE void dyn_spinlock_init(spinlock_t* lock);

static ALWAYS_INLINE void hle_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock);

static ALWAYS_INLINE bool hle_spinlock_isfree(spinlock_t* lock); 

static ALWAYS_INLINE void hle_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock);

static ALWAYS_INLINE void rtm_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock);

static ALWAYS_INLINE void rtm_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock);

/* ---- DEFINITIONS ---- */
                
static ALWAYS_INLINE void dyn_spinlock_init(spinlock_t* lock)
{
    lock->v = 0;
}

static ALWAYS_INLINE void hle_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
{            
    while (__atomic_exchange_n(&lock->v, 1, __ATOMIC_ACQUIRE|__ATOMIC_HLE_ACQUIRE) != 0) { 
        int val; 
        /* Wait for lock to become free again before retrying. */ 
        do { 
            _mm_pause(); 
            /* Abort speculation */ 
            val = __atomic_load_n(&lock->v, __ATOMIC_CONSUME); 
        } while (val == 1); 
    } 
}

static ALWAYS_INLINE bool hle_spinlock_isfree(spinlock_t* lock)
{
    return (__sync_bool_compare_and_swap(&lock->v, 0, 0) ? true : false);
}

static ALWAYS_INLINE void hle_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
{
    __atomic_clear(&lock->v, __ATOMIC_RELEASE|__ATOMIC_HLE_RELEASE);
}

#define _MAX_TRY_XBEGIN         10
#define _MAX_ABORT_RETRY        5

static ALWAYS_INLINE int new_rtm_spinlock_acquire (pthread_mutex_t *mutex)
{
    unsigned status;
    int abort_retry = 0;

    for(int try_xbegin = 0; try_xbegin < _MAX_TRY_XBEGIN; try_xbegin ++) {
        if ((status = _xbegin()) == _XBEGIN_STARTED) {

            // POSIX pthreads locking does not export an operation to query internal lock status
            if ((mutex)->__data.__lock == 0)
                return 0;

            // Lock was busy. Fall back to normal locking. 
            _xabort (0xff);
        }

        if (!(status & _XABORT_RETRY)) {
            if (abort_retry >= _MAX_ABORT_RETRY)
                break;
            abort_retry ++;
        }
    }

    /* Use a normal lock here.  */
    return pthread_mutex_lock(mutex);
}

static ALWAYS_INLINE int new_rtm_spinlock_release(pthread_mutex_t *mutex) {

    if ((mutex)->__data.__lock == 0) {
        _xend();
        return 0;
    }
}

static ALWAYS_INLINE void rtm_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
{
    unsigned int tm_status = 0;

tm_try:
    if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
        // If the lock is free, speculatively elide acquisition and continue. 
        if (hle_spinlock_isfree(lock)) return;

        // Otherwise fall back to the spinlock by aborting. 
        _xabort(0xff); // 0xff canonically denotes 'lock is taken'. 
    } else {
        // _xbegin could have had a conflict, been aborted, etc 
        if (tm_status & _XABORT_RETRY) {
            //__sync_add_and_fetch(&g_rtm_retries, 1);
            goto tm_try; /* Retry */
        }
        if (tm_status & _XABORT_EXPLICIT) {
            int code = _XABORT_CODE(tm_status);
            if (code == 0xff) goto tm_fail; /* Lock was taken; fallback */
        }

        //fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
tm_fail:
        //__sync_add_and_fetch(&g_locks_failed, 1);
        hle_spinlock_acquire(lock);
    }
}

static ALWAYS_INLINE void rtm_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
{
    // If the lock is still free, we'll assume it was elided. This implies we're in a transaction. 
    if (hle_spinlock_isfree(lock)) {
        //g_locks_elided += 1;
        _xend(); // Commit transaction 
    } else {
        // Otherwise, the lock was taken by us, so release it too. */
        hle_spinlock_release(lock);
    }
}

#endif /* _RTM_H_ */
