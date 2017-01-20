#ifndef _MATCH_ACTRIE_H_
#define _MATCH_ACTRIE_H_


#include "common.h"


typedef struct trie_node *trie_node_ptr;
typedef struct trie *trie_ptr;

typedef trie_node_ptr (*next_node_func)(trie_ptr, trie_node_ptr, unsigned char);

typedef struct trie_node { // 十字链表实现字典树
	size_t child;       // 指向子结点
#define trie_child   child
	union {
		/* failed 用于自动机，因为自动机构建时需要 bfs，而构建无队列 bfs 需要线性排序。
		 * 因此，使用 failed 字段时已不需要 brother
		 */
		size_t brother; // 指向兄弟结点
		size_t failed;  // 指向失败时跳跃结点
	} trie_bf;
#define trie_brother  trie_bf.brother
#define trie_failed   trie_bf.failed
	union {
		size_t parent;  // 指向逻辑父结点，用于线性排序
		size_t datidx;  // 指向 dat 中对应结点，
	} trie_pd;
#define trie_parent   trie_pd.parent
#define trie_datidx   trie_pd.datidx
	union {
		const char *keyword;
		size_t placeholder;
	} trie_kp;
#define trie_keyword  trie_kp.keyword
#define trie_p0       trie_kp.placeholder
	union {
		const char *extra;
		size_t placeholder;
	} trie_ep;
#define trie_extra    trie_ep.extra
#define trie_p1       trie_ep.placeholder
	unsigned char len;  // 一个结点只存储 1 byte 数据
	unsigned char key;
} trie_node;

typedef struct trie {
	trie_node_ptr   _nodepool[REGION_SIZE];
	size_t          _autoindex;
	trie_node_ptr   root;
	match_dict_ptr  _dict;
} trie;


trie_ptr trie_construct_by_file(FILE *fp);

trie_ptr trie_construct_by_s(const char *s);

void trie_release(trie_ptr p);

void trie_sort_to_line(trie_ptr self);

void trie_rebuild_parent_relation(trie_ptr self);

void trie_construct_automation(trie_ptr self);

void trie_ac_match(trie_ptr self, unsigned char content[], size_t len);


#endif // _MATCH_ACTRIE_H_
