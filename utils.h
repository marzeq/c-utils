/*
utils.h - One file for the things you end up writing in every C project anyway.

Provides:

- Custom memory allocators 
- String views and builders
- Defer functionality
- Dynamic arrays, hash maps and other collections
- File utilities
- Command-line flag parsing

Inspired by modern systems programming languages and
common patterns used in contemporary C code.

Some parts were written from scratch,
while others are adapted from public-domain code by various authors.

Usage:
  #define USE_(FEATURE1)_UTILS
  #define USE_(FEATURE2)_UTILS
  #include "utils.h"
  
Or:
  #define USE_ALL_UTILS
  #include "utils.h"

See the definition of USE_ALL_UTILS for the complete feature list.

Dual-licensed under either of these:

A) The Unlicense (Public Domain)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>

B) MIT

Copyright (c) 2026 Antoni Marzec

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _UTILS_H
#define _UTILS_H

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error utils.h requires C23 or later
#endif

static_assert(sizeof(void*) == 8, "utils.h requires 64-bit pointers");

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef size_t    usz;
typedef ptrdiff_t isz;

#define U8_MAX  UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define I8_MIN  INT8_MIN
#define I8_MAX  INT8_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

#define nil nullptr

#define TODO(message) assert(0 && "TODO:" message)


#ifdef USE_ALL_UTILS
#define USE_ALLOC_UTILS
#define USE_DEFER_UTILS
#define USE_STR_UTILS
#define USE_COLLECTION_UTILS
#define USE_FILE_UTILS
#define USE_FLAGS_UTILS
#endif


// Handle dependencies between utilities.

#ifdef USE_FILE_UTILS
#define USE_STR_UTILS
#endif

#ifdef USE_FLAGS_UTILS
#define USE_STR_UTILS
#define USE_COLLECTION_UTILS
#endif

#ifdef USE_ALLOC_UTILS

#include <string.h>
#include <assert.h>

typedef struct {
  void* ctx;

  void* (*alloc)(void* ctx, usz size);
  void* (*realloc)(void* ctx, void* ptr, usz new_size);
  void  (*free)(void* ctx, void* ptr);
  void  (*reset)(void* ctx);
} allocator;

#define a_alloc(a, size) ((a).alloc((a).ctx, (size)))
#define a_new(a, T) ((T*)a_alloc((a), sizeof(T)))
#define a_realloc(a, ptr, new_size) ((a).realloc((a).ctx, (ptr), (new_size)))
#define a_free(a, ptr) ((a).free((a).ctx, (ptr)))
#define a_reset(a) ((a).reset((a).ctx))

static void* _libc_alloc(void* ctx, usz size) {
  (void)ctx;
  return malloc(size);
}

static void* _libc_realloc(void* ctx, void* ptr, usz new_size) {
  (void)ctx;

  return realloc(ptr, new_size);
}

static void _libc_free(void* ctx, void* ptr) {
  (void)ctx;
  free(ptr);
}

static void _libc_reset(void* ctx) {
  (void)ctx;
  assert(0 && "libc_allocator does not support reset");
}

typedef struct {
  void** allocations;
  usz count;
  usz capacity;
} tracking_allocator;

static bool _tracking_allocator_resize(tracking_allocator* tracker) {
  usz new_capacity = tracker->capacity * 2;

  void** new_allocations = realloc(tracker->allocations, sizeof(void*) * new_capacity);

  if (new_allocations == nil) {
    return false;
  }

  tracker->allocations = new_allocations;
  tracker->capacity = new_capacity;

  return true;
}

bool tracking_allocator_track_ptr(tracking_allocator* tracker, void* ptr) {
  if (ptr == nil) {
    return false;
  }

  if (tracker->allocations == nil) {
    tracker->capacity = 16;

    tracker->allocations = malloc(sizeof(void*) * tracker->capacity);

    if (tracker->allocations == nil) {
      tracker->capacity = 0;
      return false;
    }
  }

  if (tracker->count >= tracker->capacity) {
    if (!_tracking_allocator_resize(tracker)) {
      return false;
    }
  }

  tracker->allocations[tracker->count++ ] = ptr;

  return true;
}

void tracking_allocator_untrack_ptr(tracking_allocator* tracker, void* ptr) {
  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      tracker->allocations[i] = tracker->allocations[tracker->count - 1];

      tracker->count -= 1;

      return;
    }
  }
}

static void* _tracked_alloc(void* ctx, usz size) {
  tracking_allocator* tracker = ctx;

  void* ptr = malloc(size);

  if (ptr == nil) {
    return nil;
  }

  if (!tracking_allocator_track_ptr(tracker, ptr)) {
    free(ptr);
    return nil;
  }

  return ptr;
}

static void* _tracked_realloc(void* ctx, void* ptr, usz new_size) {
  tracking_allocator* tracker = ctx;

  if (ptr == nil) {
    void* new_ptr =
      malloc(new_size);

    if (new_ptr == nil) {
      return nil;
    }

    if (!tracking_allocator_track_ptr(tracker, new_ptr)) {
      free(new_ptr);
      return nil;
    }

    return new_ptr;
  }

  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      void* new_ptr = realloc(ptr, new_size);

      if (new_ptr == nil) {
        return nil;
      }

      tracker->allocations[i] = new_ptr;

      return new_ptr;
    }
  }

  return nil;
}

static void _tracked_free(void* ctx, void* ptr) {
  tracking_allocator* tracker = ctx;

  tracking_allocator_untrack_ptr(tracker, ptr);

  free(ptr);
}

static void _tracking_allocator_reset(void* ctx) {
  tracking_allocator* tracker = ctx;
  for (usz i = 0; i < tracker->count; i++) {
    free(tracker->allocations[i]);
  }

  free(tracker->allocations);

  tracker->allocations = nil;
  tracker->count = 0;
  tracker->capacity = 0;
}

typedef struct arena_block arena_block;
struct arena_block {
  arena_block* next;
  arena_block* prev;
  usz capacity;
  usz used;
  bool dedicated;
};

typedef struct {
  arena_block* block;
  usz size;
} arena_header;

typedef struct {
  arena_block* first;
  arena_block* current;
  usz block_size;
} arena;

#ifndef ARENA_MIN_BLOCK_SIZE
#define ARENA_MIN_BLOCK_SIZE 256
#endif

static usz _arena_align_up(usz size) {
  usz align = sizeof(void*) - 1;

  return (size + align) & ~align;
}

static arena_block* _arena_new_block(usz capacity, bool dedicated) {
  arena_block* block = malloc(sizeof(arena_block) + capacity);

  if (block == nil) {
    return nil;
  }

  block->next = nil;
  block->prev = nil;
  block->capacity = capacity;
  block->used = 0;
  block->dedicated = dedicated;

  return block;
}

static void _arena_link_after(arena_block* prev, arena_block* block) {
  block->prev = prev;
  block->next = prev->next;

  if (prev->next != nil) {
    prev->next->prev = block;
  }

  prev->next = block;
}

static void _arena_release_block(arena* arena, arena_block* block) {
  if (block->prev != nil) {
    block->prev->next = block->next;
  } else {
    arena->first = block->next;
  }

  if (block->next != nil) {
    block->next->prev = block->prev;
  }

  if (arena->current == block) {
    arena->current = block->prev;
  }

  free(block);
}

static void* _arena_alloc(void* ctx, usz size) {
  arena* a = ctx;
  size = _arena_align_up(size);

  usz total_size = _arena_align_up(sizeof(arena_header)) + size;

  if (a->block_size < ARENA_MIN_BLOCK_SIZE) {
    a->block_size = ARENA_MIN_BLOCK_SIZE;
  }

  if (a->current == nil) {
    arena_block* block = _arena_new_block(a->block_size, false);

    if (block == nil) {
      return nil;
    }

    a->first = block;
    a->current = block;
  }

  if (total_size > a->current->capacity / 2) {
    arena_block* block = _arena_new_block(total_size, true);

    if (block == nil) {
      return nil;
    }

    _arena_link_after(a->current, block);

    arena_header* header = (arena_header*)(block + 1);

    header->block = block;
    header->size = size;

    block->used = total_size;

    return header + 1;
  }

  if (a->current->used + total_size > a->current->capacity) {
    usz capacity = a->current->capacity * 2;

    if (capacity < total_size) {
      capacity = total_size;
    }

    arena_block* block = _arena_new_block(capacity, false);

    if (block == nil) {
      return nil;
    }

    _arena_link_after(a->current, block);

    a->current = block;
  }

  arena_header* header = (arena_header*)((u8*)(a->current + 1) + a->current->used);

  header->block = a->current;
  header->size = size;

  a->current->used += total_size;

  return header + 1;
}

static usz _usz_min(usz a, usz b) {
  return a < b ? a : b;
}

static void _arena_free(void* ctx, void* ptr) {
  arena* a = ctx;

  if (ptr == nil) {
    return;
  }

  arena_header* header = (arena_header*)ptr - 1;
  arena_block* block = header->block;

  usz total_size = _arena_align_up(sizeof(arena_header)) + header->size;

  u8* end = (u8*)header + total_size;
  u8* block_end = (u8*)(block + 1) + block->used;

  if (end != block_end) {
    return;
  }

  block->used -= total_size;

  if (block->used != 0) {
    return;
  }

  if (block->dedicated) {
    _arena_release_block(a, block);
  } else if (block == a->current && block->prev != nil) {
    _arena_release_block(a, block);
  }
}

static void* _arena_realloc(void* ctx, void* ptr, usz new_size) {
  if (ptr == nil) {
    return _arena_alloc(ctx, new_size);
  }

  new_size = _arena_align_up(new_size);

  arena_header* header = (arena_header*)ptr - 1;
  arena_block* block = header->block;

  usz old_size = header->size;
  usz old_total = _arena_align_up(sizeof(arena_header)) + old_size;
  usz new_total = _arena_align_up(sizeof(arena_header)) + new_size;

  u8* end = (u8*)header + old_total;
  u8* block_end = (u8*)(block + 1) + block->used;

  if (end == block_end) {
    usz used_without_this = block->used - old_total;

    if (used_without_this + new_total <= block->capacity) {
      block->used = used_without_this + new_total;
      header->size = new_size;

      return ptr;
    }
  }

  void* new_ptr = _arena_alloc(ctx, new_size);

  if (new_ptr == nil) {
    return nil;
  }

  memcpy(new_ptr, ptr, _usz_min(old_size, new_size));

  _arena_free(ctx, ptr);

  return new_ptr;
}

static void _arena_reset(void* ctx) {
  arena* a = ctx;

  arena_block* block = a->first;

  while (block != nil) {
    arena_block* next = block->next;

    free(block);

    block = next;
  }

  a->first = nil;
  a->current = nil;
}

static allocator _make_simple_allocator(void) {
  return (allocator) {
    .ctx = nil,
    .alloc = _libc_alloc,
    .realloc = _libc_realloc,
    .free = _libc_free,
    .reset = _libc_reset,
  };
}

static allocator _make_tracking_allocator(tracking_allocator* tracker) {
  return (allocator) {
    .ctx = tracker,
    .alloc = _tracked_alloc,
    .realloc = _tracked_realloc,
    .free = _tracked_free,
    .reset = _tracking_allocator_reset,
  };
}

static allocator _make_allocator_arena(arena* arena) {
  if (arena->block_size < ARENA_MIN_BLOCK_SIZE) {
    arena->block_size = ARENA_MIN_BLOCK_SIZE;
  }

  return (allocator) {
    .ctx = arena,
    .alloc = _arena_alloc,
    .realloc = _arena_realloc,
    .free = _arena_free,
    .reset = _arena_reset,
  };
}

#define _MAKE_ALLOCATOR_0() \
    _make_simple_allocator()

#define _MAKE_ALLOCATOR_1(arg) \
    _Generic((arg), \
        tracking_allocator*: _make_tracking_allocator, \
        arena*: _make_allocator_arena \
    )(arg)

#define GET_MACRO(_0, _1, NAME, ...) NAME

#define make_allocator(...) \
    GET_MACRO(_ __VA_OPT__(,) __VA_ARGS__, \
              _MAKE_ALLOCATOR_1, \
              _MAKE_ALLOCATOR_0) \
    (__VA_ARGS__)


typedef struct {
  arena_block* block;
  usz used;
} arena_save_point;

arena_save_point arena_save(arena* a) {
  return (arena_save_point){
    .block = a->current,
    .used = a->current ? a->current->used : 0,
  };
}

void arena_restore(arena* a, arena_save_point save) {
  arena_block *block = a->current;

  while (block && block != save.block) {
    arena_block *prev = block->prev;
    _arena_release_block(a, block);
    block = prev;
  }

  a->current = save.block;

  if (a->current) {
    a->current->used = save.used;
  }
}

typedef struct {
  arena* backing;
  arena_save_point save;
  allocator alloc;
} scratch;

thread_local arena _scratch_arenas[2];

static bool _scratch_has_conflict(arena* a, arena** conflicts, usz count) {
  for (usz i = 0; i < count; ++i) {
    if (conflicts[i] == a) {
      return true;
    }
  }

  return false;
}

scratch scratch_begin_with(const scratch *conflict) {
  arena *a = nil;

  for (usz i = 0; i < 2; ++i) {
    if (conflict == nil || &_scratch_arenas[i] != conflict->backing) {
      a = &_scratch_arenas[i];
      break;
    }
  }

  assert(a != nil);

  return (scratch){
    .backing= a,
    .save = arena_save(a),
    .alloc = make_allocator(a),
  };
}

scratch scratch_begin(void) {
  return scratch_begin_with(nil);
}

void scratch_end(scratch s) {
  arena_restore(s.backing, s.save);
}

#endif // USE_ALLOC_UTILS



#ifdef USE_DEFER_UTILS


#if defined(__clangd__)

// we want clangd lsp to typecheck the code but not error because we use nexted funcs
// obviously, this is not correct, because code would run immediately, but because it's
// just the lsp and not the actual compiler, it's fine
#define defer(code) code

#elif defined(__GNUC__) && !defined(__clang__) && !defined(__cplusplus)

#define _CONCAT_INTERNAL(x, y) x##y
#define _CONCAT(x, y) _CONCAT_INTERNAL(x, y)

#define _DEFER_INTERNAL(id, code)                     \
  void _CONCAT(_defer_func_, id)(void* _unused) {     \
    (void)_unused;                                    \
    code                                              \
  }                                                   \
                                                      \
  __attribute__((cleanup(_CONCAT(_defer_func_, id)))) \
  int _CONCAT(_defer_var_, id) = 0

#define defer(code) _DEFER_INTERNAL(__COUNTER__, code)

#else

#define defer(...) \
  static_assert(0, "defer is only supported with GCC that has nested functions support enabled")

#endif

#endif // USE_DEFER_UTILS



#ifdef USE_STR_UTILS

/*
Taken from tsoding's nob.h
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct {
  usz count;
  const char* data;
} str_view;

str_view str_view_from_parts(const char* data, usz count) {
  str_view sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

str_view str_view_chop_while(str_view* sv, int (*p)(int x)) {
  usz i = 0;
  while (i < sv->count && p(sv->data[i])) {
    i += 1;
  }

  str_view result = str_view_from_parts(sv->data, i);
  sv->count -= i;
  sv->data  += i;

  return result;
}

str_view str_view_chop_by_delim(str_view* sv, char delim) {
  usz i = 0;
  while (i < sv->count && sv->data[i] != delim) {
    i += 1;
  }

  str_view result = str_view_from_parts(sv->data, i);

  if (i < sv->count) {
    sv->count -= i + 1;
    sv->data  += i + 1;
  } else {
    sv->count -= i;
    sv->data  += i;
  }

  return result;
}

str_view str_view_chop_left(str_view* sv, usz n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data, n);

  sv->data  += n;
  sv->count -= n;

  return result;
}

str_view str_view_chop_right(str_view* sv, usz n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data + sv->count - n, n);

  sv->count -= n;

  return result;
}

str_view str_view_from_cstr(const char* cstr) {
  return str_view_from_parts(cstr, strlen(cstr));
}

#define str_view_eq(a, b) _Generic((a), \
  str_view: str_view_eq_sv, \
  const char*: str_view_eq_cstr, \
  char*: str_view_eq_cstr \
)(a, b)

bool str_view_eq_sv(str_view a, str_view b) {
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
}

bool str_view_eq_cstr(str_view sv, const char* cstr) {
  return str_view_eq_sv(sv, str_view_from_cstr(cstr));
}

#define str_view_ends_with(sv, suffix) _Generic((suffix), \
  str_view: str_view_ends_with_sv, \
  const char*: str_view_ends_with_cstr, \
  char*: str_view_ends_with_cstr \
)(sv, suffix)

bool str_view_ends_with_sv(str_view sv, str_view suffix) {
  if (sv.count >= suffix.count) {
    str_view sv_tail = {
      .count = suffix.count,
      .data = sv.data + sv.count - suffix.count,
    };
    return str_view_eq_sv(sv_tail, suffix);
  }
  return false;
}

bool str_view_ends_with_cstr(str_view sv, const char* cstr) {
  return str_view_ends_with_sv(sv, str_view_from_cstr(cstr));
}

#define str_view_starts_with(sv, prefix) _Generic((prefix), \
  str_view: str_view_starts_with_sv, \
  const char*: str_view_starts_with_cstr, \
  char*: str_view_starts_with_cstr \
)(sv, prefix)

bool str_view_starts_with_sv(str_view sv, str_view expected_prefix) {
  if (expected_prefix.count <= sv.count) {
    str_view actual_prefix = str_view_from_parts(sv.data, expected_prefix.count);
    return str_view_eq_sv(expected_prefix, actual_prefix);
  }

  return false;
}

bool str_view_starts_with_cstr(str_view sv, const char* cstr) {
  return str_view_starts_with_sv(sv, str_view_from_cstr(cstr));
}

#define str_view_chop_prefix(sv, prefix) _Generic((prefix), \
  str_view: str_view_chop_prefix_sv, \
  const char*: str_view_chop_prefix_cstr, \
  char*: str_view_chop_prefix_cstr \
)(sv, prefix)

bool str_view_chop_prefix_sv(str_view* sv, str_view prefix) {
  if (str_view_starts_with_sv(*sv, prefix)) {
    str_view_chop_left(sv, prefix.count);
    return true;
  }
  return false;
}

bool str_view_chop_prefix_cstr(str_view* sv, const char* prefix) {
  return str_view_chop_prefix_sv(sv, str_view_from_cstr(prefix));
}

#define str_view_chop_suffix(sv, suffix) _Generic((suffix), \
  str_view: str_view_chop_suffix_sv, \
  const char*: str_view_chop_suffix_cstr, \
  char*: str_view_chop_suffix_cstr \
)(sv, suffix)

bool str_view_chop_suffix_sv(str_view* sv, str_view suffix) {
  if (str_view_ends_with(*sv, suffix)) {
    str_view_chop_right(sv, suffix.count);
    return true;
  }
  return false;
}

bool str_view_chop_suffix_cstr(str_view* sv, const char* suffix) {
  return str_view_chop_suffix_sv(sv, str_view_from_cstr(suffix));
}

str_view str_view_trim_left(str_view sv) {
  usz i = 0;
  while (i < sv.count && isspace(sv.data[i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data + i, sv.count - i);
}

str_view str_view_trim_right(str_view sv) {
  usz i = 0;
  while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data, sv.count - i);
}

str_view str_view_trim(str_view sv) {
  return str_view_trim_right(str_view_trim_left(sv));
}

int str_view_find(str_view sv, char c) {
  for (usz i = 0; i < sv.count; ++i) {
    if (sv.data[i] == c) {
      return (int)i;
    }
  }
  return -1;
}

typedef struct {
  char* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTILS
  allocator alloc;
#endif
} str_builder;

#define sfmt "%.*s"

#define sfmtarg(sv) (int)(sv).count, (sv).data

#ifdef USE_ALLOC_UTILS

static void _str_builder_ensure_allocator(str_builder* sb) {
  if (sb->alloc.alloc == nil) {
    sb->alloc = make_allocator();
  }
}

#define _str_builder_realloc(sb, new_capacity) \
  sb->alloc.realloc(sb->alloc.ctx, sb->data, new_capacity)

#define _str_builder_free(sb, ptr) \
  do { \
    if ((ptr) != nil) { \
      sb->alloc.free(sb->alloc.ctx, (ptr)); \
    } \
  } while (0)

#else

static void _str_builder_ensure_allocator(str_builder* sb) {
  (void)sb;
}

#define _str_builder_realloc(sb, new_capacity) \
  realloc(sb->data, new_capacity)

#define _str_builder_free(sb, ptr) \
  free(ptr)

#endif

bool str_builder_reserve(str_builder* sb, usz additional) {
  usz required = sb->count + additional + 1;

  if (required <= sb->capacity) {
    return true;
  }

  usz new_capacity = sb->capacity > 0 ? sb->capacity : 64;

  while (new_capacity < required) {
    usz next = new_capacity * 2;

    if (next <= new_capacity) {
      return false;
    }

    new_capacity = next;
  }

  _str_builder_ensure_allocator(sb);

  char* new_data = _str_builder_realloc(sb, new_capacity);

  if (new_data == nil) {
    return false;
  }

  sb->data = new_data;
  sb->capacity = new_capacity;

  return true;
}

bool str_builder_append_bytes(str_builder* sb, const void* data, usz size) {
  if (!str_builder_reserve(sb, size)) {
    return false;
  }

  memcpy(sb->data + sb->count, data, size);

  sb->count += size;

  sb->data[sb->count] = '\0';

  return true;
}

bool str_builder_append_cstr(str_builder* sb, const char* cstr) {
  return str_builder_append_bytes(sb, cstr, strlen(cstr));
}

bool str_builder_append_sb(str_builder* sb, const str_builder* other) {
  return str_builder_append_bytes(sb, other->data, other->count);
}

bool _str_builder_append_sb_value(str_builder* sb, str_builder other) {
  return str_builder_append_sb(sb, &other);
}

bool str_builder_append_sv(str_builder* sb, str_view sv) {
  return str_builder_append_bytes(sb, sv.data, sv.count);
}

str_view str_builder_view(const str_builder* sb) {
  return str_view_from_parts(sb->data ? sb->data : "", sb->count);
}

#define str_builder_append(sb, data)           \
  _Generic((data),                             \
    char*: str_builder_append_cstr,            \
    const char*: str_builder_append_cstr,      \
    str_builder: _str_builder_append_sb_value, \
    str_builder*: str_builder_append_sb,       \
    const str_builder*: str_builder_append_sb, \
    str_view: str_builder_append_sv            \
  )(sb, data)

void str_builder_clear(str_builder* sb) {
  sb->count = 0;

  if (sb->data != nil) {
    sb->data[0] = '\0';
  }
}

void str_builder_free(str_builder* sb) {
  _str_builder_ensure_allocator(sb);

  _str_builder_free(sb, sb->data);

  sb->data = nil;
  sb->count = 0;
  sb->capacity = 0;
}

#endif // USE_STR_UTILS



#ifdef USE_COLLECTION_UTILS

#include <string.h>

#ifdef USE_ALLOC_UTILS
#define _COLLECTION_ALLOC_FIELD allocator alloc;
#else
#define _COLLECTION_ALLOC_FIELD
#endif

typedef struct {
  void* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTILS
  allocator alloc;
#endif
} _da_base;

#define da(T) struct {    \
  T* data;                \
  usz count;              \
  usz capacity;           \
  _COLLECTION_ALLOC_FIELD \
}

#ifdef USE_ALLOC_UTILS

void _da_base_ensure_allocator(_da_base* arr) {
  if (arr->alloc.alloc == nil) {
    arr->alloc = make_allocator();
  }
}

#define _da_base_realloc(arr, elem_size, new_capacity) \
  arr->alloc.realloc(arr->alloc.ctx, arr->data, new_capacity * elem_size)

#define _da_base_free(arr) \
  do { \
    if ((arr)->data != nil) { \
      (arr)->alloc.free((arr)->alloc.ctx, (arr)->data); \
    } \
  } while (0)

#else

void _da_base_ensure_allocator(_da_base* arr) {
  (void)arr;
}

#define _da_base_realloc(arr, elem_size, new_capacity) \
  realloc(arr->data, new_capacity * elem_size)

#define _da_base_free(arr) \
  free((arr)->data)

#endif

void* _da_base_resize(_da_base* arr, usz elem_size, usz new_capacity) {
  _da_base_ensure_allocator(arr);

  return _da_base_realloc(arr, elem_size, new_capacity);
}

bool _da_base_append_impl(_da_base* arr, void* value, usz elem_size) {
  if (arr->count >= arr->capacity) {
    usz new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 4;

    void* new_data = _da_base_resize(arr, elem_size, new_capacity);

    if (new_data == nil) {
      return false;
    }

    arr->data = new_data;
    arr->capacity = new_capacity;
  }

  memcpy((u8*)arr->data + arr->count * elem_size, value, elem_size);

  arr->count += 1;

  return true;
}

void _da_free(_da_base* arr) {
  _da_base_ensure_allocator(arr);

  _da_base_free(arr);

  arr->data = nil;
  arr->count = 0;
  arr->capacity = 0;
}

#define da_append(arr, value)            \
  ({                                     \
    typeof(*(arr)->data) _tmp = (value); \
    _da_base_append_impl(                \
      (_da_base*)(arr),                  \
      &_tmp,                             \
      sizeof(_tmp)                       \
    );                                   \
  })

#define da_at(arr, index) ((arr)->data[(index)])

#define da_last(arr) ((arr)->data[(arr)->count - 1])

#define da_free(arr) _da_free((_da_base*)(arr))

#define _DA_FOREACH_1(arr) \
  _DA_FOREACH_2(arr, it)

#define _DA_FOREACH_2(arr, it)                                     \
  for (usz _i = 0; _i < (arr)->count; ++_i)                        \
    for (typeof(*(arr)->data) it = (arr)->data[_i], *_once = &it;  \
         _once != nil;                                             \
         _once = nil)

#define _DA_FOREACH_GET(_1, _2, NAME, ...) NAME

#define da_foreach(...) \
  _DA_FOREACH_GET(__VA_ARGS__, _DA_FOREACH_2, _DA_FOREACH_1)(__VA_ARGS__)

#define _DA_FOREACH_I_1(arr) \
  _DA_FOREACH_I_3(arr, idx, it)

#define _DA_FOREACH_I_3(arr, i, it)                                \
  for (usz i = 0; i < (arr)->count; ++i)                           \
    for (typeof(*(arr)->data) it = (arr)->data[i], *_once = &it;   \
         _once != nil;                                             \
         _once = nil)

#define _DA_FOREACH_I_GET(_1, _2, _3, NAME, ...) NAME

#define da_foreach_i(...) \
  _DA_FOREACH_I_GET(__VA_ARGS__, _DA_FOREACH_I_3, _, _DA_FOREACH_I_1)(__VA_ARGS__)


typedef struct {
  void *data;
  usz count;
  usz capacity;
  _COLLECTION_ALLOC_FIELD
} _map_base;

#ifdef USE_ALLOC_UTILS

static void _map_ensure_allocator(_map_base *map) {
  if (map->alloc.alloc == nil) {
    map->alloc = make_allocator();
  }
}

#define _map_alloc(map, size) \
  (map)->alloc.alloc((map)->alloc.ctx, (size))

#define _map_free_mem(map, ptr) \
  (map)->alloc.free((map)->alloc.ctx, (ptr))

#else

static void _map_ensure_allocator(_map_base *map) {
  (void)map;
}

#define _map_alloc(map, size) malloc(size)

#define _map_free_mem(map, ptr) free(ptr)

#endif

#define _map_entry(T) struct { \
  const char* key;             \
  T value;                     \
  bool occupied;               \
}

#define map(T) struct {   \
  _map_entry(T)* data;    \
  usz count;              \
  usz capacity;           \
  _COLLECTION_ALLOC_FIELD \
}

static u64 _map_hash(const char *str) {
  u64 hash = 14695981039346656037ull;

  while (*str) {
    hash ^= (u8)*str;
    hash *= 1099511628211ull;
    str += 1;
  }

  return hash;
}

static bool _map_init_slots(_map_base* map, usz elem_size, usz capacity) {
  _map_ensure_allocator(map);

  void* data = _map_alloc(map, capacity * elem_size);

  if (data == nil) {
    return false;
  }

  memset(data, 0, capacity * elem_size);

  map->data = data;
  map->capacity = capacity;
  map->count = 0;

  return true;
}

static bool _map_insert_no_rehash(
  _map_base* map,
  const char* key,
  void* value,
  usz elem_size
) {
  usz cap = map->capacity;
  u8* base = map->data;

  usz idx = _map_hash(key) % cap;
  usz probes = 0;

  while (probes < cap) {
    void* entry = base + idx * elem_size;

    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      memcpy(entry, &key, sizeof key);

      memcpy(
        (u8*)entry + sizeof(char*),
        value,
        elem_size - sizeof(char*) - sizeof(bool)
      );

      *occupied = true;
      map->count += 1;

      return true;
    }

    idx += 1;

    if (idx >= cap) {
      idx = 0;
    }

    probes += 1;
  }

  return false;
}

static bool _map_rehash(_map_base* map, usz elem_size, usz new_capacity) {
  _map_base new_map = {0};

#ifdef USE_ALLOC_UTILS
  new_map.alloc = map->alloc;
#endif

  if (!_map_init_slots(&new_map, elem_size, new_capacity)) {
    return false;
  }

  u8* base = map->data;

  for (usz i = 0; i < map->capacity; i += 1) {
    void* entry = base + i * elem_size;

    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (*occupied) {
      const char* key = *(const char**)entry;

      void* value = (u8*)entry + sizeof(char*);

      if (!_map_insert_no_rehash(
        &new_map,
        key,
        value,
        elem_size
      )) {

        _map_free_mem(&new_map, new_map.data);

        return false;
      }
    }
  }

  if (map->data != nil) {
    _map_free_mem(map, map->data);
  }

  map->data = new_map.data;
  map->count = new_map.count;
  map->capacity = new_map.capacity;

  return true;
}

static bool _map_insert_impl(
  _map_base* map,
  const char* key,
  void* value,
  usz elem_size
) {
  if (map->capacity == 0) {
    if (!_map_init_slots(map, elem_size, 16)) {
      return false;
    }
  }

  if ((map->count + 1) * 10 >= map->capacity * 7) {
    if (!_map_rehash(
      map,
      elem_size,
      map->capacity * 2
    )) {
      return false;
    }
  }

  usz cap = map->capacity;
  u8* base = map->data;

  usz idx = _map_hash(key) % cap;
  usz probes = 0;

  while (probes < cap) {
    void* entry = base + idx * elem_size;

    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      memcpy(entry, &key, sizeof key);

      memcpy(
        (u8*)entry + sizeof(char*),
        value,
        elem_size - sizeof(char*) - sizeof(bool)
      );

      *occupied = true;

      map->count += 1;

      return true;
    }

    const char* existing = *(const char**)entry;

    if (strcmp(existing, key) == 0) {
      memcpy(
        (u8*)entry + sizeof(char*),
        value,
        elem_size - sizeof(char*) - sizeof(bool)
      );

      return true;
    }

    idx += 1;

    if (idx >= cap) {
      idx = 0;
    }

    probes += 1;
  }

  return false;
}

static void* _map_get_impl(_map_base* map, const char* key, usz elem_size) {
  if (map->capacity == 0) {
    return nil;
  }

  usz idx = _map_hash(key) % map->capacity;
  usz probes = 0;

  u8* base = map->data;

  while (probes < map->capacity) {
    void* entry = base + idx * elem_size;

    bool* occupied = (bool*)((u8*)entry + elem_size - sizeof(bool));

    if (!*occupied) {
      return nil;
    }

    const char* existing = *(const char**)entry;

    if (strcmp(existing, key) == 0) {
      return (u8*)entry + sizeof(char*);
    }

    idx += 1;

    if (idx >= map->capacity) {
      idx = 0;
    }

    probes += 1;
  }

  return nil;
}

static void _map_free(_map_base* map) {
  _map_ensure_allocator(map);

  if (map->data != nil) {
    _map_free_mem(map, map->data);
  }

  map->data = nil;
  map->count = 0;
  map->capacity = 0;
}

#define map_insert(map, k, v) ({           \
  typeof((map)->data[0].value) _tmp = (v); \
  _map_insert_impl(                        \
    (_map_base*)(map),                     \
    (k),                                   \
    &_tmp,                                 \
    sizeof((map)->data[0])                 \
  );                                       \
})

#define map_get(map, k)            \
  ((typeof(&(map)->data[0].value)) \
    _map_get_impl(                 \
      (_map_base*)(map),           \
      (k),                         \
      sizeof((map)->data[0])       \
    ))

#define map_free(map) \
  _map_free((_map_base*)(map))


#endif // USE_COLLECTION_UTILS


#ifdef USE_FILE_UTILS

#include <stdio.h>

bool read_entire_file(const char* path, str_builder* sb) {
  FILE* f = fopen(path, "rb");

  if (f == nil) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);

  if (size < 0) {
    fclose(f);
    return false;
  }

  rewind(f);

  str_builder_clear(sb);

  if (!str_builder_reserve(sb, (usz)size)) {
    fclose(f);
    return false;
  }

  usz read = fread(sb->data, 1, (usz)size, f);

  fclose(f);

  if (read != (usz)size) {
    return false;
  }

  sb->count = read;
  sb->data[sb->count] = '\0';

  return true;
}

bool write_entire_file_cstr(const char* path, const char* data) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz size = strlen(data);

  usz written = fwrite(data, 1, size, f);

  fclose(f);

  return written == size;
}

bool write_entire_file_sv(const char* path, str_view sv) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sv.data, 1, sv.count, f);

  fclose(f);

  return written == sv.count;
}


bool write_entire_file_sv_ptr(const char* path, str_view* sv) {
  if (sv == nil) {
    return false;
  }

  return write_entire_file_sv(
    path,
    *sv
  );
}

bool write_entire_file_sb(const char* path, str_builder sb) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sb.data, 1, sb.count, f);

  fclose(f);

  return written == sb.count;
}

bool write_entire_file_sb_ptr(const char* path, str_builder* sb) {
  if (sb == nil) {
    return false;
  }

  return write_entire_file_sb(path, *sb);
}

#define write_entire_file(path, data)            \
  _Generic((data),                               \
    char*: write_entire_file_cstr,               \
    const char*: write_entire_file_cstr,         \
    str_view: write_entire_file_sv,              \
    str_view*: write_entire_file_sv_ptr,         \
    const str_view*: write_entire_file_sv_ptr,   \
    str_builder: write_entire_file_sb,           \
    str_builder*: write_entire_file_sb_ptr,      \
    const str_builder*: write_entire_file_sb_ptr \
  )(path, data)

#endif // USE_FILE_UTILS


#ifdef USE_FLAGS_UTILS

#include <limits.h>
#include <assert.h>

typedef enum flag_type {
  BOOL,
  STRING,
  NUMBER,
} flag_type;

typedef struct {
  const char* name;
  const char* desc;
  flag_type type;
  union {
    bool bool_value;
    str_view string_value;
    int number_value;
  } value;
  bool is_set;
} flag;

#define get_flag(pvalue) ((flag*)((char*)(pvalue) - offsetof(flag, value)))
#define flag_is_set(pvalue) (get_flag(pvalue)->is_set)
#define flag_name(pvalue) (get_flag(pvalue)->name)
#define flag_desc(pvalue) (get_flag(pvalue)->desc)

#ifndef FLAGS_MAX_FLAGS
#define FLAGS_MAX_FLAGS 64
#endif

static_assert(FLAGS_MAX_FLAGS > 0, "FLAGS_MAX_FLAGS must be greater than 0");

typedef struct flags {
  const char* positional_args_req;

  flag flags[FLAGS_MAX_FLAGS];
  usz flags_count;

  da(str_view) positional_args;

  bool got_help;

  bool _parsed;
  bool failed_adding;

#ifdef USE_ALLOC_UTILS
  allocator alloc;
#endif
} flags;

#define add_flag(a, name, description, def) \
  _Generic((def),                           \
    char*: _add_flag_string,                \
    const char*: _add_flag_string,          \
    str_view: _add_flag_str_view,           \
    int: _add_flag_int,                     \
    bool: _add_flag_bool                    \
  )(a, name, description, def)

static void* _add_flag(flags* f, const char* name, const char* description, flag_type type) {
  if (f->flags_count >= FLAGS_MAX_FLAGS) {
    fprintf(
      stderr,
      "Maximum number of flags exceeded (%d). "
      "#define FLAGS_MAX_FLAGS before including utils.h to increase this limit.\n",
      FLAGS_MAX_FLAGS
    );
    f->failed_adding = true;
    return nil;
  }

  if (f->_parsed) {
    fprintf(stderr, "cannot add flags after parsing\n");
    f->failed_adding = true;
    return nil;
  }

  if (name == nil || description == nullptr) {
    fprintf(stderr, "flag name and/or description cannot be null\n");
    f->failed_adding = true;
    return nil;
  }

  for (const char* p = name; *p != '\0'; p++) {
    if (*p == '=') {
      fprintf(stderr, "flag name cannot contain '=': %s\n", name);
      f->failed_adding = true;
      return nil;
    }
  }

  if (strcmp(name, "h") == 0) {
    fprintf(stderr, "-h is reserved for help\n");
    f->failed_adding = true;
    return nil;
  }

  for (usz i = 0; i < f->flags_count; i++) {
    if (strcmp(f->flags[i].name, name) == 0) {
      fprintf(stderr, "duplicate flag name: %s\n", name);
      f->failed_adding = true;
      return nil;
    }
  }

  flag* flag = &f->flags[f->flags_count];
  flag->name = name;
  flag->desc = description;
  flag->type = type;
  flag->is_set = false;
  f->flags_count += 1;
  return &f->flags[f->flags_count - 1].value;
}

str_view* _add_flag_string(flags* f, const char* name, const char* description, const char* def) {
  if (def == nil) {
    def = "";
  }
  void* got = _add_flag(f, name, description, STRING);
  if (!got) {
    return nil;
  }
  f->flags[f->flags_count - 1].value.string_value = str_view_from_cstr(def);
  return (str_view*)got;
}

str_view* _add_flag_str_view(flags* f, const char* name, const char* description, str_view def) {
  void* got = _add_flag(f, name, description, STRING);
  if (!got) {
    return nil;
  }
  f->flags[f->flags_count - 1].value.string_value = def;
  return (str_view*)got;
}

int* _add_flag_int(flags* f, const char* name, const char* description, int def) {
  void* got = _add_flag(f, name, description, NUMBER);
  if (!got) {
    return nil;
  }
  f->flags[f->flags_count - 1].value.number_value = def;
  return (int*)got;
}

bool* _add_flag_bool(flags* f, const char* name, const char* description, bool def) {
  void* got = _add_flag(f, name, description, BOOL);
  if (!got) {
    return nil;
  }
  f->flags[f->flags_count - 1].value.bool_value = def;
  return (bool*)got;
}

static usz _null_term_array_len(const void** arr) {
  usz len = 0;
  while (arr[len] != nil) {
    len += 1;
  }
  return len;
}

void flags_reset(flags* f) {
  f->flags_count = 0;

  da_free(&f->positional_args);

  f->got_help = false;

  f->_parsed = false;
}

static bool _is_flag(const str_view flag) {
  return flag.count > 0 && flag.data[0] == '-';
}

static bool _str_startswith(const char* str, const char* prefix) {
  usz str_len = strlen(str);
  usz prefix_len = strlen(prefix);
  return str_len >= prefix_len && strncmp(str, prefix, prefix_len) == 0;
}

static bool _add_positional_arg(flags* f, const str_view flag) {
#ifdef USE_ALLOC_UTILS
  if (f->positional_args.alloc.alloc == nil) {
    if (f->alloc.alloc == nil) {
      f->alloc = make_allocator();
    }
    f->positional_args.alloc = f->alloc;
  }
#endif
  return da_append(&f->positional_args, flag);
}

static bool _parse_int(str_view sv, int *out) {
  if (sv.count == 0) {
    return false;
  }

  bool negative = false;
  usz i = 0;

  if (sv.data[0] == '-') {
    negative = true;
    i = 1;
    if (i == sv.count) {
      return false;
    }
  }

  long value = 0;

  for (; i < sv.count; i++) {
    char c = sv.data[i];
    if (c < '0' || c > '9') {
      return false;
    }

    value = value * 10 + (c - '0');

    if ((!negative && value > INT_MAX) || (negative && -value < INT_MIN)) {
      return false;
    }
  }

  *out = negative ? -(int)value : (int)value;
  return true;
}

static bool _set_flag_value(flags* f, flag* flag, const str_view sv) {
  if (flag->is_set) {
    fprintf(stderr, "flag -%s specified multiple times\n", flag->name);
    return false;
  }

  switch (flag->type) {
    case BOOL: {
      if (sv.count > 0) {
        fprintf(stderr, "boolean flag -%s does not take a value\n", flag->name);
        return false;
      }

      flag->value.bool_value = true;
      break;
    }
    case STRING: {
      flag->value.string_value = sv;
      break;
    }
    case NUMBER: {
      int value;
      if (!_parse_int(sv, &value)) {
        fprintf(stderr, "invalid integer value for flag -%s: '" sfmt "'\n", flag->name, sfmtarg(sv));
        return false;
      }

      flag->value.number_value = (int)value;
      break;
    }
  }

  flag->is_set = true;

  return true;
}

void flags_print_help(flags* f, const char* prog_name) {
  printf("Usage: %s [options]", prog_name);
  if (f->positional_args_req) {
    if (strcmp(f->positional_args_req, "+") == 0) {
      printf(" <arg1> [arg2] ...");
    } else if (strcmp(f->positional_args_req, "?") == 0) {
      printf(" [arg]");
    } else if (strcmp(f->positional_args_req, "*") == 0) {
      printf(" [arg1] [arg2] ...");
    } else {
      printf(" ");
      int expected = atoi(f->positional_args_req);
      for (long j = 0; j < expected; j++) {
        printf("<arg%ld> ", j + 1);
      }
    }
  }

  printf("\n");

  if (f->flags_count > 0) {
    printf("\nOptions:\n");

    usz max_name_len = 0;
    for (usz j = 0; j < f->flags_count; j++) {
      usz len = strlen(f->flags[j].name);
      if (len > max_name_len) {
        max_name_len = len;
      }
    }

    for (usz j = 0; j < f->flags_count; j++) {
      flag* flag = &f->flags[j];
      printf("  -%-*s  %s", (int)max_name_len, flag->name, flag->desc);
      switch (flag->type) {
        case STRING:
          if (flag->value.string_value.count > 0) {
            printf(" (default: " sfmt ")", sfmtarg(flag->value.string_value));
          }
          break;
        case NUMBER:
          printf(" (default: %d)", flag->value.number_value);
          break;
        default:
          break;
      }
      printf("\n");
    }
  }
}

bool flags_parse(flags* f, int flagc, char** flagv) {
  if (!f->positional_args_req) {
  } else if (strcmp(f->positional_args_req, "+") == 0) {
  } else if (strcmp(f->positional_args_req, "?") == 0) {
  } else if (strcmp(f->positional_args_req, "*") == 0) {
  } else {
    int expected = atoi(f->positional_args_req);
    if (expected < 0) {
      fprintf(stderr, "invalid positional_args_req: %s\n", f->positional_args_req);
      return false;
    }
  }

  for (int i = 0; i < flagc; i++) {
    if (strcmp(flagv[i], "-h") == 0) {
      f->got_help = true;
      return true;
    }
  }

  for (int i = 1; i < flagc; i++) {
    str_view got = str_view_from_cstr(flagv[i]);
    if (!_is_flag(got)) {
      if (!_add_positional_arg(f, got)) {
        return false;
      }
      continue;
    }

    // skip the leading '-'
    str_view_chop_left(&got, 1);
    
    bool found = false;
    for (usz j = 0; j < f->flags_count; j++) {
      flag* flag = &f->flags[j];
      // -flag value syntax
      if (str_view_eq_cstr(got, flag->name)) {
        found = true;
        str_view value = {0};

        if (flag->type != BOOL) {
          if (i + 1 >= flagc) {
            fprintf(stderr, "flag -" sfmt " requires a value\n", sfmtarg(got));
            return false;
          }

          value = str_view_from_cstr(flagv[i + 1]);
          i += 1;
        }

        if (!_set_flag_value(f, &f->flags[j], value)) {
          return false;
        }
        break;
      }

      usz candidate_len = strlen(flag->name);
      if (!str_view_starts_with_cstr(got, flag->name)) {
        continue;
      }

      str_view suffix = got;
      str_view_chop_left(&suffix, candidate_len);

      if (suffix.count == 0 || suffix.data[0] != '=') {
        continue;
      }

      // -flag=value syntax
      found = true;
      str_view value = suffix;
      str_view_chop_left(&value, 1);

      if (!_set_flag_value(f, &f->flags[j], value)) {
        return false;
      }
    }

    if (!found) {
      int equal_sign = str_view_find(got, '=');

      if (equal_sign != -1) {
        str_view flag_name = got;
        flag_name.count = (usz)equal_sign;

        fprintf(stderr, "unknown flag -" sfmt "\n", sfmtarg(flag_name));
        return false;
      } else {
        fprintf(stderr, "unknown flag -" sfmt "\n", sfmtarg(got));
        return false;
      }
    }
  }

  if (!f->positional_args_req) {
    // unspecified, assume 0
    if (f->positional_args.count > 0) {
      fprintf(stderr, "expected no positional arguments, got %zu\n", f->positional_args.count);
      return false;
    }
  } else if (strcmp(f->positional_args_req, "+") == 0) {
    if (f->positional_args.count == 0) {
      fprintf(stderr, "expected at least one positional argument\n");
      return false;
    }
  } else if (strcmp(f->positional_args_req, "?") == 0) {
    if (f->positional_args.count > 1) {
      fprintf(stderr, "expected at most one positional argument\n");
      return false;
    }
  } else if (strcmp(f->positional_args_req, "*") == 0) {
    // any number of positional arguments is allowed
  } else {
    // expected to be a number
    int expected = atoi(f->positional_args_req);
    if (f->positional_args.count != (usz)expected) {
      fprintf(stderr, "expected %d positional arguments, got %zu\n", expected, f->positional_args.count);
      return false;
    }
  }

  f->_parsed = true;
  return true;
}

#endif // USE_FLAGS_UTILS

#endif // _UTILS_H
