#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bplus_tree.h"

enum {
	BRANCH = 0,
	LEAF = 1,
};

#define key_offset(p_tree, p_node) ((_key_t *)((char *) (p_node) + sizeof(bplus_node)))
#define data_offset(p_tree, p_node) ((_value_t *)(key_offset(p_tree, p_node) + p_tree->max_entry))
#define child_offset(p_tree, p_node) ((off_t *)(key_offset(p_tree, p_node) + (p_tree->max_order)))

p_bplus_tree bplus_tree_init(const char *f_path, size_t block_size) {
	FILE *fp = NULL;

	// malloc bplus tree
	p_bplus_tree p_tree = calloc(1, sizeof(bplus_tree));
	if(!p_tree) {
		fprintf(stderr, "Malloc bplus tree memory fail!\n");
		return NULL;
	}

	// open db file
	if((fp = fopen(f_path, "r+")) != NULL) {
	} else if((fp = fopen(f_path, "w+")) != NULL) {
	} else {
		fprintf(stderr, "Open db file fail!\n");
		free(p_tree);
		return NULL;
	}
	p_tree->fp = fp;

	// file size
	fseek(fp, 0, SEEK_END);
	p_tree->f_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(p_tree->f_size < sizeof(size_t) + sizeof(off_t)) {
		p_tree->block_size = block_size > 0 ? block_size : DEFAULT_BLOCK_SIZE;
		p_tree->root = 0;
		fwrite(&p_tree->block_size, sizeof(size_t), 1, fp);
		fwrite(&p_tree->root, sizeof(off_t), 1, fp);
		fflush(fp);
		p_tree->f_size = sizeof(size_t) + sizeof(off_t);
	} else {
		fread(&p_tree->block_size, sizeof(size_t), 1, fp);
		fread(&p_tree->root, sizeof(off_t), 1, fp);
	}

	p_tree->max_order = (p_tree->block_size - sizeof(bplus_node)) / (sizeof(_key_t) + sizeof(off_t));
	p_tree->max_entry = (p_tree->block_size - sizeof(bplus_node)) / (sizeof(_key_t) + sizeof(_value_t));
	printf("block size:%u, node size:%u, max order:%d, max entries:%d\n", p_tree->block_size, sizeof(bplus_node), p_tree->max_order, p_tree->max_entry);
	if(p_tree->max_order < 2) {
		fprintf(stderr, "Order should greater than 2!\n");
		free(p_tree);
		return NULL;
	}

	return p_tree;
}

void bplus_tree_destruct(p_bplus_tree p_tree) {
	if(p_tree->fp >= 0) {
		fclose(p_tree->fp);
		p_tree->fp = NULL;
	}
	if(p_tree) free(p_tree);
}

p_bplus_node node_new(p_bplus_tree p_tree, int type) {
	// create node
	p_bplus_node p_node = (p_bplus_node)calloc(1, p_tree->block_size);
	if (!p_node) {
		fprintf(stderr, "Create new bplus tree node fail!\n");
		return NULL;
	}
	p_node->is_leaf = type;

	return p_node;
}

void node_destruct(p_bplus_node p_node) {
	if(!p_node) return;
	free(p_node);
}

p_bplus_node node_load(p_bplus_tree p_tree, off_t f_offset) {
	if(f_offset == 0) return NULL;

	// create node
	p_bplus_node p_node = (p_bplus_node)calloc(1, p_tree->block_size);
	if (!p_node) {
		fprintf(stderr, "Create new bplus tree node fail!\n");
		return NULL;
	}

	fseek(p_tree->fp, f_offset, SEEK_SET);
	size_t len = fread(p_node, p_tree->block_size, 1, p_tree->fp);
	if(len != 1) {
		fprintf(stderr, "Load bplus tree node fail! offset:%d.\n", f_offset);
		free(p_node);
		return NULL;
	}

//	printf("Load node, this:%d, parent:%d, prev:%d, next:%d, type:%d, entries/children:%d.\n", 
//			p_node->this, p_node->parent, p_node->prev, p_node->next, p_node->is_leaf, p_node->entries);
	return p_node;
}

