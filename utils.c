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

static inline u64 random_u64(void) {
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

static inline i64 random_i64(void) {
  return (i64)random_u64();
}

static inline u32 random_u32(void) {
  return (u32)random_u64();
}

static inline i32 random_i32(void) {
  return (i32)random_u32();
}

static inline u16 random_u16(void) {
  return (u16)random_u64();
}

static inline i16 random_i16(void) {
  return (i16)random_u16();
}

static inline u8 random_u8(void) {
  return (u8)random_u64();
}

static inline i8 random_i8(void) {
  return (i8)random_u8();
}

static inline u64 random_u64_range(u64 min, u64 max) {
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

static inline i64 random_i64_range(i64 min, i64 max) {
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

static inline u32 random_u32_range(u32 min, u32 max) {
  return (u32)random_u64_range(min, max);
}

static inline i32 random_i32_range(i32 min, i32 max) {
  return (i32)random_i64_range(min, max);
}

static inline u16 random_u16_range(u16 min, u16 max) {
  return (u16)random_u64_range(min, max);
}

static inline i16 random_i16_range(i16 min, i16 max) {
  return (i16)random_i64_range(min, max);
}

static inline u8 random_u8_range(u8 min, u8 max) {
  return (u8)random_u64_range(min, max);
}

static inline i8 random_i8_range(i8 min, i8 max) {
  return (i8)random_i64_range(min, max);
}

static inline f64 random_f64(void) {
  return (f64)random_u64() / ((f64)u64_max + 1.0);
}

static inline f32 random_f32(void) {
  return (f32)random_u32() / ((f32)u32_max + 1.0f);
}

static inline f64 random_f64_range(f64 min, f64 max) {
  return min + (max - min) * random_f64();
}

static inline f32 random_f32_range(f32 min, f32 max) {
  return min + (max - min) * random_f32();
}

#endif // USE_RANDOM_UTIL



#ifdef USE_ALLOC_UTIL

typedef struct {
  void** allocations;
  usz count;
  usz capacity;
} alloc_tracker;

static void alloc_tracker_init(alloc_tracker* tracker) {
  const usz initial_capacity = 16;
  tracker->allocations = malloc(sizeof(void*) * initial_capacity);
  tracker->count = 0;
  tracker->capacity = initial_capacity;
}

static bool _alloc_tracker_resize(alloc_tracker* tracker) {
  const usz new_capacity = (usz)(tracker->capacity * 1.5);
  void** new_allocations = realloc(tracker->allocations, sizeof(void*) * new_capacity);

  if (new_allocations) {
    tracker->allocations = new_allocations;
    tracker->capacity = new_capacity;
    return true;
  }

  return false;
}

static void* talloc(alloc_tracker* tracker, usz size) {
  if (tracker->count >= tracker->capacity) {
    if (!_alloc_tracker_resize(tracker)) {
      return nil;
    }
  }

  void* ptr = malloc(size);

  if (ptr) {
    tracker->allocations[tracker->count++] = ptr;
  }

  return ptr;
}

static void tfree(alloc_tracker* tracker, void* ptr) {
  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      free(ptr);
      tracker->allocations[i] = tracker->allocations[tracker->count - 1];
      tracker->count--;
      return;
    }
  }
}

static bool alloc_tracker_add_ptr(alloc_tracker* tracker, void* ptr) {
  if (ptr == nil || tracker == nil) {
    return false;
  }

  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      return false;
    }
  }

  if (
    tracker->count >= tracker->capacity &&
    !_alloc_tracker_resize(tracker)
  ) {
    return false;
  }

  if (tracker->count < tracker->capacity) {
    tracker->allocations[tracker->count] = ptr;
    tracker->count += 1;
  }

  return true;
}

static void* trealloc(alloc_tracker* tracker, void* ptr, usz new_size) {
  usz found_index = 0;
  bool found = false;

  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      found_index = i;
      found = true;
      break;
    }
  }

  if (!found) {
    return nil;
  }

  void* new_ptr = realloc(ptr, new_size);

  if (!new_ptr) {
    return nil;
  }

  tracker->allocations[found_index] = new_ptr;

  return new_ptr;
}

