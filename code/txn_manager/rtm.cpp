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

#include "rtm.h"
#include "immintrin.h"  /* for _mm_pause() */
#include <iostream>

using namespace std;

int cpu_has_rtm(void)
{
    if (__get_cpuid_max(0, NULL) >= 7) {
        unsigned a, b, c, d;
        __cpuid_count(7, 0, a, b, c, d);
        return !!(b & CPUID_RTM);
    }
    return 0;
}

int cpu_has_hle(void)
{
    if (__get_cpuid_max(0, NULL) >= 7) {
        unsigned a, b, c, d;
        __cpuid_count(7, 0, a, b, c, d);
        return !!(b & CPUID_HLE);
    }
    return 0;
}

void dyn_spinlock_init(spinlock_t* lock)
{
    lock->v = 0;
}

void hle_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
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

bool hle_spinlock_isfree(spinlock_t* lock)
{
    return (__sync_bool_compare_and_swap(&lock->v, 0, 0) ? true : false);
}

void hle_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
{
    __atomic_clear(&lock->v, __ATOMIC_RELEASE|__ATOMIC_HLE_RELEASE);
}

void rtm_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
{
    unsigned int tm_status = 0;

tm_try:
    if ((tm_status = _xbegin()) == _XBEGIN_STARTED) {
        /* If the lock is free, speculatively elide acquisition and continue. */
        if (hle_spinlock_isfree(lock)) return;

        /* Otherwise fall back to the spinlock by aborting. */
        _xabort(0xff); /* 0xff canonically denotes 'lock is taken'. */
    } else {
        /* _xbegin could have had a conflict, been aborted, etc */
        if (tm_status & _XABORT_RETRY) {
            __sync_add_and_fetch(&g_rtm_retries, 1);
            goto tm_try; /* Retry */
        }
        if (tm_status & _XABORT_EXPLICIT) {
            int code = _XABORT_CODE(tm_status);
            if (code == 0xff) goto tm_fail; /* Lock was taken; fallback */
        }

        //fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
tm_fail:
        __sync_add_and_fetch(&g_locks_failed, 1);
        hle_spinlock_acquire(lock);
    }
}

void rtm_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
{
    /* If the lock is still free, we'll assume it was elided. This implies
       we're in a transaction. */
    if (hle_spinlock_isfree(lock)) {
        g_locks_elided += 1;
        _xend(); /* Commit transaction */
    } else {
        /* Otherwise, the lock was taken by us, so release it too. */
        hle_spinlock_release(lock);
    }
}