off_t node_append(p_bplus_tree p_tree, p_bplus_node p_node) {
	p_node->this = p_tree->f_size;
	p_tree->f_size += p_tree->block_size;
	return p_node->this;
}

int	node_flush(p_bplus_tree p_tree, p_bplus_node p_node) {
	fseek(p_tree->fp, p_node->this, SEEK_SET);
	size_t len = fwrite(p_node, p_tree->block_size, 1, p_tree->fp);
	if(len != 1) {
		fprintf(stderr, "Write node to db fail!\n");
		return -1;
	}
	fflush(p_tree->fp);

//	printf("Node flush, this:%d, parent:%d, prev:%d, next:%d, type:%d, entries/children:%d.\n", 
//			p_node->this, p_node->parent, p_node->prev, p_node->next, p_node->is_leaf, p_node->entries);
	node_destruct(p_node);
	return 0;
}

// while return value >= 0, it's the index of the key
// otherwise return value < 0 , means the key is between 2 entries
int entry_search(p_bplus_tree p_tree, p_bplus_node p_node, _key_t key) {
	_key_t *p_key = key_offset(p_tree, p_node);
	int low = -1, mid = 0;
	int high = (p_node->is_leaf == LEAF) ? p_node->entries : p_node->children - 1;

	while(low + 1 < high) {
		mid = low + (high - low) / 2;
		if (key > *(p_key+mid)) {
			low = mid;
		} else {
			high = mid;
		}
	}

	if(*(p_key+high) == key) {
		return high;
	}

	return -high-1;
}

p_bplus_node branch_split(p_bplus_tree p_tree, p_bplus_node p_node_l, int insert_index, _key_t key, off_t child) {
	int i;
	p_bplus_node p_child = NULL;

	p_bplus_node p_node_r = node_new(p_tree, p_node_l->is_leaf);
	if(!p_node_r) return NULL;

	p_node_r->parent = p_node_l->parent;
	node_append(p_tree, p_node_r);

	_key_t *p_key_l = key_offset(p_tree, p_node_l);
	_key_t *p_key_r = key_offset(p_tree, p_node_r);
	off_t *p_child_l = child_offset(p_tree, p_node_l);
	off_t *p_child_r = child_offset(p_tree, p_node_r);

	int mid = p_node_l->children / 2;
	_key_t upgrade_key = 0;

//	 0 1		0 1 2		0 1 2 3
//	0 1 2	   0 1 2 3	   0 1 2 3 4
//	  1			  2			   2

	if(insert_index < mid) {
		upgrade_key = *(p_key_l + (mid - 1));
		// key
		memmove(p_key_r, p_key_l + mid, sizeof(_key_t) * (p_node_l->children - mid));
		memset(p_key_l + (mid - 1), 0x00, sizeof(_key_t) * (p_node_l->children - (mid - 1)));
		// child
		memmove(p_child_r, p_child_l + mid, sizeof(off_t) * (p_node_l->children - mid));
		memset(p_child_l + mid, 0x00, sizeof(off_t) * (p_node_l->children - mid));
		
		p_node_r->children = p_node_l->children - mid;
		p_node_l->children = mid;

		// insert child to left
		memmove(p_key_l+insert_index+1, p_key_l+insert_index, (p_node_l->children-1-insert_index)*sizeof(_key_t));
		*(p_key_l+insert_index) = key;
		memmove(p_child_l+(insert_index+1)+1, p_child_l+(insert_index+1), (p_node_l->children-(insert_index+1))*sizeof(off_t));
		*(p_child_l+(insert_index+1)) = child;
		p_node_l->children++;

	} else if (insert_index == mid) {
		upgrade_key = key;
		// key
		memmove(p_key_r, p_key_l + mid, sizeof(_key_t) * (p_node_l->children - mid));
		memset(p_key_l + mid, 0x00, sizeof(_key_t) * (p_node_l->children - mid));
		// child
		memmove(p_child_r, p_child_l + mid, sizeof(off_t) * (p_node_l->children - mid));
		memset(p_child_l + mid, 0x00, sizeof(off_t) * (p_node_l->children - mid));
		
		p_node_r->children = p_node_l->children - mid;
		p_node_l->children = mid;

		// insert child to left
		*(p_child_l+insert_index) = child;
		p_node_l->children++;

	} else {
		upgrade_key = *(p_key_l + mid);
		// key
		memmove(p_key_r, p_key_l + (mid + 1), sizeof(_key_t) * (p_node_l->children - (mid + 1)));
		memset(p_key_l + mid, 0x00, sizeof(_key_t) * (p_node_l->children - mid));
		// child
		memmove(p_child_r, p_child_l + (mid + 1), sizeof(off_t) * (p_node_l->children - (mid + 1)));
		memset(p_child_l + (mid + 1), 0x00, sizeof(off_t) * (p_node_l->children - (mid + 1)));
	
		p_node_r->children = p_node_l->children - (mid + 1);
		p_node_l->children = mid + 1;

		// insert child to right
		memmove(p_key_r+(insert_index-(mid+1)+1), p_key_r+(insert_index-(mid+1)), (p_node_r->children-(insert_index-(mid+1)))*sizeof(_key_t));
		*(p_key_r+(insert_index-(mid+1))) = key;
		memmove(p_child_r+(insert_index-mid+1), p_child_r+(insert_index-mid), (p_node_r->children-(insert_index-mid))*sizeof(off_t));
		*(p_child_r+(insert_index-mid)) = child;
		p_node_r->children++;

	}

	// put key to parent branch
	branch_put(p_tree, p_node_l, p_node_r, upgrade_key, p_node_r->this);

	for(i = 0; i < p_node_r->children; i++) {
		p_child = node_load(p_tree, *(p_child_r+i));
		p_child->parent = p_node_r->this;
		node_flush(p_tree, p_child);
	}

	return p_node_r;
}