static void alloc_tracker_free_all(alloc_tracker* tracker) {
  for (usz i = 0; i < tracker->count; i++) {
    free(tracker->allocations[i]);
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
  (ARENA_ALIGNMENT & (ARENA_ALIGNMENT - 1)) == 0,
  "ARENA_ALIGNMENT must be a power of two"
);

static usz _arena_align(usz x) {
  usz mask = ARENA_ALIGNMENT - 1;
  return (x + mask) & ~mask;
}

static _arena_chunk* _arena_chunk_create(usz capacity) {
  _arena_chunk* c = (_arena_chunk*)malloc(sizeof(_arena_chunk));

  if (c == nil) {
    return nil;
  }

  c->memory = (u8*)malloc(capacity);

  if (c->memory == nil) {
    free(c);
    return nil;
  }

  c->capacity = capacity;
  c->offset = 0;
  c->next = nil;

  return c;
}

static void _arena_class_init(_arena_class* cls, usz chunk_size) {
  cls->head = nil;
  cls->tail = nil;
  cls->current = nil;
  cls->chunk_size = chunk_size;
}

static void _arena_class_destroy(_arena_class* cls) {
  _arena_chunk* c = cls->head;

  while (c != nil) {
    _arena_chunk* next = c->next;

    free(c->memory);
    free(c);

    c = next;
  }

  cls->head = nil;
  cls->tail = nil;
  cls->current = nil;
}

static void _arena_class_reset(_arena_class* cls) {
  for (_arena_chunk* c = cls->head; c != nil; c = c->next) {
    c->offset = 0;
  }

  cls->current = cls->head;
}

static void* _arena_class_alloc(_arena_class* cls, usz size) {
  size = _arena_align(size);

  if (cls->current == nil) {
    cls->current = cls->head;
  }

  while (
    cls->current != nil &&
    cls->current->offset + size > cls->current->capacity
  ) {
    cls->current = cls->current->next;
  }

  if (cls->current == nil) {
    usz alloc_size =
      (size > cls->chunk_size)
      ? size
      : cls->chunk_size;

    _arena_chunk* chunk = _arena_chunk_create(alloc_size);

    if (chunk == nil) {
      return nil;
    }

    if (cls->head == nil) {
      cls->head = chunk;
      cls->tail = chunk;
    } else {
      cls->tail->next = chunk;
      cls->tail = chunk;
    }

    cls->current = chunk;
  }

  void* ptr =
    cls->current->memory +
    cls->current->offset;

  cls->current->offset += size;

  return ptr;
}

static bool arena_init_custom(arena* a, usz chunk_size) {
  if (a == nil) {
    return false;
  }

  _arena_class_init(&a->primary, chunk_size);
  _arena_class_init(&a->oversized, chunk_size);

  return true;
}

static bool arena_init(arena* a) {
  return arena_init_custom(a, 64 * 1024); // 64 KB
}

static void arena_reset(arena* a) {
  _arena_class_reset(&a->primary);
  _arena_class_destroy(&a->oversized);
  _arena_class_init(&a->oversized, a->primary.chunk_size);
}

static void arena_destroy(arena* a) {
  _arena_class_destroy(&a->primary);
  _arena_class_destroy(&a->oversized);
}

static void* aalloc(arena* a, usz size) {
  size = _arena_align(size);

  if (size <= a->primary.chunk_size) {
    return _arena_class_alloc(&a->primary, size);
  }

  return _arena_class_alloc(&a->oversized, size);
}

#endif // USE_ALLOC_UTIL



#ifdef USE_OPTION_UTIL

typedef struct {
  enum {
    OPTION_NONE,
    OPTION_SOME
  } tag;

  void* some;
} option;

static inline option option_none(void) {
  option opt;
  opt.tag = OPTION_NONE;
  return opt;
}

static inline option option_some(void* value) {
  option opt;
  opt.tag = OPTION_SOME;
  opt.some = value;
  return opt;
}

static inline bool opt_is_none(option opt) {
  return opt.tag == OPTION_NONE;
}

static inline bool opt_is_some(option opt) {
  return opt.tag == OPTION_SOME;
}

static inline option option_from_ptr(void* ptr) {
  if (ptr == nil) {
    return option_none();
  }

  return option_some(ptr);
}

#endif // USE_OPTION_UTIL



#ifdef USE_DEFER_UTIL

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
static str_view str_view_chop_while(str_view *sv, int (*p)(int x));
static str_view str_view_chop_by_delim(str_view *sv, char delim);
static str_view str_view_chop_left(str_view *sv, size_t n);
static str_view str_view_chop_right(str_view *sv, size_t n);
static bool str_view_chop_prefix(str_view *sv, str_view prefix);
static bool str_view_chop_suffix(str_view *sv, str_view suffix);
static str_view str_view_trim(str_view sv);
static str_view str_view_trim_left(str_view sv);
static str_view str_view_trim_right(str_view sv);
static bool str_view_eq(str_view a, str_view b);
static bool str_view_ends_with_cstr(str_view sv, const char *cstr);
static bool str_view_ends_with(str_view sv, str_view suffix);
static bool str_view_starts_with(str_view sv, str_view prefix);
static str_view str_view_from_cstr(const char *cstr);
static str_view str_view_from_parts(const char *data, size_t count);

#define svpfmt "%.*s"
#define svpfarg(sv) (int)(sv).count, (sv).data

static str_view str_view_chop_while(str_view *sv, int (*p)(int x)) {
  size_t i = 0;
  while (i < sv->count && p(sv->data[i])) {
    i += 1;
  }

  str_view result = str_view_from_parts(sv->data, i);
  sv->count -= i;
  sv->data  += i;

  return result;
}

static str_view str_view_chop_by_delim(str_view *sv, char delim) {
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

static bool str_view_chop_prefix(str_view *sv, str_view prefix) {
  if (str_view_starts_with(*sv, prefix)) {
    str_view_chop_left(sv, prefix.count);
    return true;
  }
  return false;
}

static bool str_view_chop_suffix(str_view *sv, str_view suffix) {
  if (str_view_ends_with(*sv, suffix)) {
    str_view_chop_right(sv, suffix.count);
    return true;
  }
  return false;
}

static str_view str_view_chop_left(str_view *sv, size_t n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data, n);

  sv->data  += n;
  sv->count -= n;

  return result;
}

static str_view str_view_chop_right(str_view *sv, size_t n) {
  if (n > sv->count) {
    n = sv->count;
  }

  str_view result = str_view_from_parts(sv->data + sv->count - n, n);

  sv->count -= n;

  return result;
}

static str_view str_view_from_parts(const char *data, size_t count) {
  str_view sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

static str_view str_view_trim_left(str_view sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data + i, sv.count - i);
}

static str_view str_view_trim_right(str_view sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return str_view_from_parts(sv.data, sv.count - i);
}

static str_view str_view_trim(str_view sv) {
  return str_view_trim_right(str_view_trim_left(sv));
}

static str_view str_view_from_cstr(const char *cstr) {
  return str_view_from_parts(cstr, strlen(cstr));
}

static bool str_view_eq(str_view a, str_view b) {
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
}

static bool str_view_ends_with_cstr(str_view sv, const char *cstr) {
  return str_view_ends_with(sv, str_view_from_cstr(cstr));
}

static bool str_view_ends_with(str_view sv, str_view suffix) {
  if (sv.count >= suffix.count) {
    str_view sv_tail = {
      .count = suffix.count,
      .data = sv.data + sv.count - suffix.count,
    };
    return str_view_eq(sv_tail, suffix);
  }
  return false;
}

static bool str_view_starts_with(str_view sv, str_view expected_prefix) {
  if (expected_prefix.count <= sv.count) {
    str_view actual_prefix = str_view_from_parts(sv.data, expected_prefix.count);
    return str_view_eq(expected_prefix, actual_prefix);
  }

  return false;
}

#endif // USE_STR_VIEW_UTIL



#ifdef USE_DYN_ARR_UTIL

/*
Inspired by tsoding.
*/

#include <string.h>
#include <stdalign.h>

typedef struct {
  usz count;
  usz capacity;

#ifdef USE_ALLOC_UTIL
  alloc_tracker* tracker;
#endif

} _dyn_arr_header;

#define DYN_ARR_INIT_CAPACITY 8

#define _DYN_ARR_HEADER_SIZE                              \
  ((sizeof(_dyn_arr_header) + alignof(max_align_t) - 1) & \
   ~(alignof(max_align_t) - 1))

#define dyn_arr_hdr(arr) \
  ((_dyn_arr_header*)((char*)(arr) - _DYN_ARR_HEADER_SIZE))

#define dyn_arr_len(arr) \
  ((arr) ? dyn_arr_hdr(arr)->count : 0)

#ifdef USE_ALLOC_UTIL

static void _dyn_arr_free_impl(
  void** arr
) {
  if (*arr == nil) {
    return;
  }

  _dyn_arr_header* h =
    dyn_arr_hdr(*arr);

  if (h->tracker != nil) {
    tfree(
      h->tracker,
      h
    );
  } else {
    free(h);
  }

  *arr = nil;
}

#define dyn_arr_free(arr) \
  _dyn_arr_free_impl((void**)&(arr))

#else

#define dyn_arr_free(arr)     \
  do {                        \
    if (arr) {                \
      free(dyn_arr_hdr(arr)); \
      (arr) = nil;            \
    }                         \
  } while (0)

#endif // USE_ALLOC_UTIL

static bool _dyn_arr_push_impl(
  void** arr,
  usz elem_size,
  const void* value

#ifdef USE_ALLOC_UTIL
  , alloc_tracker* tracker
#endif

) {
  if (*arr == nil) {
    usz cap = DYN_ARR_INIT_CAPACITY;

    if (
      cap == 0 ||
      cap > SIZE_MAX / elem_size
    ) {
      return false;
    }

    usz bytes =
      _DYN_ARR_HEADER_SIZE +
      elem_size * cap;

    _dyn_arr_header* h;

#ifdef USE_ALLOC_UTIL

    if (tracker != nil) {
      h = talloc(
        tracker,
        bytes
      );
    } else {
      h = malloc(bytes);
    }

#else

    h = malloc(bytes);

#endif

    if (h == nil) {
      return false;
    }

    h->count = 0;
    h->capacity = cap;

#ifdef USE_ALLOC_UTIL
    h->tracker = tracker;
#endif

    *arr =
      (char*)h +
      _DYN_ARR_HEADER_SIZE;
  }

  _dyn_arr_header* h =
    dyn_arr_hdr(*arr);

  if (h->count >= h->capacity) {
    usz newcap =
      _int_by_1_5(h->capacity);

    if (
      newcap <= h->capacity ||
      newcap > SIZE_MAX / elem_size
    ) {
      return false;
    }

    usz bytes =
      _DYN_ARR_HEADER_SIZE +
      elem_size * newcap;

    _dyn_arr_header* newh;

#ifdef USE_ALLOC_UTIL

    if (h->tracker != nil) {
      newh = trealloc(
        h->tracker,
        h,
        bytes
      );
    } else {
      newh = realloc(
        h,
        bytes
      );
    }

#else

    newh = realloc(
      h,
      bytes
    );

#endif

    if (newh == nil) {
      return false;
    }

    newh->capacity = newcap;

    *arr =
      (char*)newh +
      _DYN_ARR_HEADER_SIZE;

    h = newh;
  }

  memcpy(
    (char*)(*arr) +
    elem_size * h->count,
    value,
    elem_size
  );

  h->count++;

  return true;
}

#ifdef USE_ALLOC_UTIL

#define dyn_arr_push(arr, value) \
  _dyn_arr_push_impl(            \
    (void**)&(arr),              \
    sizeof(*(arr)),              \
    &(typeof(*(arr))){(value)},  \
    nil                          \
  )

#define dyn_arr_push_tracked(   \
  arr,                          \
  value,                        \
  tracker                       \
)                               \
  _dyn_arr_push_impl(           \
    (void**)&(arr),             \
    sizeof(*(arr)),             \
    &(typeof(*(arr))){(value)}, \
    (tracker)                   \
  )

