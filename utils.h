/*
utils.c - Random C utilities to make it more bearable to work with.
Made for my usecase.

Tested with C23 only.

Some of these come from other people licensed in the public domain, some I wrote myself.

Documentation is included at the end of the file.

Licensed in the public domain. Do whatever you want with it.

Styleguide:
- 2 space indentation,
- pointers aligned to the type (int* ptr, not int *ptr),
- use snake_case for functions, function-like macros, variables and types,
- use ALL_CAPS for macro expansions and macro constants,
- use shorthands defined below always.
*/

#ifndef _UTILS_C
#define _UTILS_C

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

#define u8_max  UINT8_MAX
#define u16_max UINT16_MAX
#define u32_max UINT32_MAX
#define u64_max UINT64_MAX

#define i8_min  INT8_MIN
#define i8_max  INT8_MAX
#define i16_min INT16_MIN
#define i16_max INT16_MAX
#define i32_min INT32_MIN
#define i32_max INT32_MAX
#define i64_min INT64_MIN
#define i64_max INT64_MAX

#define nil NULL

#define _int_by_1_5(val) \
  ((val) + (val) / 2)

#ifdef USE_RANDOM_UTIL

#include <sys/random.h>
#include <errno.h>
#include <assert.h>

inline u64 random_u64(void) {
  u64 value = 0;
  usz offset = 0;

  while (offset < sizeof(value)) {
    ssize_t result = getrandom(
      ((u8*)&value) + offset,
      sizeof(value) - offset,
      0
    );

    if (result <= 0) {
      if (errno == EINTR) {
        continue;
      }

      assert(0 && "getrandom failed");
    }

    offset += (usz)result;
  }

  return value;
}

inline i64 random_i64(void) {
  return (i64)random_u64();
}

inline u32 random_u32(void) {
  return (u32)random_u64();
}

inline i32 random_i32(void) {
  return (i32)random_u32();
}

inline u16 random_u16(void) {
  return (u16)random_u64();
}

inline i16 random_i16(void) {
  return (i16)random_u16();
}

inline u8 random_u8(void) {
  return (u8)random_u64();
}

inline i8 random_i8(void) {
  return (i8)random_u8();
}

inline u64 random_u64_range(u64 min, u64 max) {
  if (min > max) {
    u64 tmp = min;
    min = max;
    max = tmp;
  }

  if (min == 0 && max == u64_max) {
    return random_u64();
  }

  u64 range = max - min + 1;

  u64 limit = u64_max - (u64_max % range);

  u64 value;

  do {
    value = random_u64();
  } while (value >= limit);

  return min + (value % range);
}

inline i64 random_i64_range(i64 min, i64 max) {
  if (min > max) {
    i64 tmp = min;
    min = max;
    max = tmp;
  }

  u64 range = (u64)max - (u64)min + 1;

  u64 limit = u64_max - (u64_max % range);

  u64 value;

  do {
    value = random_u64();
  } while (value >= limit);

  return min + (i64)(value % range);
}

inline u32 random_u32_range(u32 min, u32 max) {
  return (u32)random_u64_range(min, max);
}

inline i32 random_i32_range(i32 min, i32 max) {
  return (i32)random_i64_range(min, max);
}

inline u16 random_u16_range(u16 min, u16 max) {
  return (u16)random_u64_range(min, max);
}

inline i16 random_i16_range(i16 min, i16 max) {
  return (i16)random_i64_range(min, max);
}

inline u8 random_u8_range(u8 min, u8 max) {
  return (u8)random_u64_range(min, max);
}

inline i8 random_i8_range(i8 min, i8 max) {
  return (i8)random_i64_range(min, max);
}

inline f64 random_f64(void) {
  return (f64)random_u64() / ((f64)u64_max + 1.0);
}

inline f32 random_f32(void) {
  return (f32)random_u32() / ((f32)u32_max + 1.0f);
}

inline f64 random_f64_range(f64 min, f64 max) {
  return min + (max - min) * random_f64();
}

inline f32 random_f32_range(f32 min, f32 max) {
  return min + (max - min) * random_f32();
}

#endif // USE_RANDOM_UTIL



#ifdef USE_ALLOC_UTIL

#include <string.h>

typedef struct allocator allocator;

struct allocator {
  void* ctx;

  void* (*alloc)(
    void* ctx,
    usz size
  );

  void* (*realloc)(
    void* ctx,
    void* ptr,
    usz old_size,
    usz new_size
  );

  void (*free)(
    void* ctx,
    void* ptr
  );
};

