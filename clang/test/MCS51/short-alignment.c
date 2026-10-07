// RUN: %clang_cc1 -triple mcs51 -target-cpu cc2530 -fsyntax-only -verify %s
// expected-no-diagnostics

// The MCS-51 data layout and SDCC both use byte alignment for every scalar.
// Structures shared with SDCC-built code must not acquire padding.

_Static_assert(_Alignof(short) == 1, "short alignment");
_Static_assert(_Alignof(unsigned short) == 1, "unsigned short alignment");
_Static_assert(_Alignof(int) == 1, "int alignment");
_Static_assert(_Alignof(long) == 1, "long alignment");
_Static_assert(_Alignof(void *) == 1, "pointer alignment");

struct byte_then_short {
  unsigned char tag;
  unsigned short value;
};
_Static_assert(sizeof(struct byte_then_short) == 3, "no padding");
_Static_assert(__builtin_offsetof(struct byte_then_short, value) == 1,
               "short member follows byte");

struct byte_then_array {
  unsigned char tag;
  short values[2];
};
_Static_assert(sizeof(struct byte_then_array) == 5, "no array padding");