#else

#define dyn_arr_push(arr, value) \
  _dyn_arr_push_impl(            \
    (void**)&(arr),              \
    sizeof(*(arr)),              \
    &(typeof(*(arr))){(value)}   \
  )

#endif // USE_ALLOC_UTIL

static bool _c_arr_to_dyn_impl(
  void** arr,
  const void* c_arr,
  usz size,
  usz count

#ifdef USE_ALLOC_UTIL
  , alloc_tracker* tracker
#endif

) {
  if (
    count > SIZE_MAX / size
  ) {
    return false;
  }

  usz capacity =
    count < DYN_ARR_INIT_CAPACITY
    ? DYN_ARR_INIT_CAPACITY
    : count;

  usz bytes =
    _DYN_ARR_HEADER_SIZE +
    size * capacity;

  _dyn_arr_header* h;

#ifdef USE_ALLOC_UTIL

  if (tracker != nil) {
    h = talloc(
      tracker,
      bytes
    );
  } else {
    h = malloc(bytes);
  }

#else

  h = malloc(bytes);

#endif

  if (h == nil) {
    return false;
  }

  h->count = count;
  h->capacity = capacity;

#ifdef USE_ALLOC_UTIL
  h->tracker = tracker;
#endif

  memcpy(
    (char*)h + _DYN_ARR_HEADER_SIZE,
    c_arr,
    size * count
  );

  *arr =
    (char*)h +
    _DYN_ARR_HEADER_SIZE;

  return true;
}

