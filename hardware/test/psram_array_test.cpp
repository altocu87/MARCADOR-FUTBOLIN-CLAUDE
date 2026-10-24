#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>
static bool fail = true;
static int calls = 0;
static void* testAlloc(size_t n) { ++calls; return fail ? nullptr : malloc(n); }
#define MFV3_UI_ALLOC(n) testAlloc(n)
#include "../esp32s3_pantalla7/s3_memory.h"
struct Entry { int value = 7; char label[8] = {}; };
int main() {
  {
    s3mem::ExternalArray<Entry, 3> a;
    assert(!a.ensure() && !a.data() && !s3mem::uiBytes);
    fail = false; assert(a.ensure());
    auto* p = a.data(); assert(a[0].value==7 && a[2].value==7);
    a[2].value = 42; int before=calls;
    assert(a.ensure() && a.data()==p && a[2].value==42 && calls==before);
    s3mem::ExternalArray<char[2][40], 4> rows;
    assert(rows.ensure() && rows[3][1][39]==0);
    strcpy(rows[3][1], "última pareja");
    assert(!strcmp(rows[3][1], "última pareja"));
    assert(s3mem::uiBytes==sizeof(Entry)*3+sizeof(char)*2*40*4 && s3mem::uiArrays==2);
  }
  assert(!s3mem::uiBytes && !s3mem::uiArrays);
  puts("Tablas PSRAM: fallo recuperable, inicializadores, matrices, reserva única y duración OK");
}
