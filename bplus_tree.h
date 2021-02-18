#include <stdio.h>
#include <stdint.h>

#ifndef __BPLUS_TREE_H__
#define __BPLUS_TREE_H__

#define DEFAULT_BLOCK_SIZE 4*1024   // 4K

typedef uint64_t _key_t;
typedef uint64_t _value_t;

/*
 * Structure
 *				   |root|
 *					/
 *             |parent|
 *				/
 *  |prev|  |this|  |next|
 *
 * For m orders b+ tree
 *	ceil(m/2) <= k <= m
 * */
typedef struct bplus_node {
	off_t this;
	off_t parent;
	off_t prev;
	off_t next;

	int is_leaf;
	union {
		int entries;		// max_entry
		int children;		// max_order
	};
	off_t reserve;
} bplus_node, *p_bplus_node;

//// branch
//typedef struct bplus_branch {
//	bplus_node node;
//	_key_t *p_key;			// keys[max_order-1]
//	off_t *p_child;			// children_ptr[max_order]
//} bplus_branch, *p_bplus_branch;
//
//// leaf
//typedef struct bplus_leaf {
//	bplus_node node;
//	_key_t *p_key;			// keys[max_entry]
//	_value_t *p_data;		// datas[max_entry]
//} bplus_leaf, *p_bplus_leaf;

// tree
typedef struct bplus_tree {
	// char *f_path;
	FILE *fp;
	off_t f_size;

	size_t block_size;
	int max_order;			// (block_size - sizeof(bplus_node)) / (sizeof(_key_t) + sizeof(off_t))
	int max_entry;			// (block_size - sizeof(bplus_node)) / (sizeof(_key_t) + sizeof(_value_t))
	off_t root;

} bplus_tree, *p_bplus_tree;


p_bplus_tree bplus_tree_init(const char *f_path, size_t block_size);
void bplus_tree_destruct(p_bplus_tree p_tree);

int bplus_tree_put(p_bplus_tree p_tree, _key_t key, _value_t value);
int bplus_tree_delete(p_bplus_tree p_tree, _key_t key);
int bplus_tree_get(p_bplus_tree p_tree, _key_t key, _value_t *p_value_out);
// using bplus_tree_get_range to get the count of data between keys, 
// then using a loop with bplus_tree_next to traverse each data
int bplus_tree_get_range(p_bplus_tree p_tree, _key_t key1, _key_t key2, size_t *p_cnt_out);
int bplus_tree_next(p_bplus_tree p_tree, _key_t skey, _key_t *p_key_out, _value_t *p_value_out);



//p_bplus_node	node_new(p_bplus_tree p_tree, int type);
//p_bplus_node	node_load(p_bplus_tree p_tree, off_t f_offset);
//void			node_destruct(p_bplus_node p_node);
//off_t			node_append(p_bplus_tree p_tree, p_bplus_node p_node);
//int				node_flush(p_bplus_tree p_tree, p_bplus_node p_node);
//
//int				entry_put(p_bplus_tree p_tree, p_bplus_node p_node, _key_t key, _value_t value);
//int				entry_search(p_bplus_tree p_tree, p_bplus_node p_node, _key_t key);
//p_bplus_node	entry_split(p_bplus_tree p_tree, p_bplus_node p_node_l, int insert_index, _key_t key, _value_t value);
//
//int				branch_put(p_bplus_tree p_tree, p_bplus_node p_node_l, p_bplus_node p_node_r, _key_t insert_key, off_t insert_child);
//p_bplus_node	branch_split(p_bplus_tree p_tree, p_bplus_node p_node_l, int insert_index, _key_t key, off_t child);
//int bplus_tree_dump(p_bplus_tree p_tree);    // for debug

#endif	/*__BPLUS_TREE_H__*/
