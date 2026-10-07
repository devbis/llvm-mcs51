// RUN: %clang_cc1 -triple mcs51 -fsyntax-only -verify %s

// PDATA and DATA/IDATA pointers are 8 bits wide, so objects in those address
// spaces are limited to 255 bytes; XDATA and CODE pointers are 16 bits wide.
__pdata unsigned char p_ok[255];
__pdata unsigned char p_bad[256]; // expected-error {{is too large for the address space (maximum allowed size of 255 bytes)}}
__data unsigned char d_bad[256]; // expected-error {{is too large for the address space (maximum allowed size of 255 bytes)}}
__idata unsigned char i_bad[256]; // expected-error {{is too large for the address space (maximum allowed size of 255 bytes)}}
__xdata unsigned char x_ok[4096];