#ifdef USE_ALLOC_UTIL

#define c_arr_to_dyn(     \
  arr,                    \
  c_arr,                  \
  count                   \
)                         \
  _c_arr_to_dyn_impl(     \
    (void**)&(arr),       \
    (const void*)(c_arr), \
    sizeof(*(arr)),       \
    (usz)(count),         \
    nil                   \
  )

#define c_arr_to_dyn_tracked( \
  arr,                        \
  c_arr,                      \
  count,                      \
  tracker                     \
)                             \
  _c_arr_to_dyn_impl(         \
    (void**)&(arr),           \
    (const void*)(c_arr),     \
    sizeof(*(arr)),           \
    (usz)(count),             \
    (tracker)                 \
  )

#else

#define c_arr_to_dyn(arr, c_arr, count) \
  _c_arr_to_dyn_impl(                   \
    (void**)&(arr),                     \
    (const void*)(c_arr),               \
    sizeof(*(arr)),                     \
    (usz)(count)                        \
  )

#endif // USE_ALLOC_UTIL

#endif // USE_DYN_ARR_UTIL


#ifdef USE_STR_BUILDER_UTIL

#include <string.h>

typedef struct {
  char* data;
  usz count;
  usz capacity;
} str_builder;

#define sbpfmt "%.*s"
#define sbpfarg(sb) (int)(sb).count, (sb).data

