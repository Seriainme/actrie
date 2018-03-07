#ifndef _ACTRIE_ACTRIE_H_
#define _ACTRIE_ACTRIE_H_

#include "dict0.h"
#include "segarray.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct trie_node { /* 十字链表实现字典树 */
  size_t child;       /* 指向子结点 */
#define trie_child   child
  union {
    /* failed 用于自动机，因为自动机构建时需要 bfs，而构建无队列 bfs 需要线性排序。
     * 因此，使用 failed 字段时已不需要 brother
     */
    size_t brother; /* 指向兄弟结点 */
    size_t failed;  /* 指向失败时跳跃结点 */
  } trie_bf;
#define trie_brother  trie_bf.brother
#define trie_failed   trie_bf.failed
  union {
    size_t parent;  /* 指向逻辑父结点，用于线性排序 */
    size_t datidx;  /* 指向 dat 中对应结点 */
  } trie_pd;
#define trie_parent   trie_pd.parent
#define trie_datidx   trie_pd.datidx
  aobj idxlist;
  unsigned char len;  /* 一个结点只存储 1 byte 数据 */
  unsigned char key;
  char placeholder[6]; /* 8 byte align */
} trie_node_s, *trie_node_t;

typedef struct trie {
  segarray_t nodearray; /* 区位设计不需要大块连续内存，但不能用指针做遍历 */
  trie_node_t root;
  match_dict_t _dict;
} trie_s, *trie_t;

typedef struct trie_conf {
  uint8_t filter;
  bool enable_automation;
} trie_conf_s, *trie_conf_t;

trie_t trie_construct(match_dict_t dict, trie_conf_t conf);
void trie_destruct(trie_t self);

trie_t trie_alloc();
bool trie_add_keyword(trie_t self, const unsigned char keyword[], size_t len, aobj obj);
aobj trie_search(trie_t self, const unsigned char keyword[], size_t len);
void trie_rebuild_parent_relation(trie_t self);

static inline trie_node_t trie_access_node(trie_t self, size_t index) {
#ifdef CHECK
  return segarray_access_s(self->nodearray, index);
#else
  return segarray_access(self->nodearray, index);
#endif // CHECK
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ACTRIE_ACTRIE_H_ */
