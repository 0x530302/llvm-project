// Test the behavior of malloc/calloc/realloc/new when the allocation size is
// more than TSan allocator's max allowed one.
// By default (allocator_may_return_null=0) the process should crash.
// With allocator_may_return_null=1 the allocator should return 0, except the
// operator new(), which should crash anyway (operator new(std::nothrow) should
// return nullptr, indeed).
//
// RUN: %clangxx_tsan -O0 %s -o %t
// RUN: not %run %t malloc 2>&1 | FileCheck %s --check-prefix=CHECK-mCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mNULL
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t calloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-cCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t calloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-cNULL
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t calloc-overflow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-coCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t calloc-overflow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-coNULL
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t realloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-rCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t realloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-rNULL
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t realloc-after-malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mrCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t realloc-after-malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mrNULL
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t new 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1 not %run %t new 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=0 not %run %t new-nothrow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nnCRASH
// RUN: %env_tsan_opts=allocator_may_return_null=1     %run %t new-nothrow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nnNULL

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <new>

int main(int argc, char **argv) {
  // Disable stderr buffering. Needed on Windows.
  setvbuf(stderr, NULL, _IONBF, 0);

  assert(argc == 2);
  const char *action = argv[1];
  fprintf(stderr, "%s:\n", action);

  static const size_t kMaxAllowedMallocSizePlusOne = (1ULL << 40) + 1;

  void *x = 0;
  if (!strcmp(action, "malloc")) {
    x = malloc(kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "calloc")) {
    x = calloc((kMaxAllowedMallocSizePlusOne / 4) + 1, 4);
  } else if (!strcmp(action, "calloc-overflow")) {
    volatile size_t kMaxSizeT = std::numeric_limits<size_t>::max();
    size_t kArraySize = 4096;
    volatile size_t kArraySize2 = kMaxSizeT / kArraySize + 10;
    x = calloc(kArraySize, kArraySize2);
  } else if (!strcmp(action, "realloc")) {
    x = realloc(0, kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "realloc-after-malloc")) {
    char *t = (char*)malloc(100);
    *t = 42;
    x = realloc(t, kMaxAllowedMallocSizePlusOne);
    assert(*t == 42);
  } else if (!strcmp(action, "new")) {
    x = operator new(kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "new-nothrow")) {
    x = operator new(kMaxAllowedMallocSizePlusOne, std::nothrow);
  } else {
    assert(0);
  }

  // The NULL pointer is printed differently on different systems, while (long)0
  // is always the same.
  fprintf(stderr, "x: %lx\n", (long)x);
  free(x);
  return x != 0;
}

// CHECK-mCRASH: malloc:
// CHECK-mCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-cCRASH: calloc:
// CHECK-cCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-coCRASH: calloc-overflow:
// CHECK-coCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-rCRASH: realloc:
// CHECK-rCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-mrCRASH: realloc-after-malloc:
// CHECK-mrCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-nCRASH: new:
// CHECK-nCRASH: ThreadSanitizer's allocator is terminating the process
// CHECK-nnCRASH: new-nothrow:
// CHECK-nnCRASH: ThreadSanitizer's allocator is terminating the process

// CHECK-mNULL: malloc:
// CHECK-mNULL: x: 0
// CHECK-cNULL: calloc:
// CHECK-cNULL: x: 0
// CHECK-coNULL: calloc-overflow:
// CHECK-coNULL: x: 0
// CHECK-rNULL: realloc:
// CHECK-rNULL: x: 0
// CHECK-mrNULL: realloc-after-malloc:
// CHECK-mrNULL: x: 0
// CHECK-nnNULL: new-nothrow:
// CHECK-nnNULL: x: 0
