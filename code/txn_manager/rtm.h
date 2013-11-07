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

#define _mm_pause()                             \
  ({                                            \
    __asm__ __volatile__("pause" ::: "memory"); \
  })                                            \

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

//void (*dyn_spinlock_acquire)(spinlock_t*);
//void (*dyn_spinlock_release)(spinlock_t*);

void dyn_spinlock_init(spinlock_t* lock);

void hle_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock);

bool hle_spinlock_isfree(spinlock_t* lock); 

void hle_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock);

void rtm_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock);

void rtm_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock);

/* -------------------------------------------------------------------------- */
/* -- Intrusive linked list ------------------------------------------------- */

#define container_of(ptr, type, member) ({                      \
      const typeof( ((type *)0)->member ) *__mptr = (ptr);      \
      (type *)( (char *)__mptr - offsetof(type,member) ); })

typedef struct node {
  struct node* next;
} node_t;

typedef struct list { node_t root; } list_t;

void new_list(list_t* list);

void push_list(list_t* list, node_t* node);

void pop_list(list_t* list);

/* -------------------------------------------------------------------------- */
/* -- Sample threaded application #1 ---------------------------------------- */

static spinlock_t g_lock;
static int g_testval GUARDED_BY(g_lock);
static list_t g_list GUARDED_BY(g_lock);

typedef struct intobj {
  unsigned int v;
  node_t node;
} intobj_t;

typedef struct threadctx {
  int id;
} threadctx_t;

static void update_ctx(intobj_t* obj) EXCLUSIVE_LOCKS_REQUIRED(g_lock) ;

static void* runThread(void* voidctx);

int begin(int nthr);

/* -------------------------------------------------------------------------- */
/* -- Boilerplate & driver -------------------------------------------------- */

void __attribute__((constructor)) init();

#endif /* _RTM_H_ */