void* _libc_alloc(
  void* ctx,
  usz size
) {
  (void)ctx;
  return malloc(size);
}

void* _libc_realloc(
  void* ctx,
  void* ptr,
  usz old_size,
  usz new_size
) {
  (void)ctx;
  (void)old_size;

  return realloc(
    ptr,
    new_size
  );
}

void _libc_free(
  void* ctx,
  void* ptr
) {
  (void)ctx;
  free(ptr);
}

allocator libc_allocator(void) {
  return (allocator) {
    .ctx = nil,
    .alloc = _libc_alloc,
    .realloc = _libc_realloc,
    .free = _libc_free,
  };
}

typedef struct {
  void** allocations;
  usz count;
  usz capacity;
} alloc_tracker;

bool _alloc_tracker_resize(
  alloc_tracker* tracker
) {
  usz new_capacity =
    _int_by_1_5(tracker->capacity);

  void** new_allocations =
    realloc(
      tracker->allocations,
      sizeof(void*) * new_capacity
    );

  if (new_allocations == nil) {
    return false;
  }

  tracker->allocations = new_allocations;
  tracker->capacity = new_capacity;

  return true;
}

bool _alloc_tracker_track_ptr(
  alloc_tracker* tracker,
  void* ptr
) {
  if (ptr == nil) {
    return false;
  }

  if (tracker->allocations == nil) {
    tracker->capacity = 16;

    tracker->allocations =
      malloc(
        sizeof(void*) *
        tracker->capacity
      );

    if (tracker->allocations == nil) {
      tracker->capacity = 0;
      return false;
    }
  }

  if (tracker->count >= tracker->capacity) {
    if (!_alloc_tracker_resize(tracker)) {
      return false;
    }
  }

  tracker->allocations[
    tracker->count++
  ] = ptr;

  return true;
}

void _alloc_tracker_untrack_ptr(
  alloc_tracker* tracker,
  void* ptr
) {
  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      tracker->allocations[i] =
        tracker->allocations[
          tracker->count - 1
        ];

      tracker->count -= 1;

      return;
    }
  }
}

void* _tracked_alloc(
  void* ctx,
  usz size
) {
  alloc_tracker* tracker = ctx;

  void* ptr = malloc(size);

  if (ptr == nil) {
    return nil;
  }

  if (
    !_alloc_tracker_track_ptr(
      tracker,
      ptr
    )
  ) {
    free(ptr);
    return nil;
  }

  return ptr;
}

void* _tracked_realloc(
  void* ctx,
  void* ptr,
  usz old_size,
  usz new_size
) {
  (void)old_size;

  alloc_tracker* tracker = ctx;

  if (ptr == nil) {
    void* new_ptr =
      malloc(new_size);

    if (new_ptr == nil) {
      return nil;
    }

    if (
      !_alloc_tracker_track_ptr(
        tracker,
        new_ptr
      )
    ) {
      free(new_ptr);
      return nil;
    }

    return new_ptr;
  }

  for (usz i = 0; i < tracker->count; i++) {
    if (
      tracker->allocations[i] ==
      ptr
    ) {
      void* new_ptr =
        realloc(
          ptr,
          new_size
        );

      if (new_ptr == nil) {
        return nil;
      }

      tracker->allocations[i] =
        new_ptr;

      return new_ptr;
    }
  }

  return nil;
}

void _tracked_free(
  void* ctx,
  void* ptr
) {
  alloc_tracker* tracker = ctx;

  _alloc_tracker_untrack_ptr(
    tracker,
    ptr
  );

  free(ptr);
}

allocator tracked_allocator(
  alloc_tracker* tracker
) {
  return (allocator) {
    .ctx = tracker,
    .alloc = _tracked_alloc,
    .realloc = _tracked_realloc,
    .free = _tracked_free,
  };
}

void alloc_tracker_free_all(
  alloc_tracker* tracker
) {
  for (usz i = 0; i < tracker->count; i++) {
    free(
      tracker->allocations[i]
    );
  }

  free(tracker->allocations);

  tracker->allocations = nil;
  tracker->count = 0;
  tracker->capacity = 0;
}

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT 8
#endif

typedef struct arena_chunk {
  u8* memory;
  usz capacity;
  usz offset;
  struct arena_chunk* next;
} _arena_chunk;

typedef struct {
  _arena_chunk* head;
  _arena_chunk* tail;
  _arena_chunk* current;
  usz chunk_size;
} _arena_class;