static bool str_builder_reserve(
  str_builder* sb,
  usz additional
) {
  usz required =
    sb->count +
    additional +
    1;

  if (required <= sb->capacity) {
    return true;
  }

  usz new_capacity =
    sb->capacity > 0
    ? sb->capacity
    : 64;

  while (new_capacity < required) {
    usz next = (usz)(_int_by_1_5(new_capacity));

    if (next <= new_capacity) {
      return false;
    }

    new_capacity = next;
  }

  char* new_data =
    realloc(sb->data, new_capacity);

  if (new_data == nil) {
    return false;
  }

  sb->data = new_data;
  sb->capacity = new_capacity;

  return true;
}

static bool str_builder_append_n(
  str_builder* sb,
  const char* data,
  usz size
) {
  if (!str_builder_reserve(sb, size)) {
    return false;
  }

  memcpy(
    sb->data + sb->count,
    data,
    size
  );

  sb->count += size;
  sb->data[sb->count] = '\0';

  return true;
}

static bool str_builder_append_cstr(
  str_builder* sb,
  const char* cstr
) {
  return str_builder_append_n(
    sb,
    cstr,
    strlen(cstr)
  );
}

#ifdef USE_STR_VIEW_UTIL

static bool str_builder_append_sv(
  str_builder* sb,
  str_view sv
) {
  return str_builder_append_n(
    sb,
    sv.data,
    sv.count
  );
}

