/*
utils.c - Random C utilities to make it more bearable to work with. Made for my usecases.

Some of these come from other people licensed in the public domain, some I wrote myself.

Licensed in the public domain. Do whatever you want with it.
*/

#ifndef _UTILS_C
#define _UTILS_C

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error utils.c requires C23 or later
#endif

static_assert(sizeof(void*) == 8, "utils.c requires 64-bit pointers");

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

typedef size_t   usz;
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

#define nil NULL

#define TODO(message) assert(0 && "TODO:" message)


#ifdef USE_ALL_UTILS
#define USE_ALLOC_UTILS
#define USE_DEFER_UTILS
#define USE_STR_UTILS
#define USE_DA_UTILS
#define USE_FILE_UTILS
#endif


// Handle dependencies between utilities.
#ifdef USE_FILE_UTILS
#define USE_STR_UTILS
#endif

#ifdef USE_ALLOC_UTILS

#include <string.h>
#include <assert.h>

// <@
// @name allocator
// @kind type
// @desc A struct that represents a memory allocator. It contains function pointers for allocating,
// reallocating and freeing memory, as well as a context pointer that can be used to store any state the allocator needs.
// @field ctx A pointer to any state the allocator needs. This is passed to the alloc, realloc and free functions.
// @field alloc A function pointer to a function that allocates memory.
// @field realloc A function pointer to a function that reallocates memory.
// @field free A function pointer to a function that frees memory.
typedef struct {
  void* ctx;

  void* (*alloc)(void* ctx, usz size);
  void* (*realloc)(void* ctx, void* ptr, usz new_size);
  void (*free)(void* ctx, void* ptr);
  void (*reset)(void* ctx);
} allocator;
// @>

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

// <@
// @name tracking_allocator
// @kind type
// @desc An allocator that tracks all allocations made through it.
// @warning This allocator is not thread-safe and should only be used in a single-threaded context.
// @>
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