int branch_put(p_bplus_tree p_tree, p_bplus_node p_node_l, p_bplus_node p_node_r, _key_t insert_key, off_t insert_child) {
	int i = 0;
	p_bplus_node p_branch = NULL;
	
	// no parent, so it's a root
	if(p_node_l->parent == 0) {
		p_branch = node_new(p_tree, BRANCH);
		if(!p_branch) return -1;

		// put key and child
		*(key_offset(p_tree, p_branch)+0) = insert_key;
		*(child_offset(p_tree, p_branch)+0) = p_node_l->this;
		p_branch->children++;
		*(child_offset(p_tree, p_branch)+1) = insert_child;
		p_branch->children++;

		// new root
		p_tree->root = node_append(p_tree, p_branch);
		fseek(p_tree->fp, sizeof(size_t), SEEK_SET);
		fwrite(&p_tree->root, sizeof(off_t), 1, p_tree->fp);

		p_node_l->parent = p_branch->this;
		p_node_r->parent = p_branch->this;
		return node_flush(p_tree, p_branch);
	}

	p_branch = node_load(p_tree, p_node_l->parent);
	if(!p_branch) return -1;

	_key_t *p_key = key_offset(p_tree, p_branch);
	off_t *p_child = child_offset(p_tree, p_branch);
	i = entry_search(p_tree, p_branch, insert_key);
	i = i >= 0 ? i : -i - 1;
	
	if(p_branch->children < p_tree->max_order) {
		// key move
		memmove(p_key+i+1, p_key+i, (p_branch->children-1-i)*sizeof(_key_t));
		*(p_key+i) = insert_key;
		// child move
		memmove(p_child+i+2, p_child+i+1, (p_branch->children-i-1)*sizeof(off_t));
		*(p_child+i+1) = insert_child;

		p_branch->children++;
	} else {
		// split into 2 branch
		p_bplus_node p_branch_l, p_branch_r;
		p_branch_l = p_branch;
		p_branch_r = branch_split(p_tree, p_branch_l, i, insert_key, insert_child);
		node_flush(p_tree, p_branch_r);
	}

	return node_flush(p_tree, p_branch);
}

