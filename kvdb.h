#ifndef __kvdb_h__
#define __kvdb_h__

#include <stdint.h>

typedef void *kvdb_t; 

kvdb_t kvdb_open(char *name);
int kvdb_close(kvdb_t db);
int kvdb_get(kvdb_t db, uint64_t k, uint64_t *v);
int kvdb_get_range(kvdb_t db, uint64_t k1, uint64_t k2, size_t *c);
int kvdb_put(kvdb_t db, uint64_t k, uint64_t v);
int kvdb_del(kvdb_t db, uint64_t k);
int kvdb_next(kvdb_t db, uint64_t sk, uint64_t *k, uint64_t *v);

#endif 