// <@
// @name tracking_allocator_track_ptr
// @kind function
// @desc Tracks a pointer in the tracking_allocator. This should be called whenever memory is allocated using the tracked allocator.
// @return true if the pointer was successfully tracked, false if there was an error (e.g. out of memory).
// @param tracker The tracking_allocator to track the pointer in.
// @param ptr The pointer to track.
bool tracking_allocator_track_ptr(tracking_allocator* tracker, void* ptr) {
// @>
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

// <@
// @name tracking_allocator_untrack_ptr
// @kind function
// @desc Untracks a pointer in the tracking_allocator.
// @param tracker The tracking_allocator to untrack the pointer from.
// @param ptr The pointer to untrack.
void tracking_allocator_untrack_ptr(tracking_allocator* tracker, void* ptr) {
// @>
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

// <@
// @name arena
// @kind type
// @desc A memory arena that allocates memory in blocks and allows for efficient allocation and deallocation of memory.
// @kind struct
// @>
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

// <@
// @name make_allocator
// @kind macro
// @desc Creates an allocator. If called with no arguments, it creates a simple allocator that uses malloc and free.
// If called with a tracking_allocator pointer, it creates a tracking allocator.
// If called with an arena pointer, it creates an arena allocator.
// @param ... Optional argument: a pointer to a tracking_allocator or an arena.
// @>
#define make_allocator(...) \
    GET_MACRO(_ __VA_OPT__(,) __VA_ARGS__, \
              _MAKE_ALLOCATOR_1, \
              _MAKE_ALLOCATOR_0) \
    (__VA_ARGS__)


typedef struct {
  arena_block* block;
  usz used;
} arena_save_point;

// <@
// @name arena_save
// @kind function
// @desc Saves the current allocation position of an arena.
// @param a The arena to save.
// @return A save point that can later be passed to arena_restore.
arena_save_point arena_save(arena* a) {
// @>
  return (arena_save_point){
    .block = a->current,
    .used = a->current ? a->current->used : 0,
  };
}

// <@
// @name arena_restore
// @kind function
// @desc Restores an arena to a previously saved allocation position.
// All allocations made after the save point are discarded.
// @param a The arena to restore.
// @param save The save point to restore to.
void arena_restore(arena* a, arena_save_point save) {
// @>
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

// <@
// @name scratch
// @kind type
// @desc A temporary allocation context backed by an arena.
// Memory allocated through its allocator is released when scratch_end is called.
// @kind struct
// @>
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

// <@
// @name scratch_begin_with
// @kind function
// @desc Acquires a scratch allocation context while avoiding a set of conflicting arenas.
// @param conflict An arena that should not be used for the scratch context.
// @param count The number of arenas in the conflicts array.
// @return A scratch context backed by a non-conflicting arena. The returned context must be released with scratch_end.
scratch scratch_begin_with(const scratch *conflict) {
// @>
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

// <@
// @name scratch_begin
// @kind function
// @desc Acquires a scratch allocation context using an available scratch arena.
// @return A scratch context that can be used for temporary allocations. The returned context must be released with scratch_end.
scratch scratch_begin(void) {
// @>
  return scratch_begin_with(nil);
}

// <@
// @name scratch_end
// @kind function
// @desc Releases a scratch allocation context and discards all allocations made through it.
// @param s The scratch context to release.
void scratch_end(scratch s) {
// @>
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

// <@
// @name defer
// @kind macro
// @desc Schedules the given code to be executed when the current scope is exited.
// This is useful for ensuring that resources are properly released, even if an error occurs or a return statement is hit.
// @param code Statement or block of code to execute when the current scope is exited.
// @>
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

// <@
// @name str_view
// @kind type
// @desc A non-owning view into a string.
// The string is not guaranteed to be null-terminated.
// @field count The length of the string view in bytes.
// @field data A pointer to the string data.
typedef struct {
  size_t count;
  const char *data;
} str_view;
// @>

// Forward declarations so that the functions can call each other
str_view str_view_chop_while(str_view *sv, int (*p)(int x));
str_view str_view_chop_by_delim(str_view *sv, char delim);
str_view str_view_chop_left(str_view *sv, size_t n);
str_view str_view_chop_right(str_view *sv, size_t n);
bool str_view_chop_prefix(str_view *sv, str_view prefix);
bool str_view_chop_suffix(str_view *sv, str_view suffix);
str_view str_view_trim(str_view sv);
str_view str_view_trim_left(str_view sv);
str_view str_view_trim_right(str_view sv);
bool str_view_eq(str_view a, str_view b);
bool str_view_ends_with_cstr(str_view sv, const char *cstr);
bool str_view_ends_with(str_view sv, str_view suffix);
bool str_view_starts_with(str_view sv, str_view prefix);
str_view str_view_from_cstr(const char *cstr);
str_view str_view_from_parts(const char *data, size_t count);

// <@
// @name str_view_chop_while
// @kind function
// @desc Removes and returns the longest prefix of the string view for which the predicate returns true.
// The original string view is modified to exclude the returned prefix.
// @param sv The string view to chop from.
// @param p A predicate function that returns non-zero for matching characters.
// @return The chopped prefix.
str_view str_view_chop_while(str_view *sv, int (*p)(int x)) {
// @>
  size_t i = 0;
  while (i < sv->count && p(sv->data[i])) {
    i += 1;
  }

  str_view result = str_view_from_parts(sv->data, i);
  sv->count -= i;
  sv->data  += i;

  return result;
}

// @name str_view_chop_by_delim
// @kind function
// @desc Removes and returns everything before the first occurrence of the delimiter.
// The delimiter itself is also removed from the original string view.
// If the delimiter is not found, the entire string view is returned.
// @param sv The string view to chop from.
// @param delim The delimiter character.
// @return The chopped substring.
str_view str_view_chop_by_delim(str_view *sv, char delim) {
// @>
  size_t i = 0;
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

// <@
// @name str_view_chop_prefix
// @kind function
// @desc Removes a prefix from the string view if it matches.
// @param sv The string view to modify.
// @param prefix The prefix to remove.
// @return true if the prefix matched and was removed, false otherwise.
bool str_view_chop_prefix(str_view *sv, str_view prefix) {
// @>
  if (str_view_starts_with(*sv, prefix)) {
    str_view_chop_left(sv, prefix.count);
    return true;
  }
  return false;
}

// <@
// @name str_view_chop_suffix
// @kind function
// @desc Removes a suffix from the string view if it matches.
// @param sv The string view to modify.
// @param suffix The suffix to remove.
// @return true if the suffix matched and was removed, false otherwise.
bool str_view_chop_suffix(str_view *sv, str_view suffix) {
// @>
  if (str_view_ends_with(*sv, suffix)) {
    str_view_chop_right(sv, suffix.count);
    return true;
  }
  return false;
}

// <@
// @name str_view_chop_left
// @kind function
// @desc Removes and returns up to n bytes from the start of the string view.
// @param sv The string view to chop from.
// @param n The maximum number of bytes to remove.
// @return The removed prefix.
str_view str_view_chop_left(str_view *sv, size_t n) {
// @>
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data, n);

  sv->data  += n;
  sv->count -= n;

  return result;
}

// <@
// @name str_view_chop_right
// @kind function
// @desc Removes and returns up to n bytes from the end of the string view.
// @param sv The string view to chop from.
// @param n The maximum number of bytes to remove.
// @return The removed suffix.
str_view str_view_chop_right(str_view *sv, size_t n) {
// @>
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data + sv->count - n, n);

  sv->count -= n;

  return result;
}

// <@
// @name str_view_from_parts
// @kind function
// @desc Creates a str_view from a pointer and a length.
// @param data The string data pointer.
// @param count The number of bytes in the string view.
// @return A new str_view.
str_view str_view_from_parts(const char *data, size_t count) {
// @>
  str_view sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

// <@
// @name str_view_trim_left
// @kind function
// @desc Returns a copy of the string view with leading whitespace removed.
// @param sv The string view to trim.
// @return The trimmed string view.
str_view str_view_trim_left(str_view sv) {
// @>
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data + i, sv.count - i);
}

// <@
// @name str_view_trim_right
// @kind function
// @desc Returns a copy of the string view with trailing whitespace removed.
// @param sv The string view to trim.
// @return The trimmed string view.
str_view str_view_trim_right(str_view sv) {
// @>
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data, sv.count - i);
}

// <@
// @name str_view_trim
// @kind function
// @desc Returns a copy of the string view with leading and trailing whitespace removed.
// @param sv The string view to trim.
// @return The trimmed string view.
str_view str_view_trim(str_view sv) {
// @>
  return str_view_trim_right(str_view_trim_left(sv));
}

// <@
// @name str_view_from_cstr
// @kind function
// @desc Creates a str_view from a null-terminated C string.
// @param cstr The null-terminated string.
// @return A str_view referencing the string.
str_view str_view_from_cstr(const char *cstr) {
// @>
  return str_view_from_parts(cstr, strlen(cstr));
}


// <@
// @name str_view_eq
// @kind function
// @desc Compares two string views for equality.
// @param a The first string view.
// @param b The second string view.
// @return true if both string views contain the same bytes, false otherwise.
bool str_view_eq(str_view a, str_view b) {
// @>
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
}

// <@
// @name str_view_ends_with_cstr
// @kind function
// @desc Checks whether a string view ends with a null-terminated C string.
// @param sv The string view to check.
// @param cstr The suffix string.
// @return true if sv ends with cstr, false otherwise.
bool str_view_ends_with_cstr(str_view sv, const char *cstr) {
// @>
  return str_view_ends_with(sv, str_view_from_cstr(cstr));
}

// <@
// @name str_view_ends_with
// @kind function
// @desc Checks whether a string view ends with another string view.
// @param sv The string view to check.
// @param suffix The suffix to test.
// @return true if sv ends with suffix, false otherwise.
bool str_view_ends_with(str_view sv, str_view suffix) {
// @>
  if (sv.count >= suffix.count) {
    str_view sv_tail = {
      .count = suffix.count,
      .data = sv.data + sv.count - suffix.count,
    };
    return str_view_eq(sv_tail, suffix);
  }
  return false;
}

// <@
// @name str_view_starts_with
// @kind function
// @desc Checks whether a string view starts with another string view.
// @param sv The string view to check.
// @param prefix The prefix to test.
// @return true if sv starts with prefix, false otherwise.
bool str_view_starts_with(str_view sv, str_view expected_prefix) {
// @>
  if (expected_prefix.count <= sv.count) {
    str_view actual_prefix = str_view_from_parts(sv.data, expected_prefix.count);
    return str_view_eq(expected_prefix, actual_prefix);
  }

  return false;
}

// <@
// @name str_builder
// @kind type
// @desc A dynamically growing string builder for constructing strings efficiently.
// The buffer is always null-terminated.
// @field data Pointer to the character buffer.
// @field count The number of bytes currently used, excluding the null terminator.
// @field capacity The total capacity of the buffer in bytes.
typedef struct {
  char* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTILS
  allocator alloc;
#endif
} str_builder;
// @>

// <@
// @name sfmt
// @kind macro
// @desc printf format string helper for printing str_view or str_builder contents.
// @example printf(svpfmt, svpfarg(sv));
// @see_also sfmtarg
// @>
#define sfmt "%.*s"

// <@
// @name sfmtarg
// @kind macro
// @desc Expands a str_view or str_builder into printf arguments compatible with sfmt.
// @param sv The str_view or str_builder to expand.
// @see_also sfmt
// @>
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

// <@
// @name str_builder_reserve
// @kind function
// @desc Ensures that the string builder has enough capacity for additional bytes.
// @param sb The string builder.
// @param additional The number of additional bytes required.
// @return true on success, false on allocation failure.
bool str_builder_reserve(str_builder* sb, usz additional) {
// @>
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

// <@
// @name str_builder_append_bytes
// @kind function
// @desc Appends raw bytes to the string builder.
// @param sb The string builder.
// @param data Pointer to the bytes to append.
// @param size Number of bytes to append.
// @return true on success, false on allocation failure.
bool str_builder_append_bytes(str_builder* sb, const void* data, usz size) {
// @>
  if (!str_builder_reserve(sb, size)) {
    return false;
  }

  memcpy(sb->data + sb->count, data, size);

  sb->count += size;

  sb->data[sb->count] = '\0';

  return true;
}

// <@
// @name str_builder_append_cstr
// @kind function
// @desc Appends a null-terminated C string to the string builder.
// @param sb The string builder.
// @param cstr The string to append.
// @return true on success, false on allocation failure.
bool str_builder_append_cstr(str_builder* sb, const char* cstr) {
// @>
  return str_builder_append_bytes(sb, cstr, strlen(cstr));
}

// <@
// @name str_builder_append_sb
// @kind function
// @desc Appends the contents of another string builder.
// @param sb The destination string builder.
// @param other The source string builder.
// @return true on success, false on allocation failure.
bool str_builder_append_sb(str_builder* sb, const str_builder* other) {
// @>
  return str_builder_append_bytes(sb, other->data, other->count);
}

bool _str_builder_append_sb_value(str_builder* sb, str_builder other) {
  return str_builder_append_sb(sb, &other);
}

// <@
// @name str_builder_append_sv
// @kind function
// @desc Appends a string view to the string builder.
// @param sb The destination string builder.
// @param sv The string view to append.
// @return true on success, false on allocation failure.
bool str_builder_append_sv(str_builder* sb, str_view sv) {
// @>
  return str_builder_append_bytes(sb, sv.data, sv.count);
}

// <@
// @name str_builder_view
// @kind function
// @desc Returns a string view referencing the contents of the string builder.
// @param sb The string builder.
// @return A str_view referencing the builder contents.
str_view str_builder_view(const str_builder* sb) {
// @>
  return str_view_from_parts(sb->data ? sb->data : "", sb->count);
}

// <@
// @name str_builder_append
// @kind macro
// @desc Generic append macro for appending C-strings, string builders and string views.
// Supported types depend on enabled utilities.
// @param sb The destination string builder.
// @param data The value to append.
// @>
#define str_builder_append(sb, data)           \
  _Generic((data),                             \
    char*: str_builder_append_cstr,            \
    const char*: str_builder_append_cstr,      \
    str_builder: _str_builder_append_sb_value, \
    str_builder*: str_builder_append_sb,       \
    const str_builder*: str_builder_append_sb, \
    str_view: str_builder_append_sv            \
  )(sb, data)

// <@
// @name str_builder_clear
// @kind function
// @desc Clears the contents of the string builder without freeing its memory.
// @param sb The string builder to clear.
void str_builder_clear(str_builder* sb) {
// @>
  sb->count = 0;

  if (sb->data != nil) {
    sb->data[0] = '\0';
  }
}

// <@
// @name str_builder_free
// @kind function
// @desc Frees the memory owned by the string builder and resets it to an empty state.
// @param sb The string builder to free.
void str_builder_free(str_builder* sb) {
// @>
  _str_builder_ensure_allocator(sb);

  _str_builder_free(sb, sb->data);

  sb->data = nil;
  sb->count = 0;
  sb->capacity = 0;
}

#endif // USE_STR_UTILS



#ifdef USE_DA_UTILS

#include <string.h>

#ifdef USE_ALLOC_UTILS
#define _DA_ALLOC_FIELD allocator alloc;
#else
#define _DA_ALLOC_FIELD
#endif

typedef struct {
  void* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTILS
  allocator alloc;
#endif
} _da_base;

// <@
// @name da
// @kind macro
// @desc Declares a dynamic array type for a given element type.
// @param T The element type.
// @example
// typedef da(int) int_array;
// int_array arr = {0}; // Initialize an empty dynamic array of integers.
// da_append(&arr, 42); // Append an integer to the array.
// @>
#define da(T) struct { \
  T* data;             \
  usz count;           \
  usz capacity;        \
  _DA_ALLOC_FIELD      \
}

#ifdef USE_ALLOC_UTILS

void _da_base_ensure_allocator(_da_base* arr) {
  if (arr->alloc.alloc == nil) {
    arr->alloc = make_allocator();
  }
}

#define _da_base_realloc(arr, elem_size, new_capacity) \
  arr->alloc.realloc(arr->alloc.ctx, arr->data, new_capacity * elem_size)

#define _da_base_free(arr, ptr) \
  do { \
    if ((ptr) != nil) { \
      arr->alloc.free(arr->alloc.ctx, (ptr)); \
    } \
  } while (0)

#else

void _da_base_ensure_allocator(_da_base* arr) {
  (void)arr;
}

#define _da_base_realloc(arr, elem_size, new_capacity) \
  realloc(arr->data, new_capacity * elem_size)

#define _da_base_free(arr, ptr) \
  free(ptr)

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

  _da_base_free(arr, arr->data);

  arr->data = nil;
  arr->count = 0;
  arr->capacity = 0;
}

// <@
// @name da_append
// @kind macro
// @desc Appends a value to the dynamic array, resizing if necessary.
// @param arr Pointer to the dynamic array.
// @param value The value to append.
// @return true on success, false on allocation failure.
// @>
#define da_append(arr, value)            \
  ({                                     \
    typeof(*(arr)->data) _tmp = (value); \
    _da_base_append_impl(                \
      (_da_base*)(arr),                  \
      &_tmp,                             \
      sizeof(_tmp)                       \
    );                                   \
  })

// <@
// @name da_at
// @kind macro
// @desc Returns the element at the given index.
// No bounds checking is performed.
// @param arr Pointer to the dynamic array.
// @param index The element index.
// @>
#define da_at(arr, index) ((arr)->data[(index)])

// <@
// @name da_last
// @kind macro
// @desc Returns the last element of the dynamic array.
// The array must not be empty.
// @param arr Pointer to the dynamic array.
// @>
#define da_last(arr) ((arr)->data[(arr)->count - 1])

// <@
// @name da_free
// @kind macro
// @desc Frees the memory owned by the dynamic array and resets it to an empty state.
// @param arr Pointer to the dynamic array.
// @>
#define da_free(arr) _da_base_free((_da_base*)(arr))

#define _DA_FOREACH_1(arr) \
  _DA_FOREACH_2(arr, it)

#define _DA_FOREACH_2(arr, it)                                     \
  for (usz _i = 0; _i < (arr)->count; ++_i)                        \
    for (typeof(*(arr)->data) it = (arr)->data[_i], *_once = &it;  \
         _once != nil;                                             \
         _once = nil)

#define _DA_FOREACH_GET(_1, _2, NAME, ...) NAME

// <@
// @name da_foreach
// @kind macro
// @desc Iterates over all elements in a dynamic array.
// @param arr Pointer to the dynamic array.
// @param it Optional variable name for the current element. Defaults to 'it' if not provided.
// @example
// da_foreach(&arr) {
//   printf("%d\n", it);
// }
// da_foreach(&arr, x) {
//   printf("%d\n", x);
// }
// @>
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

// <@
// @name da_foreach_i
// @kind macro
// @desc Iterates over all elements in the dynamic array, exposing the index
// and a copy of the element.
// @param arr Pointer to the dynamic array.
// @param i Optional index variable name. Defaults to 'idx' if not provided.
// @param it Optional variable name. Defaults to 'it' if not provided.
// @example
// da_foreach_i(&arr) {
//   printf("%zu: %d\n", i, it);
// }
// da_foreach_i(&arr, i, x) {
//   printf("%zu: %d\n", i, x);
// }
// @>
#define da_foreach_i(...) \
  _DA_FOREACH_I_GET(__VA_ARGS__, _DA_FOREACH_I_3, _, _DA_FOREACH_I_1)(__VA_ARGS__)

#endif // USE_DA_UTILS


#ifdef USE_FILE_UTILS

#include <stdio.h>

// <@
// @name read_entire_file
// @kind function
// @desc Reads the entire contents of a file into a string builder.
// Existing contents of the string builder are cleared.
// @param path Path to the file.
// @param sb Destination string builder.
// @return true on success, false on failure.
bool read_entire_file(const char* path, str_builder* sb) {
// @>
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

// <@
// @name write_entire_file_cstr
// @kind function
// @desc Writes a null-terminated C string to a file, replacing its contents.
// @param path Path to the file.
// @param data The string to write.
// @return true on success, false on failure.
bool write_entire_file_cstr(const char* path, const char* data) {
// @>
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz size = strlen(data);

  usz written = fwrite(data, 1, size, f);

  fclose(f);

  return written == size;
}

// <@
// @name write_entire_file_sv
// @kind function
// @desc Writes a string view to a file, replacing its contents.
// @param path Path to the file.
// @param sv The string view to write.
// @return true on success, false on failure.
bool write_entire_file_sv(const char* path, str_view sv) {
// @>
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sv.data, 1, sv.count, f);

  fclose(f);

  return written == sv.count;
}


// <@
// @name write_entire_file_sv_ptr
// @kind function
// @desc Writes a string view pointed to by sv to a file.
// @param path Path to the file.
// @param sv Pointer to the string view to write.
// @return true on success, false on failure.
bool write_entire_file_sv_ptr(const char* path, str_view* sv) {
// @>
  if (sv == nil) {
    return false;
  }

  return write_entire_file_sv(
    path,
    *sv
  );
}

// <@
// @name write_entire_file_sb
// @kind function
// @desc Writes the contents of a string builder to a file.
// @param path Path to the file.
// @param sb The string builder to write.
// @return true on success, false on failure.
bool write_entire_file_sb(const char* path, str_builder sb) {
// @>
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written = fwrite(sb.data, 1, sb.count, f);

  fclose(f);

  return written == sb.count;
}

// <@
// @name write_entire_file_sb_ptr
// @kind function
// @desc Writes the contents of a string builder pointed to by sb to a file.
// @param path Path to the file.
// @param sb Pointer to the string builder to write.
// @return true on success, false on failure.
bool write_entire_file_sb_ptr(const char* path, str_builder* sb) {
// @>
  if (sb == nil) {
    return false;
  }

  return write_entire_file_sb(path, *sb);
}

// <@
// @name write_entire_file
// @kind macro
// @desc Generic file writing macro supporting C strings, string views and string builders.
// Supported types depend on enabled utilities.
// @param path Path to the file.
// @param data The data to write.
// @>
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

#endif // _UTILS_C
