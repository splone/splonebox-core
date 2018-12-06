#include <stdlib.h>

#include "sb-common.h"

void *malloc_or_die(size_t n)
{
  void *p = malloc(n);
  if (!p && 0 < n)
    LOG_MEMERROR();
  return p;
}


void *malloc_array_or_die(size_t nmemb, size_t n)
{
  void *p = reallocarray(NULL, nmemb, n);
  if (!p && 0 < n)
    LOG_MEMERROR();
  return p;
}


void *calloc_or_die(size_t nmemb, size_t n)
{
  void *p = calloc(nmemb, n);
  if (!p && 0 < n)
    LOG_MEMERROR();
  return p;
}


void *realloc_array_or_die(void *ptr, size_t nmemb, size_t n)
{
  void *p = reallocarray(ptr, nmemb, n);
  if (!p && 0 < n)
    LOG_MEMERROR();
  return p;
}


void *realloc_or_die(void *ptr, size_t n)
{
  void *p = realloc(ptr, n);
  if (!p && 0 < n)
    LOG_MEMERROR();
  return p;
}
