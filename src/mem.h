#pragma once

#include <stdlib.h>

#define FREE(p) do {  \
  if (PREDICT_LIKELY((p)!=NULL))          \
      free(p);        \
      p = NULL;       \
  } while (0)

#define MALLOC_TYPE(t) ((t)malloc_or_die(sizeof(t)))

void *reallocarray(void *optr, size_t nmemb, size_t size);

void *realloc_array_or_die(void *ptr, size_t nmemb, size_t n);
void *malloc_array_or_die(size_t nmemb, size_t n);
void *malloc_or_die(size_t n);
void *calloc_or_die(size_t nmemb, size_t n);
void *realloc_or_die(void *ptr, size_t n);

