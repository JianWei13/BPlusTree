all:
	gcc -shared -fPIC -o libkvdb.so kvdb.c bplus_tree.c
	gcc -L. -lkvdb -o main main.c

clean:
	rm -f main libkvdb.so bplus_kv.db

test:clean all
	./main
	hexdump -v bplus_kv.db