typedef struct {
  _arena_class primary;
  _arena_class oversized;
} arena;

_Static_assert(
  (ARENA_ALIGNMENT &
  (ARENA_ALIGNMENT - 1)) == 0,
  "ARENA_ALIGNMENT must be a power of two"
);

usz _arena_align(
  usz x
) {
  usz mask =
    ARENA_ALIGNMENT - 1;

  return (x + mask) & ~mask;
}

_arena_chunk* _arena_chunk_create(
  usz capacity
) {
  _arena_chunk* c =
    malloc(sizeof(_arena_chunk));

  if (c == nil) {
    return nil;
  }

  c->memory =
    malloc(capacity);

  if (c->memory == nil) {
    free(c);
    return nil;
  }

  c->capacity = capacity;
  c->offset = 0;
  c->next = nil;

  return c;
}

void _arena_class_init(
  _arena_class* cls,
  usz chunk_size
) {
  cls->head = nil;
  cls->tail = nil;
  cls->current = nil;
  cls->chunk_size = chunk_size;
}

void _arena_class_destroy(
  _arena_class* cls
) {
  _arena_chunk* c =
    cls->head;

  while (c != nil) {
    _arena_chunk* next =
      c->next;

    free(c->memory);
    free(c);

    c = next;
  }

  cls->head = nil;
  cls->tail = nil;
  cls->current = nil;
}

void _arena_class_reset(
  _arena_class* cls
) {
  for (
    _arena_chunk* c = cls->head;
    c != nil;
    c = c->next
  ) {
    c->offset = 0;
  }

  cls->current = cls->head;
}

void* _arena_class_alloc(
  _arena_class* cls,
  usz size
) {
  size = _arena_align(size);

  if (cls->current == nil) {
    cls->current = cls->head;
  }

  while (
    cls->current != nil &&
    cls->current->offset +
    size >
    cls->current->capacity
  ) {
    cls->current =
      cls->current->next;
  }

  if (cls->current == nil) {
    usz alloc_size =
      size > cls->chunk_size
      ? size
      : cls->chunk_size;

    _arena_chunk* chunk =
      _arena_chunk_create(
        alloc_size
      );

    if (chunk == nil) {
      return nil;
    }

    if (cls->head == nil) {
      cls->head = chunk;
      cls->tail = chunk;
    } else {
      cls->tail->next =
        chunk;

      cls->tail = chunk;
    }

    cls->current = chunk;
  }

  void* ptr =
    cls->current->memory +
    cls->current->offset;

  cls->current->offset +=
    size;

  return ptr;
}

bool arena_init_custom(
  arena* a,
  usz chunk_size
) {
  if (a == nil) {
    return false;
  }

  _arena_class_init(
    &a->primary,
    chunk_size
  );

  _arena_class_init(
    &a->oversized,
    chunk_size
  );

  return true;
}

bool arena_init(
  arena* a
) {
  return arena_init_custom(
    a,
    64 * 1024
  );
}

void arena_reset(
  arena* a
) {
  _arena_class_reset(
    &a->primary
  );

  _arena_class_destroy(
    &a->oversized
  );

  _arena_class_init(
    &a->oversized,
    a->primary.chunk_size
  );
}

void arena_destroy(
  arena* a
) {
  _arena_class_destroy(
    &a->primary
  );

  _arena_class_destroy(
    &a->oversized
  );
}

void* aalloc(
  arena* a,
  usz size
) {
  size = _arena_align(size);

  if (
    size <=
    a->primary.chunk_size
  ) {
    return _arena_class_alloc(
      &a->primary,
      size
    );
  }

  return _arena_class_alloc(
    &a->oversized,
    size
  );
}

void* _arena_alloc(
  void* ctx,
  usz size
) {
  return aalloc(
    (arena*)ctx,
    size
  );
}

void* _arena_realloc(
  void* ctx,
  void* ptr,
  usz old_size,
  usz new_size
) {
  arena* a = ctx;

  void* new_ptr =
    aalloc(a, new_size);

  if (new_ptr == nil) {
    return nil;
  }

  if (ptr != nil) {
    memcpy(
      new_ptr,
      ptr,
      old_size < new_size
        ? old_size
        : new_size
    );
  }

  return new_ptr;
}

void _arena_free(
  void* ctx,
  void* ptr
) {
  (void)ctx;
  (void)ptr;
}

allocator arena_allocator(
  arena* a
) {
  return (allocator) {
    .ctx = a,
    .alloc = _arena_alloc,
    .realloc = _arena_realloc,
    .free = _arena_free,
  };
}

