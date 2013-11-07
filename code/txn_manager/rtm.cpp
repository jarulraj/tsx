#ifndef _RTM_CPP_
#define _RTM_CPP_

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


/* -------------------------------------------------------------------------- */
/* -- A simple spinlock implementation with lock elision -------------------- */

void
dyn_spinlock_init(spinlock_t* lock)
{
  lock->v = 0;
}

void
hle_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
{
  while (__sync_lock_test_and_set(&lock->v, 1) != 0)
  {
    int val;
    do {
      _mm_pause();
      val = __sync_val_compare_and_swap(&lock->v, 1, 1);
    } while (val == 1);
  }
}

bool
hle_spinlock_isfree(spinlock_t* lock)
{
  return (__sync_bool_compare_and_swap(&lock->v, 0, 0) ? true : false);
}

void
hle_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
{
  __sync_lock_release(&lock->v);
}

void
rtm_spinlock_acquire(spinlock_t* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
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

#ifndef NDEBUG
    fprintf(stderr, "TSX RTM: failure; (code %d)\n", tm_status);
#endif /* NDEBUG */
  tm_fail:
    __sync_add_and_fetch(&g_locks_failed, 1);
    hle_spinlock_acquire(lock);
  }
}

void
rtm_spinlock_release(spinlock_t* lock) UNLOCK_FUNCTION(lock)
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

/* -------------------------------------------------------------------------- */
/* -- Intrusive linked list ------------------------------------------------- */

void
new_list(list_t* list)
{
  list->root.next = NULL;
}

void
push_list(list_t* list, node_t* node)
{
  node->next = list->root.next;
  list->root.next = node;
}

void
pop_list(list_t* list)
{
  list->root.next = list->root.next;
}

/* -------------------------------------------------------------------------- */
/* -- Sample threaded application #1 ---------------------------------------- */

static void
update_ctx(intobj_t* obj) EXCLUSIVE_LOCKS_REQUIRED(g_lock)
{
#ifndef NDEBUG
  /* The following print is disabled because the synchronization otherwise
     kills any possible TSX transactions. */
  /* fprintf(stderr, "Node insert: %d:0x%p\n", obj->v, obj); */
#endif /* NDEBUG */
  g_testval++;
  push_list(&g_list, &obj->node);
}

static void*
runThread(void* voidctx)
{
#define NOBJS 32
  threadctx_t* ctx = (threadctx_t*)voidctx;
  intobj_t* objs[NOBJS];

  for (int i = 0; i < NOBJS; i++) {
    objs[i] = (intobj_t*) malloc(sizeof(intobj_t));
    objs[i]->v = (i+1)*(ctx->id);
  }

  for (int i = 0; i < NOBJS; i++) {
    rtm_spinlock_acquire(&g_lock);
    //hle_spinlock_acquire(&g_lock);
    update_ctx(objs[i]);
    rtm_spinlock_release(&g_lock);
    //hle_spinlock_release(&g_lock);
  }
  return NULL;
#undef NOBJS
}

int
begin(int nthr)
{
  /* Initialize contexts */
  pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * nthr);
  threadctx_t* ctxs = (threadctx_t*) malloc(sizeof(threadctx_t) * nthr);
  if (threads == NULL || ctxs == NULL) {
    printf("ERROR: could not allocate thread structures!\n");
    return -1;
  }
  for (int i = 0; i < nthr; i++) ctxs[i].id = i+1;

  /* Spawn threads & wait */
  new_list(&g_list);
  dyn_spinlock_init(&g_lock);
  fprintf(stderr, "Creating %d threads...\n", nthr);
  for (int i = 0; i < nthr; i++) {
    if (pthread_create(&threads[i], NULL, runThread, (void*)&ctxs[i])) {
      printf("ERROR: could not create threads #%d!\n", i);
      return -1;
    }
  }
  for (int i = 0; i < nthr; i++) pthread_join(threads[i], NULL);

  /* free list contents */
  int total_entries = 0;
  node_t* cur = g_list.root.next;
  while (cur != NULL) {
    intobj_t* obj = container_of(cur, intobj_t, node);
#ifndef NDEBUG
    fprintf(stderr, "Read value (%d:0x%p): %d\n", total_entries+1, obj, obj->v);
#endif /* NDEBUG */
    cur = cur->next;
    free(obj);
    total_entries++;
  }

  fprintf(stderr, "OK, done.\n");
  fprintf(stderr, "Stats:\n");
  fprintf(stderr, "  total entries:\t\t%d\n", total_entries);
  fprintf(stderr, "  g_testval:\t\t\t%d\n", g_testval);
  fprintf(stderr, "  Successful RTM elisions:\t%d\n", g_locks_elided);
  fprintf(stderr, "  Failed RTM elisions:\t\t%d\n", g_locks_failed);
  fprintf(stderr, "  RTM retries:\t\t\t%d\n", g_rtm_retries);
  free(ctxs);
  free(threads);

  return 0;
}

/* -------------------------------------------------------------------------- */
/* -- Boilerplate & driver -------------------------------------------------- */

void __attribute__((constructor))
init()
{
  int rtm = cpu_has_rtm();
#ifndef NDEBUG
  int hle = cpu_has_hle();
  printf("TSX HLE: %s\nTSX RTM: %s\n", hle ? "YES" : "NO", rtm ? "YES" : "NO");
#endif /* NDEBUG */

  /*
  if (rtm == true) {
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
    // Iff we're using thread/addr sanitizer, fallback to the normal
    //   locks, which it can properly test/sanitize against. Clang
    //   only. 
    dyn_spinlock_acquire = &hle_spinlock_acquire;
    dyn_spinlock_release = &hle_spinlock_release;
#else
    // Otherwise, attempt elision via hardware 
    dyn_spinlock_acquire = &rtm_spinlock_acquire;
    dyn_spinlock_release = &rtm_spinlock_release;
#endif
  } else {
    dyn_spinlock_acquire = &hle_spinlock_acquire;
    dyn_spinlock_release = &hle_spinlock_release;
  }
  */

}

#endif /* __RTM_CPP_ */
