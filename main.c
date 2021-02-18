#include <stdio.h>

#include "kvdb.h"

int main(int argc, char **argv) {
	int ret = 0;

	kvdb_t db = kvdb_open("bplus_kv.db");
	if (db == NULL) {
		fprintf(stderr, "Error: DB create fail!\n");
		return -1;
	}
	
	ret = kvdb_put(db, 10, 10);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 11, 11);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 13, 13);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 9, 9);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 7, 7);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 12, 12);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 8, 8);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 5, 5);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 6, 6);
	printf("DB put ret=%d\n", ret);
	ret = kvdb_put(db, 15, 15);
	printf("DB put ret=%d\n", ret);

	uint64_t key = 0;
	uint64_t value = 0;
	ret = kvdb_get(db, 6, &value);
	printf("DB get ret=%d, value:%u\n", ret, value);

	size_t cnt = 0;
	ret = kvdb_get_range(db, 6, 12, &cnt);
	printf("DB get range ret=%d, cnt:%u\n", ret, cnt);

	value = 0;
	ret = kvdb_next(db, 15, &key, &value);
	printf("DB get next ret=%d, key:%u, value:%u\n", ret, key, value);
	
	kvdb_close(db);
	return 0;
}