#endif // USE_ALLOC_UTIL



#ifdef USE_DEFER_UTIL


#if defined(__clangd__)

// we want clangd lsp to typecheck the code but not error because we use nexted funcs
// obviously, this is not correct, because code would run immediately, but because it's
// just the lsp and not the actual compiler, it's fine
#define defer(code) code

#elif defined(__GNUC__)

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
  _Static_assert(0, "defer is only supported with GCC that has nested functions support enabled")

#endif

#endif // USE_DEFER_UTIL



#ifdef USE_STR_VIEW_UTIL

/*
Taken from tsoding's nob.h
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct {
  size_t count;
  const char *data;
} str_view;

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

#define svpfmt "%.*s"
#define svpfarg(sv) (int)(sv).count, (sv).data

str_view str_view_chop_while(str_view *sv, int (*p)(int x)) {
  size_t i = 0;
  while (i < sv->count && p(sv->data[i])) {
    i += 1;
  }

  str_view result = str_view_from_parts(sv->data, i);
  sv->count -= i;
  sv->data  += i;

  return result;
}

str_view str_view_chop_by_delim(str_view *sv, char delim) {
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

bool str_view_chop_prefix(str_view *sv, str_view prefix) {
  if (str_view_starts_with(*sv, prefix)) {
    str_view_chop_left(sv, prefix.count);
    return true;
  }
  return false;
}

bool str_view_chop_suffix(str_view *sv, str_view suffix) {
  if (str_view_ends_with(*sv, suffix)) {
    str_view_chop_right(sv, suffix.count);
    return true;
  }
  return false;
}

str_view str_view_chop_left(str_view *sv, size_t n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data, n);

  sv->data  += n;
  sv->count -= n;

  return result;
}

str_view str_view_chop_right(str_view *sv, size_t n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data + sv->count - n, n);

  sv->count -= n;

  return result;
}

str_view str_view_from_parts(const char *data, size_t count) {
  str_view sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

str_view str_view_trim_left(str_view sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data + i, sv.count - i);
}

str_view str_view_trim_right(str_view sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data, sv.count - i);
}

str_view str_view_trim(str_view sv) {
  return str_view_trim_right(str_view_trim_left(sv));
}

str_view str_view_from_cstr(const char *cstr) {
  return str_view_from_parts(cstr, strlen(cstr));
}

bool str_view_eq(str_view a, str_view b) {
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
}

bool str_view_ends_with_cstr(str_view sv, const char *cstr) {
  return str_view_ends_with(sv, str_view_from_cstr(cstr));
}

bool str_view_ends_with(str_view sv, str_view suffix) {
  if (sv.count >= suffix.count) {
    str_view sv_tail = {
      .count = suffix.count,
      .data = sv.data + sv.count - suffix.count,
    };
    return str_view_eq(sv_tail, suffix);
  }
  return false;
}

bool str_view_starts_with(str_view sv, str_view expected_prefix) {
  if (expected_prefix.count <= sv.count) {
    str_view actual_prefix = str_view_from_parts(sv.data, expected_prefix.count);
    return str_view_eq(expected_prefix, actual_prefix);
  }

  return false;
}

#endif // USE_STR_VIEW_UTIL


#ifdef USE_DYN_ARR_UTIL

#ifdef USE_ALLOC_UTIL
#define _DYN_ARR_ALLOC_FIELD allocator alloc;
#else
#define _DYN_ARR_ALLOC_FIELD
#endif

typedef struct {
  void* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTIL
  allocator alloc;
#endif
} _dyn_arr_base;

#define dyn_arr(T) struct { \
  T* data;                  \
  usz count;                \
  usz capacity;             \
  _DYN_ARR_ALLOC_FIELD      \
}

#ifdef USE_ALLOC_UTIL

void _dyn_arr_ensure_allocator(
  _dyn_arr_base* arr
) {
  if (arr->alloc.alloc == nil) {
    arr->alloc =
      libc_allocator();
  }
}

#endif

void* _dyn_arr_resize(
  _dyn_arr_base* arr,
  usz elem_size,
  usz new_capacity
) {
#ifdef USE_ALLOC_UTIL

  _dyn_arr_ensure_allocator(
    arr
  );

  return arr->alloc.realloc(
    arr->alloc.ctx,
    arr->data,
    arr->capacity *
      elem_size,
    new_capacity *
      elem_size
  );

#else

  return realloc(
    arr->data,
    new_capacity *
      elem_size
  );

#endif
}

bool _dyn_arr_push_impl(
  _dyn_arr_base* arr,
  void* value,
  usz elem_size
) {
  if (arr->count >= arr->capacity) {
    usz new_capacity =
      arr->capacity > 0
      ? _int_by_1_5(
          arr->capacity
        )
      : 4;

    void* new_data =
      _dyn_arr_resize(
        arr,
        elem_size,
        new_capacity
      );

    if (new_data == nil) {
      return false;
    }

    arr->data = new_data;
    arr->capacity =
      new_capacity;
  }

  memcpy(
    (u8*)arr->data +
    arr->count *
      elem_size,
    value,
    elem_size
  );

  arr->count += 1;

  return true;
}

void _dyn_arr_free(
  _dyn_arr_base* arr
) {
#ifdef USE_ALLOC_UTIL

  _dyn_arr_ensure_allocator(
    arr
  );

  if (arr->data != nil) {
    arr->alloc.free(
      arr->alloc.ctx,
      arr->data
    );
  }

#else

  free(arr->data);

#endif

  arr->data = nil;
  arr->count = 0;
  arr->capacity = 0;
}

#define da_push(arr, value)                    \
  _dyn_arr_push_impl(                          \
    (_dyn_arr_base*)(arr),                     \
    (void*)&((typeof(*(arr)->data)){(value)}), \
    sizeof(*(arr)->data)                       \
  )

#define da_at(arr, index) \
  ((arr)->data[(index)])

#define da_last(arr) \
  ((arr)->data[      \
    (arr)->count - 1 \
  ])

#define da_free(arr)       \
  _dyn_arr_free(           \
    (_dyn_arr_base*)(arr)  \
  )

#endif // USE_DYN_ARR_UTIL



#ifdef USE_STR_BUILDER_UTIL

#include <string.h>

typedef struct {
  char* data;
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTIL
  allocator alloc;
#endif
} str_builder;

#define sbpfmt "%.*s"

#define sbpfarg(sb) \
  (int)(sb).count, \
  (sb).data

#ifdef USE_ALLOC_UTIL

static void _str_builder_ensure_allocator(
  str_builder* sb
) {
  if (sb->alloc.alloc == nil) {
    sb->alloc =
      libc_allocator();
  }
}

#endif

bool str_builder_reserve(
  str_builder* sb,
  usz additional
) {
  usz required =
    sb->count +
    additional +
    1;

  if (
    required <=
    sb->capacity
  ) {
    return true;
  }

  usz new_capacity =
    sb->capacity > 0
    ? sb->capacity
    : 64;

  while (
    new_capacity <
    required
  ) {
    usz next =
      _int_by_1_5(
        new_capacity
      );

    if (
      next <=
      new_capacity
    ) {
      return false;
    }

    new_capacity = next;
  }

#ifdef USE_ALLOC_UTIL

  _str_builder_ensure_allocator(
    sb
  );

  char* new_data =
    sb->alloc.realloc(
      sb->alloc.ctx,
      sb->data,
      sb->capacity,
      new_capacity
    );

#else

  char* new_data =
    realloc(
      sb->data,
      new_capacity
    );

#endif

  if (new_data == nil) {
    return false;
  }

  sb->data = new_data;
  sb->capacity =
    new_capacity;

  return true;
}

bool str_builder_append_bytes(
  str_builder* sb,
  const void* data,
  usz size
) {
  if (
    !str_builder_reserve(
      sb,
      size
    )
  ) {
    return false;
  }

  memcpy(
    sb->data +
      sb->count,
    data,
    size
  );

  sb->count += size;

  sb->data[
    sb->count
  ] = '\0';

  return true;
}

bool str_builder_append_cstr(
  str_builder* sb,
  const char* cstr
) {
  return str_builder_append_bytes(
    sb,
    cstr,
    strlen(cstr)
  );
}

bool str_builder_append_sb(
  str_builder* sb,
  const str_builder* other
) {
  return str_builder_append_bytes(
    sb,
    other->data,
    other->count
  );
}

bool _str_builder_append_sb_value(
  str_builder* sb,
  str_builder other
) {
  return str_builder_append_sb(
    sb,
    &other
  );
}

#ifdef USE_STR_VIEW_UTIL

bool str_builder_append_sv(
  str_builder* sb,
  str_view sv
) {
  return str_builder_append_bytes(
    sb,
    sv.data,
    sv.count
  );
}

str_view str_builder_view(
  const str_builder* sb
) {
  return str_view_from_parts(
    sb->data
      ? sb->data
      : "",
    sb->count
  );
}

#endif // USE_STR_VIEW_UTIL

#ifdef USE_STR_VIEW_UTIL

#define _STR_BUILDER_APPEND_SV_TYPES \
  , str_view: str_builder_append_sv

#else

#define _STR_BUILDER_APPEND_SV_TYPES

#endif

#define str_builder_append(sb, data)       \
  _Generic((data),                         \
    char*:                                 \
      str_builder_append_cstr,             \
    const char*:                           \
      str_builder_append_cstr,             \
    str_builder:                           \
      _str_builder_append_sb_value,        \
    str_builder*:                          \
      str_builder_append_sb,               \
    const str_builder*:                    \
      str_builder_append_sb                \
    _STR_BUILDER_APPEND_SV_TYPES           \
  )(sb, data)

void str_builder_clear(
  str_builder* sb
) {
  sb->count = 0;

  if (sb->data != nil) {
    sb->data[0] = '\0';
  }
}

void str_builder_free(
  str_builder* sb
) {
#ifdef USE_ALLOC_UTIL

  _str_builder_ensure_allocator(
    sb
  );

  if (sb->data != nil) {
    sb->alloc.free(
      sb->alloc.ctx,
      sb->data
    );
  }

#else

  free(sb->data);

#endif

  sb->data = nil;
  sb->count = 0;
  sb->capacity = 0;
}

#endif // USE_STR_BUILDER_UTIL



#ifdef USE_FILE_UTIL

#include <stdio.h>

#ifdef USE_STR_BUILDER_UTIL

bool read_entire_file(
  const char* path,
  str_builder* sb
) {
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

  if (
    !str_builder_reserve(
      sb,
      (usz)size
    )
  ) {
    fclose(f);
    return false;
  }

  usz read =
    fread(
      sb->data,
      1,
      (usz)size,
      f
    );

  fclose(f);

  if (read != (usz)size) {
    return false;
  }

  sb->count = read;
  sb->data[sb->count] = '\0';

  return true;
}

#endif // USE_STR_BUILDER_UTIL

bool write_entire_file_cstr(
  const char* path,
  const char* data
) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz size = strlen(data);

  usz written =
    fwrite(
      data,
      1,
      size,
      f
    );

  fclose(f);

  return written == size;
}

#ifdef USE_STR_VIEW_UTIL

bool write_entire_file_sv(
  const char* path,
  str_view sv
) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written =
    fwrite(
      sv.data,
      1,
      sv.count,
      f
    );

  fclose(f);

  return written == sv.count;
}

bool write_entire_file_sv_ptr(
  const char* path,
  str_view* sv
) {
  if (sv == nil) {
    return false;
  }

  return write_entire_file_sv(
    path,
    *sv
  );
}

#endif // USE_STR_VIEW_UTIL

#ifdef USE_STR_BUILDER_UTIL

bool write_entire_file_sb(
  const char* path,
  str_builder sb
) {
  FILE* f = fopen(path, "wb");

  if (f == nil) {
    return false;
  }

  usz written =
    fwrite(
      sb.data,
      1,
      sb.count,
      f
    );

  fclose(f);

  return written == sb.count;
}

bool write_entire_file_sb_ptr(
  const char* path,
  str_builder* sb
) {
  if (sb == nil) {
    return false;
  }

  return write_entire_file_sb(
    path,
    *sb
  );
}

#endif // USE_STR_BUILDER_UTIL

#ifdef USE_STR_VIEW_UTIL
#define _WRITE_FILE_SV_TYPES \
  , str_view: write_entire_file_sv \
  , str_view*: write_entire_file_sv_ptr \
  , const str_view*: write_entire_file_sv_ptr
#else
#define _WRITE_FILE_SV_TYPES
#endif

#ifdef USE_STR_BUILDER_UTIL
#define _WRITE_FILE_SB_TYPES \
  , str_builder: write_entire_file_sb \
  , str_builder*: write_entire_file_sb_ptr \
  , const str_builder*: write_entire_file_sb_ptr
#else
#define _WRITE_FILE_SB_TYPES
#endif

#define write_entire_file(path, data)   \
  _Generic((data),                      \
    char*: write_entire_file_cstr,      \
    const char*: write_entire_file_cstr \
    _WRITE_FILE_SV_TYPES                 \
    _WRITE_FILE_SB_TYPES                 \
  )(path, data)

#endif // USE_FILE_UTIL

#endif // _UTILS_C
