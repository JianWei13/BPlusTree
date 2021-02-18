#include <stdint.h>
#include <stddef.h>

#include "kvdb.h"
#include "bplus_tree.h"

kvdb_t kvdb_open(char *name)
{
	return bplus_tree_init(name, 96);
}

int kvdb_close(kvdb_t db) {
	bplus_tree_destruct(db);
	return 0;
}

int kvdb_put(kvdb_t db, uint64_t k, uint64_t v) {
	return bplus_tree_put(db, k, v);
}

int kvdb_get(kvdb_t db, uint64_t k, uint64_t *v) {
	return bplus_tree_get(db, k, v);
}

int kvdb_get_range(kvdb_t db, uint64_t k1, uint64_t k2, size_t *c) {
	return bplus_tree_get_range(db, k1, k2, c);
}

int kvdb_next(kvdb_t db, uint64_t sk, uint64_t *k, uint64_t *v){
	return bplus_tree_next(db, sk, k, v);
}


int kvdb_del(kvdb_t db, uint64_t k){ return 0; }