p_bplus_node entry_split(p_bplus_tree p_tree, p_bplus_node p_node_l, int insert_index, _key_t key, _value_t value) {
	p_bplus_node p_node_r = node_new(p_tree, p_node_l->is_leaf);
	if(!p_node_r) return NULL;

	p_node_r->parent = p_node_l->parent;
	{	// noly for leaf
		p_node_r->prev = p_node_l->this;
		p_node_r->next = p_node_l->next;
		p_node_l->next = node_append(p_tree, p_node_r);

		if(p_node_r->next != 0) {
			p_bplus_node p_node_n = node_load(p_tree, p_node_r->next);
			if(!p_node_n) {
				node_destruct(p_node_r);
				return NULL;	
			}
			p_node_n->prev = p_node_r->this;
			if(node_flush(p_tree, p_node_n) != 0) {
				node_destruct(p_node_n);
				node_destruct(p_node_r);
				return NULL;	
			}
		}
	}

	_key_t *p_key_l = key_offset(p_tree, p_node_l);
	_key_t *p_key_r = key_offset(p_tree, p_node_r);
	_value_t *p_data_l = data_offset(p_tree, p_node_l);
	_value_t *p_data_r = data_offset(p_tree, p_node_r);

	int split_index = (p_node_l->entries + 1) / 2;
	if(insert_index < split_index) {
		memmove(p_key_r, p_key_l + p_node_l->entries / 2, sizeof(_key_t) * (p_node_l->entries - p_node_l->entries / 2));
		memset(p_key_l + p_node_l->entries / 2, 0x00, sizeof(_key_t) * (p_node_l->entries - p_node_l->entries / 2));
		
		memmove(p_data_r, p_data_l + p_node_l->entries / 2, sizeof(_key_t) * (p_node_l->entries - p_node_l->entries / 2));
		memset(p_data_l + p_node_l->entries / 2, 0x00, sizeof(_key_t) * (p_node_l->entries - p_node_l->entries / 2));
		
		p_node_r->entries = p_node_l->entries - p_node_l->entries / 2;
		p_node_l->entries = p_node_l->entries / 2;

		memmove(p_key_l+insert_index+1, p_key_l+insert_index, (p_node_l->entries-insert_index)*sizeof(_key_t));
		*(p_key_l+insert_index) = key;
		memmove(p_data_l+insert_index+1, p_data_l+insert_index, (p_node_l->entries-insert_index)*sizeof(_value_t));
		*(p_data_l+insert_index) = value;
		p_node_l->entries++;
	} else {
		memmove(p_key_r, p_key_l + split_index, sizeof(_key_t) * (p_node_l->entries - split_index));
		memset(p_key_l + split_index, 0x00, sizeof(_key_t) * (p_node_l->entries - split_index));
	
		memmove(p_data_r, p_data_l + split_index, sizeof(_key_t) * (p_node_l->entries - split_index));
		memset(p_data_l + split_index, 0x00, sizeof(_key_t) * (p_node_l->entries - split_index));

		p_node_r->entries = p_node_l->entries - split_index;
		p_node_l->entries = split_index;

		insert_index -= split_index;
		memmove(p_key_r+insert_index+1, p_key_r+insert_index, (p_node_r->entries-insert_index)*sizeof(_key_t));
		*(p_key_r+insert_index) = key;
		memmove(p_data_r+insert_index+1, p_data_r+insert_index, (p_node_r->entries-insert_index)*sizeof(_value_t));
		*(p_data_r+insert_index) = value;
		p_node_r->entries++;
	}

	return p_node_r;
}

