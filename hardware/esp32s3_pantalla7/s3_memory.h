#ifndef MFV3_S3_MEMORY_H
#define MFV3_S3_MEMORY_H

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

// Hook local para simular fallos de memoria en las pruebas del PC.
#ifndef MFV3_REALLOC
#define MFV3_REALLOC realloc
#endif
namespace s3mem {
// Conserva el bloque y su capacidad si no se puede ampliar. Nunca reduce ni reserva de uno en uno.
template <typename T> static bool reserve(T*& data, int& capacity, int needed, int initial = 16) {
  if (needed < 0 || capacity < 0 || initial < 1) return false;
  if (needed <= capacity) return true;
  int next = capacity ? capacity : initial;
  while (next < needed) {
    if (next > INT_MAX / 2) { next = needed; break; }
    next *= 2;
  }
  if ((size_t)next > SIZE_MAX / sizeof(T)) return false;
  T* grown = (T*)MFV3_REALLOC(data, (size_t)next * sizeof(T));
  if (!grown) return false;
  data = grown;
  capacity = next;
  return true;
}
}

// Solo datos persistentes de interfaz; se reservan tras el arranque y nunca para DMA/ISR.
#include <new>
#include <type_traits>
#include <assert.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#endif
namespace s3mem {
static size_t uiBytes = 0;
static unsigned uiArrays = 0;
static void* allocateUi(size_t bytes) {
#ifdef ESP_PLATFORM
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return malloc(bytes);
#endif
}
#ifndef MFV3_UI_ALLOC
#define MFV3_UI_ALLOC(bytes) ::s3mem::allocateUi(bytes)
#endif
template <typename T, size_t N> class ExternalArray {
  static_assert(N > 0 && N <= SIZE_MAX / sizeof(T), "Tamaño de tabla no válido");
  static_assert(std::is_trivially_destructible<T>::value, "Solo tablas de datos sin destructor");
  T* ptr_ = nullptr;
 public:
  ExternalArray() = default;
  ExternalArray(const ExternalArray&) = delete;
  ExternalArray& operator=(const ExternalArray&) = delete;
  ~ExternalArray() { if (ptr_) { free(ptr_); uiBytes -= sizeof(T)*N; --uiArrays; } }
  bool ensure() {
    if (ptr_) return true;
    void* raw = MFV3_UI_ALLOC(sizeof(T)*N);
    if (!raw) return false;
#ifdef ESP_PLATFORM
    if (!esp_ptr_external_ram(raw)) { free(raw); return false; }
#endif
    T* p = static_cast<T*>(raw);
    for (size_t i=0;i<N;i++) new (p+i) T{};
    ptr_ = p;
    uiBytes += sizeof(T)*N; ++uiArrays;
    return true;
  }
  static constexpr size_t capacity() { return N; }
  T* data() { return ptr_; }
  const T* data() const { return ptr_; }
  operator T*() { return ptr_; }
  operator const T*() const { return ptr_; }
  T& operator[](size_t i) { assert(ptr_ && i<N); return ptr_[i]; }
  const T& operator[](size_t i) const { assert(ptr_ && i<N); return ptr_[i]; }
};
}

#endif