static str_view str_builder_view(
  const str_builder* sb
) {
  return str_view_from_parts(
    sb->data ? sb->data : "",
    sb->count
  );
}

#endif // USE_STR_VIEW_UTIL

static void str_builder_clear(
  str_builder* sb
) {
  sb->count = 0;

  if (sb->data != nil) {
    sb->data[0] = '\0';
  }
}

static void str_builder_free(
  str_builder* sb
) {
  free(sb->data);

  sb->data = nil;
  sb->count = 0;
  sb->capacity = 0;
}

#endif // USE_STR_BUILDER_UTIL



#ifdef USE_FILE_UTIL

#include <stdio.h>

#ifdef USE_STR_BUILDER_UTIL

static bool read_entire_file(
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

static bool write_entire_file_cstr(
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

static bool write_entire_file_sv(
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

#endif // USE_STR_VIEW_UTIL

#if defined(USE_STR_BUILDER_UTIL) && defined(USE_STR_VIEW_UTIL)

static bool write_entire_file_sb(
  const char* path,
  const str_builder* sb
) {
  return write_entire_file_sv(
    path,
    str_builder_view(sb)
  );
}

#endif // USE_STR_BUILDER_UTIL && USE_STR_VIEW_UTIL

#ifdef USE_STR_VIEW_UTIL
#define _WRITE_FILE_SV_TYPES \
  , str_view: write_entire_file_sv
#else
#define _WRITE_FILE_SV_TYPES
#endif

#if defined(USE_STR_BUILDER_UTIL) && defined(USE_STR_VIEW_UTIL)
#define _WRITE_FILE_SB_TYPES \
  , str_builder: write_entire_file_sb \
  , str_builder*: write_entire_file_sb
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

/*
DOCUMENTATION

===============
CORE SHORTHANDS
===============

Description:

  Convenient shorthands for common types and constants.

Typedefs/macro constants:

  u8, u16, u32, u64 - Unsigned integers of 8, 16, 32 and 64 bits.
  i8, i16, i32, i64 - Signed integers of 8, 16, 32 and 64 bits.

  f32, f64 - 32-bit and 64-bit floating point numbers.

  usz - Unsigned size for platform.
  isz - Signed size for platform (pointer difference).

  u8_max, u16_max, u32_max, u64_max - Maximum values for unsigned integers.
  i8_min, i8_max, i16_min, i16_max, i32_min, i32_max, i64_min, i64_max - Minimum and maximum values for signed integers.

  nil - Shorthand for NULL pointer.

===============
USE_RANDOM_UTIL
===============

Description:

  Cryptographically secure random number generation utilities built on top of getrandom(2).

Functions/macros:

  u64 random_u64(void) - Generate a random u64.
  i64 random_i64(void) - Generate a random i64.
  u32 random_u32(void) - Generate a random u32.
  i32 random_i32(void) - Generate a random i32.
  u16 random_u16(void) - Generate a random u16.
  i16 random_i16(void) - Generate a random i16.
  u8 random_u8(void) - Generate a random u8.
  i8 random_i8(void) - Generate a random i8.

  u64 random_u64_range(u64 min, u64 max) - Generate a random u64 in the range [min, max].
  i64 random_i64_range(i64 min, i64 max) - Generate a random i64 in the range [min, max].
  u32 random_u32_range(u32 min, u32 max) - Generate a random u32 in the range [min, max].
  i32 random_i32_range(i32 min, i32 max) - Generate a random i32 in the range [min, max].
  u16 random_u16_range(u16 min, u16 max) - Generate a random u16 in the range [min, max].
  i16 random_i16_range(i16 min, i16 max) - Generate a random i16 in the range [min, max].
  u8 random_u8_range(u8 min, u8 max) - Generate a random u8 in the range [min, max].
  i8 random_i8_range(i8 min, i8 max) - Generate a random i8 in the range [min, max].

  f64 random_f64(void) - Generate a random f64 in the range [0.0, 1.0).
  f32 random_f32(void) - Generate a random f32 in the range [0.0f, 1.0f).
  f64 random_f64_range(f64 min, f64 max) - Generate a random f64 in the range [min, max).
  f32 random_f32_range(f32 min, f32 max) - Generate a random f32 in the range [min, max).

==============
USE_ALLOC_UTIL
==============

Description:

  Allocation utilities including an allocation tracker and a simple arena allocator.

Structs:

  alloc_tracker {
    void** allocations; // Array of tracked pointers
    usz count;          // Number of tracked allocations
    usz capacity;       // Capacity of the allocations array
  }

  arena {
    // Private fields...
  }

Macro constants:

  ARENA_ALIGNMENT - Alignment used for arena allocations (default 8).

Functions/macros:

  void alloc_tracker_init(alloc_tracker* tracker) - Initialize an allocation tracker.

  void* talloc(alloc_tracker* tracker, usz size)
    Allocate memory using malloc and track the allocation.

  void tfree(alloc_tracker* tracker, void* ptr)
    Free a tracked allocation and remove it from the tracker.

  bool alloc_tracker_add_ptr(alloc_tracker* tracker, void* ptr)
    Add an externally allocated pointer to the tracker.

  void* trealloc(alloc_tracker* tracker, void* ptr, usz new_size)
    Reallocate a tracked allocation.

  void alloc_tracker_free_all(alloc_tracker* tracker)
    Free all tracked allocations and reset the tracker.

  bool arena_init(arena* a)
    Initialize an arena with the default chunk size (64 KB).

  bool arena_init_custom(arena* a, usz chunk_size)
    Initialize an arena with a custom chunk size.

  void arena_reset(arena* a)
    Reset an arena for reuse.
    Keeps primary chunks allocated for future allocations.
    Frees oversized allocations.

  void arena_destroy(arena* a)
    Destroy an arena and free all associated memory.

  void* aalloc(arena* a, usz size)
    Allocate memory from the arena.

===============
USE_OPTION_UTIL
===============

Description:

  Rust-style optional value container for nullable pointers.

Structs:

  option {
    ... tag  - OPTION_NONE or OPTION_SOME
    void* some - Stored pointer value when tag == OPTION_SOME
  }

Functions/macros:

  option option_none(void)
    Create an empty option.

  option option_some(void* value)
    Create an option containing a pointer value.

  bool opt_is_none(option opt)
    Check whether an option is empty.

  bool opt_is_some(option opt)
    Check whether an option contains a value.

  option option_from_ptr(void* ptr)
    Convert a nullable pointer into an option.
    Returns OPTION_NONE when ptr == nil.

==============
USE_DEFER_UTIL
==============

Description:

  GCC-only defer utility inspired by modern languages.
  Executes code automatically when the current scope exits.

Dependencies:

  GCC compiler extensions.

Functions/macros:

  defer(code)
    Execute code automatically at scope exit.

=================
USE_STR_VIEW_UTIL
=================

Description:

  Lightweight non-owning string slice utilities.

Structs:

  str_view {
    size_t count;     // Length of the string view
    const char* data; // Pointer to string data
  }

Helper macros:

  svpfmt
    printf format string for printing str_view.

  svpfarg(str_view sv)
    printf argument helper for str_view.

Functions/macros:

  str_view str_view_chop_while(str_view* sv, int (*p)(int x))
    Remove and return characters from the beginning while predicate returns true.

  str_view str_view_chop_by_delim(str_view* sv, char delim)
    Remove and return characters until delimiter is reached.

  str_view str_view_chop_left(str_view* sv, size_t n)
    Remove and return n characters from the left side.

  str_view str_view_chop_right(str_view* sv, size_t n)
    Remove and return n characters from the right side.

  bool str_view_chop_prefix(str_view* sv, str_view prefix)
    Remove a prefix if present.

  bool str_view_chop_suffix(str_view* sv, str_view suffix)
    Remove a suffix if present.

  str_view str_view_trim(str_view sv)
    Trim whitespace from both sides.

  str_view str_view_trim_left(str_view sv)
    Trim whitespace from the left side.

  str_view str_view_trim_right(str_view sv)
    Trim whitespace from the right side.

  bool str_view_eq(str_view a, str_view b)
    Compare two string views for equality.

  bool str_view_ends_with_cstr(str_view sv, const char* cstr)
    Check whether a string view ends with a C string.

  bool str_view_ends_with(str_view sv, str_view suffix)
    Check whether a string view ends with another string view.

  bool str_view_starts_with(str_view sv, str_view prefix)
    Check whether a string view starts with another string view.

  str_view str_view_from_cstr(const char* cstr)
    Create a string view from a null-terminated C string.

  str_view str_view_from_parts(const char* data, size_t count)
    Create a string view from raw pointer and length.

================
USE_DYN_ARR_UTIL
================

Description:

  Dynamic array utilities implemented using a hidden header stored before the array data.

  When USE_ALLOC_UTIL is enabled, dynamic arrays may optionally use alloc_tracker
  for allocation management.

Macro constants:

  DYN_ARR_INIT_CAPACITY - Initial dynamic array capacity.

Functions/macros:

  usz dyn_arr_len(any* arr)
    Get the number of elements in the dynamic array.

  void dyn_arr_free(any* arr)
    Free the dynamic array and set pointer to nil.

    When the array uses alloc_tracker, the allocation is automatically removed
    from the tracker using tfree().

  bool dyn_arr_push(any* arr, any value)
    Append a value to the dynamic array using standard malloc/realloc.

    Automatically grows the allocation if needed.

  bool c_arr_to_dyn(any* arr, any[] c_arr, usz count)
    Convert a C array to a dynamic array using standard malloc/realloc.

When USE_ALLOC_UTIL is enabled:

  bool dyn_arr_push_tracked(
    any* arr,
    any value,
    alloc_tracker* tracker
  )
    Append a value to the dynamic array using tracked allocations.

    The allocation will automatically be registered in the tracker and cleaned up
    by alloc_tracker_free_all().

  bool c_arr_to_dyn_tracked(
    any* arr,
    any[] c_arr,
    usz count,
    alloc_tracker* tracker
  )
    Convert a C array to a tracked dynamic array.

Example:

  int* my_array = nil;

  dyn_arr_push(my_array, 42);
  dyn_arr_push(my_array, 99);

  for (usz i = 0; i < dyn_arr_len(my_array); i++) {
    printf("%d\n", my_array[i]);
  }

  dyn_arr_free(my_array);

Tracked example:

  alloc_tracker tracker;
  alloc_tracker_init(&tracker);

  int* values = nil;

  dyn_arr_push_tracked(
    values,
    123,
    &tracker
  );

  dyn_arr_push_tracked(
    values,
    456,
    &tracker
  );

  alloc_tracker_free_all(&tracker);

Important:
  
  Do not use dyn_arr_push_tracked on untracked arrays or dyn_arr_push on tracked arrays, as this is undefined behaviour by the library design.

=============
USE_FILE_UTIL
=============

Description:

  Convenience utilities for reading and writing entire files.

Functions/macros:

  bool read_entire_file(const char* path, str_builder* sb)
    Read an entire file into a string builder.

    Dependency:
      USE_STR_BUILDER_UTIL

  bool write_entire_file_cstr(const char* path, const char* data)
    Write a null-terminated C string to a file.

  bool write_entire_file_sv(const char* path, str_view sv)
    Write a string view to a file.

    Dependency:
      USE_STR_VIEW_UTIL

  bool write_entire_file_sb(const char* path, const str_builder* sb)
    Write a string builder contents to a file.

    Dependencies:
      USE_STR_BUILDER_UTIL
      USE_STR_VIEW_UTIL

  bool write_entire_file(
    const char* path,
    const char* | const str_builder* | str_view data
  )
    Generic macro wrapper selecting the correct file writing implementation
    based on the type of data.
*/