int entry_put(p_bplus_tree p_tree, p_bplus_node p_node, _key_t key, _value_t value) {
	if(p_node->is_leaf == LEAF) {				// leaf
		int i = entry_search(p_tree, p_node, key);
		if(i >= 0) {
			// exist
			*(data_offset(p_tree, p_node)+i) = value;
			return node_flush(p_tree, p_node);
		}

		// while i < 0
		i = -i - 1;
		_key_t *p_key = key_offset(p_tree, p_node);
		_value_t *p_data = data_offset(p_tree, p_node);
		// not full
		if(p_node->entries < p_tree->max_entry) {
			memmove(p_key+i+1, p_key+i, (p_node->entries-i)*sizeof(_key_t));
			*(p_key+i) = key;
			memmove(p_data+i+1, p_data+i, (p_node->entries-i)*sizeof(_value_t));
			*(p_data+i) = value;
			p_node->entries++;
			return node_flush(p_tree, p_node);
		} else {			// full
			// split this node into 2 nodes
			p_bplus_node p_node_l, p_node_r;
			p_node_l = p_node;
			if((p_node_r = entry_split(p_tree, p_node_l, i, key, value)) == NULL) {
				node_destruct(p_node_l);
			} else if (branch_put(p_tree, p_node_l, p_node_r, *(key_offset(p_tree, p_node_r)+0), p_node_r->this) != 0) {
				node_destruct(p_node_r);
				node_destruct(p_node_l);
			} else if (node_flush(p_tree, p_node_l) != 0) {
				node_destruct(p_node_r);
				node_destruct(p_node_l);
			} else if (node_flush(p_tree, p_node_r) != 0) {
				node_destruct(p_node_r);
			} else {
				return 0;
			}
			return -1;
		}
	}
	return 0;
}

int bplus_tree_put(p_bplus_tree p_tree, _key_t key, _value_t value) {
	p_bplus_node p_node = node_load(p_tree, p_tree->root);
	if(p_node) {
		int i = 0;
		p_bplus_node p_child = NULL;
		while(p_node->is_leaf == BRANCH) {
			i = entry_search(p_tree, p_node, key);
			i = i >= 0 ? i + 1 : -i - 1;
			p_child = node_load(p_tree, *(child_offset(p_tree, p_node)+i));
			node_destruct(p_node);
			p_node = p_child;
		}
		// put to entry
		return entry_put(p_tree, p_node, key, value);
	}

	// create new root
	p_node = node_new(p_tree, LEAF);
	if(!p_node) return -1;

	*(key_offset(p_tree, p_node)+0) = key;
	*(data_offset(p_tree, p_node)+0) = value;
	p_node->entries++;
	p_tree->root = node_append(p_tree, p_node);
	
	fseek(p_tree->fp, sizeof(size_t), SEEK_SET);
	fwrite(&p_tree->root, sizeof(off_t), 1, p_tree->fp);

	return node_flush(p_tree, p_node);
}

int entry_get(p_bplus_tree p_tree, p_bplus_node p_node, _key_t key, _value_t * p_value_out, int *p_index_out) {
	int i;
	i = entry_search(p_tree, p_node, key);
	// key is not exist
	if(i<0) return -1;

	if(p_value_out) *p_value_out = *(data_offset(p_tree, p_node)+i);
	if(p_index_out) *p_index_out = i;
	return 0;
}

int bplus_tree_get(p_bplus_tree p_tree, _key_t key, _value_t *p_value_out) {
	int i = 0;
	int ret = 0;
	p_bplus_node p_node = node_load(p_tree, p_tree->root);
	// tree isn't built
	if(!p_node) return -1;

	p_bplus_node p_child = NULL;
	while(p_node->is_leaf == BRANCH) {
		i = entry_search(p_tree, p_node, key);
		i = i >= 0 ? i + 1 : -i - 1;
		p_child = node_load(p_tree, *(child_offset(p_tree, p_node)+i));
		node_destruct(p_node);
		p_node = p_child;
	}
	// get to entry
	ret = entry_get(p_tree, p_node, key, p_value_out, NULL);
	node_destruct(p_node);
	return ret;
}

