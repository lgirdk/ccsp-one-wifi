/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2024 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 **************************************************************************/
#ifndef HE_BUS_MEMORY_H
#define HE_BUS_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#define he_bus_malloc(size) he_bus_malloc_at((size), __FILE__, __LINE__, 1)
#define he_bus_calloc(num, size) he_bus_calloc_at((num), (size), __FILE__, __LINE__, 1)
#define he_bus_realloc(ptr, new_size) he_bus_realloc_at((ptr), (new_size), __FILE__, __LINE__, 1)
#define he_bus_try_malloc(size) he_bus_malloc_at((size), __FILE__, __LINE__, 0)
#define he_bus_try_calloc(num, size) he_bus_calloc_at((num), (size), __FILE__, __LINE__, 0)
#define he_bus_try_realloc(ptr, new_size) \
    he_bus_realloc_at((ptr), (new_size), __FILE__, __LINE__, 0)
#define he_bus_free(ptr) he_bus_free_at((ptr), __FILE__, __LINE__, 0)

void *he_bus_malloc_at(size_t size, char const *file, int line, int do_abort);
void *he_bus_calloc_at(size_t num, size_t size, char const *file, int line, int do_abort);
void *he_bus_realloc_at(void *ptr, size_t new_size, char const *file, int line, int do_abort);
void he_bus_free_at(void *ptr, char const *file, int line, int do_abort);

#ifdef __cplusplus
}
#endif
#endif // HE_BUS_MEMORY_H