int bplus_tree_get_range(p_bplus_tree p_tree, _key_t key1, _key_t key2, size_t *p_cnt_out) {
	int i = 0;
	int ret = 0;
	int index_l, index_h;
	size_t cnt = 0;
	_key_t key_l, key_h;
	p_bplus_node p_child = NULL;
	p_bplus_node p_node_l = NULL;
	p_bplus_node p_node_h = NULL;
	if(key1 < key2) {
		key_l = key1;
		key_h = key2;
	} else {
		key_l = key2;
		key_h = key1;
	}

	// find key_l
	p_bplus_node p_node = node_load(p_tree, p_tree->root);
	// tree isn't built
	if(!p_node) return -1;

	while(p_node->is_leaf == BRANCH) {
		i = entry_search(p_tree, p_node, key_l);
		i = i >= 0 ? i + 1 : -i - 1;
		p_child = node_load(p_tree, *(child_offset(p_tree, p_node)+i));
		node_destruct(p_node);
		p_node = p_child;
	}
	// get to entry
	ret = entry_get(p_tree, p_node, key_l, NULL, &index_l);
	if(ret != 0) {// key not exist
		node_destruct(p_node);
		return -1;
	}
	if(key_l == key_h) {
		*p_cnt_out = 1;
		node_destruct(p_node);
		return 0;
	}
	p_node_l = p_node;

	// find key_h
	p_node = node_load(p_tree, p_tree->root);
	while(p_node->is_leaf == BRANCH) {
		i = entry_search(p_tree, p_node, key_h);
		i = i >= 0 ? i + 1 : -i - 1;
		p_child = node_load(p_tree, *(child_offset(p_tree, p_node)+i));
		node_destruct(p_node);
		p_node = p_child;
	}
	// get to entry
	ret = entry_get(p_tree, p_node, key_h, NULL, &index_h);
	if(ret != 0) {// key not exist
		node_destruct(p_node_l);
		node_destruct(p_node);
		return -1;
	}
	p_node_h = p_node;

	while(p_node_l->this != p_node_h->this) {
		cnt += p_node_l->entries - index_l;
		index_l = 0;
		p_child = node_load(p_tree, p_node_l->next);
		node_destruct(p_node_l);
		p_node_l = p_child;
	}
	cnt += index_h + 1 - index_l;
	*p_cnt_out = cnt;
	node_destruct(p_node_l);
	node_destruct(p_node_h);
	return 0;
}

int bplus_tree_next(p_bplus_tree p_tree, _key_t skey, _key_t *p_key_out, _value_t *p_value_out) {
	int i = 0;
	int ret = 0;
	int index;
	p_bplus_node p_child = NULL;

	// find skey
	p_bplus_node p_node = node_load(p_tree, p_tree->root);
	// tree isn't built
	if(!p_node) return -1;

	while(p_node->is_leaf == BRANCH) {
		i = entry_search(p_tree, p_node, skey);
		i = i >= 0 ? i + 1 : -i - 1;
		p_child = node_load(p_tree, *(child_offset(p_tree, p_node)+i));
		node_destruct(p_node);
		p_node = p_child;
	}
	// get to entry
	ret = entry_get(p_tree, p_node, skey, NULL, &index);
	if(ret != 0) {// key not exist
		node_destruct(p_node);
		return -1;
	}

	// in the same node
	if(index+1 < p_node->entries) {
		*p_key_out = *(key_offset(p_tree, p_node)+index+1);
		*p_value_out = *(data_offset(p_tree, p_node)+index+1);
		node_destruct(p_node);
		return 0;
	}

	// in the next node
	if(p_node->next == 0) {
		ret = -1;
	} else {
		p_child = node_load(p_tree, p_node->next);
		*p_key_out = *(key_offset(p_tree, p_child)+0);
		*p_value_out = *(data_offset(p_tree, p_child)+0);
		node_destruct(p_child);	
		ret = 0;
	}

	node_destruct(p_node);
	return ret;
}

int bplus_tree_delete(p_bplus_tree p_tree, _key_t key) {
	// TODO
	return 0;
}
