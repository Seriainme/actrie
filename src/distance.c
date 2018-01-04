//
// Created by james on 6/19/17.
//

#include "utf8.h"
#include "distance.h"

#define MAX_WORD_DISTANCE 15
#define MAX_CHAR_DISTANCE (MAX_WORD_DISTANCE*3)


// Distance Matcher
// ========================================================


const matcher_func_l dist_matcher_func = {
    .destruct = (matcher_destruct_func) dist_destruct,
    .alloc_context = (matcher_alloc_context_func) dist_alloc_context
};

const context_func_l dist_context_func = {
    .free_context = (matcher_free_context_func) dist_free_context,
    .reset_context = (matcher_reset_context_func) dist_reset_context,
    .next = (matcher_next_func) dist_next_on_index
};

/*
 * pattern will match:
 * - A\d{0,5}B
 * - A.{0,5}B
 */
static const char *pattern = "^(.*)((\\\\d|\\.)\\{\\d+,(\\d+)\\})(.*)$";

void dist_dict_before_reset(match_dict_t dict,
                            size_t *index_count,
                            size_t *buffer_size) {
  *index_count *= 2;
  *buffer_size *= 2;
}

size_t max_alternation_length(strlen_s keyword, bool nest) {
  size_t max = 0;

  if (keyword.len == 0) return 0;

  size_t so = 0, eo = keyword.len - 1;
  if ((keyword.ptr[so] == left_parentheses
      && keyword.ptr[eo] != right_parentheses)
      || (keyword.ptr[so] != left_parentheses
          && keyword.ptr[eo] == right_parentheses)) {
    max = keyword.len;
  } else {
    // 脱括号
    if (keyword.ptr[so] == left_parentheses
        && keyword.ptr[eo] == right_parentheses) {
      so++;
      eo--;
    }
    size_t i, depth = 0, cur = so;
    for (i = so; i <= eo; i++) {
      if (keyword.ptr[i] == tokens_delimiter) {
        if (depth == 0 && i > cur) {
          size_t len = i - cur;
          if (len > max) max = len;
          cur = i + 1;
        }
      } else if (keyword.ptr[i] == left_parentheses) {
        depth++;
      } else if (keyword.ptr[i] == right_parentheses) {
        depth--;
      }
    }
    if (i > cur) {
      size_t len = i - cur;
      if (len > max) max = len;
    }
  }

  return max;
}

bool dist_dict_add_index(match_dict_t dict, matcher_conf_t config,
                         strlen_s keyword, strlen_s extra, void * tag, mdi_prop_f prop) {
  dist_conf_t dist_config = (dist_conf_t) config->buf;
  matcher_conf_t head_config = dist_config->head;
  matcher_conf_t tail_config = dist_config->tail;
  matcher_conf_t digit_config = dist_config->digit;

  if (dist_config->regex == NULL) {
    // compile pattern
    const char *errorptr;
    int errorcode;
    int erroffset;
    dist_config->regex = pcre_compile2(pattern, PCRE_MULTILINE | PCRE_DOTALL | PCRE_UTF8, &errorcode, &errorptr, &erroffset, NULL);
    if (dist_config->regex == NULL) {
      ALOG_FATAL(errorptr);
    }
  }

  if (keyword.len == 0) return true;

  int ovector[18];
  int rc = pcre_exec(dist_config->regex, NULL, keyword.ptr, (int) keyword.len, 0, 0, ovector, 18);
  if (rc == PCRE_ERROR_NOMATCH) {
    // single
    head_config->add_index(dict, head_config, keyword, extra, tag,
                           mdi_prop_single | prop);
    return true;
  } else if (rc < 0) {
    return false;
  }

  mdi_prop_f base_prop = mdi_prop_tag_id | mdi_prop_bufkey;

  char *dist =
      astrndup(keyword.ptr + PCRE_VEC_SO(ovector, 4),
               (size_t) (PCRE_VEC_EO(ovector, 4) - PCRE_VEC_SO(ovector, 4)));
  size_t distance = (size_t) strtol(dist, NULL, 10);
  afree(dist);
  if (distance > MAX_WORD_DISTANCE)
    distance = MAX_WORD_DISTANCE;
  else if (distance < 0)
    distance = 0;

  if (strncmp(keyword.ptr + PCRE_VEC_SO(ovector, 3), "\\d",
              (size_t) (PCRE_VEC_EO(ovector, 3) - PCRE_VEC_SO(ovector, 3))) == 0) {
    // \d{0,n}
    base_prop |= mdi_prop_dist_digit;
  }

  // store original keyword
  void *key_tag = (void *) dict->idx_count;
  dict_add_index(dict, config, keyword, extra, tag, prop);

  // store processed keyword
  strlen_s head = {
      .ptr = keyword.ptr + PCRE_VEC_SO(ovector, 1),
      .len = (size_t) (PCRE_VEC_EO(ovector, 1) - PCRE_VEC_SO(ovector, 1)),
  };

  strlen_s tail = {
      .ptr = keyword.ptr + PCRE_VEC_SO(ovector, 5),
      .len = (size_t) (PCRE_VEC_EO(ovector, 5) - PCRE_VEC_SO(ovector, 5)),
  };

  if (base_prop & mdi_prop_dist_digit) {
    head_config->add_index(dict, head_config, head,
                           (strlen_s) {.ptr = (char *) distance, .len = 0},
                           key_tag, mdi_prop_head | base_prop);
    digit_config->add_index(dict, digit_config, tail,
                            (strlen_s) {.ptr = (char *) distance, .len = 0},
                            key_tag, mdi_prop_tail | base_prop);
  } else {
    size_t tail_max_len = max_alternation_length(tail, true);

    head_config->add_index(dict, head_config, head,
                           (strlen_s) {.ptr = (char *) tail_max_len, .len = 0},
                           key_tag, mdi_prop_head | base_prop);
    tail_config->add_index(dict, tail_config, tail,
                           (strlen_s) {.ptr = (char *) distance, .len = 0},
                           key_tag, mdi_prop_tail | base_prop);
  }

  return true;
}

void dist_config_clean(matcher_conf_t config) {
  if (config != NULL) {
    dist_conf_t dist_config = (dist_conf_t) config->buf;
    _release(dist_config->head);
    _release(dist_config->tail);
    _release(dist_config->digit);
    free(dist_config->regex);
  }
}

matcher_conf_t
dist_matcher_conf(uint8_t id, matcher_conf_t head, matcher_conf_t tail,
                  matcher_conf_t digit) {
  matcher_conf_t conf =
      matcher_conf(id, matcher_type_dist, dist_dict_add_index, sizeof(dist_conf_s));
  if (conf) {
    conf->clean = dist_config_clean;
    dist_conf_t dist_conf = (dist_conf_t) conf->buf;
    dist_conf->head = head;
    dist_conf->tail = tail;
    dist_conf->digit = digit;
    dist_conf->regex = NULL;
  }
  return conf;
}

bool dist_destruct(dist_matcher_t self) {
  if (self != NULL) {
    matcher_destruct(self->_head_matcher);
    matcher_destruct(self->_tail_matcher);
    matcher_destruct(self->_digit_matcher);
    dict_release(self->_dict);
    afree(self);
    return true;
  }
  return false;
}

matcher_t dist_construct(match_dict_t dict, matcher_conf_t conf) {
  dist_conf_t dist_conf = (dist_conf_t) conf->buf;
  dist_matcher_t matcher = NULL;
  matcher_t head_matcher = NULL;
  matcher_t tail_matcher = NULL;
  matcher_t digit_matcher = NULL;

  do {
    head_matcher = matcher_construct_by_dict(dict, dist_conf->head);
    if (head_matcher == NULL) break;

    tail_matcher = matcher_construct_by_dict(dict, dist_conf->tail);
    if (tail_matcher == NULL) break;

    digit_matcher = matcher_construct_by_dict(dict, dist_conf->digit);
    if (digit_matcher == NULL) break;

    matcher = amalloc(sizeof(struct dist_matcher));
    if (matcher == NULL) break;

    matcher->_dict = dict_retain(dict);

    matcher->_head_matcher = head_matcher;
    matcher->_tail_matcher = tail_matcher;
    matcher->_digit_matcher = digit_matcher;

    matcher->header._type = conf->type;
    matcher->header._func = dist_matcher_func;

    return (matcher_t) matcher;
  } while(0);

  // clean
  matcher_destruct(head_matcher);
  matcher_destruct(tail_matcher);
  matcher_destruct(digit_matcher);

  return NULL;
}

dist_context_t dist_alloc_context(dist_matcher_t matcher) {
  dist_context_t ctx = NULL;

  do {
    ctx = amalloc(sizeof(struct dist_context));
    if (ctx == NULL) break;

    ctx->_matcher = matcher;
    ctx->_head_ctx = ctx->_tail_ctx = ctx->_digit_ctx = NULL;
    ctx->_utf8_pos = NULL;
    ctx->_mdiqn_pool = NULL;
    ctx->_tail_map = NULL;

    ctx->_head_ctx = matcher_alloc_context(matcher->_head_matcher);
    if (ctx->_head_ctx == NULL) break;
    ctx->_tail_ctx = matcher_alloc_context(matcher->_tail_matcher);
    if (ctx->_tail_ctx == NULL) break;
    ctx->_digit_ctx = matcher_alloc_context(matcher->_digit_matcher);
    if (ctx->_digit_ctx == NULL) break;

    ctx->_mdiqn_pool = dynapool_construct(sizeof(mdim_node_s));
    if (ctx->_mdiqn_pool == NULL) break;
    ctx->_tail_map = mdimap_construct(false);
    if (ctx->_tail_map == NULL) break;

    ctx->hdr._type = matcher->header._type;
    ctx->hdr._func = dist_context_func;

    return ctx;
  } while (0);

  dist_free_context(ctx);

  return NULL;
}

bool dist_free_context(dist_context_t ctx) {
  if (ctx != NULL) {
    afree(ctx->_utf8_pos);
    matcher_free_context(ctx->_tail_ctx);
    matcher_free_context(ctx->_head_ctx);
    matcher_free_context(ctx->_digit_ctx);
    mdimap_destruct(ctx->_tail_map);
    dynapool_destruct(ctx->_mdiqn_pool);
    afree(ctx);
  }
  return true;
}

bool dist_reset_context(dist_context_t context, char content[], size_t len) {
  if (context == NULL) return false;

  context->hdr.content = (strlen_s) {.ptr = content, .len = len};

  context->hdr.out_index = NULL;
  context->hdr.out_pos = (strpos_s) {.so = 0, .eo = 0};

  if (context->_utf8_pos != NULL) {
    afree(context->_utf8_pos);
    context->_utf8_pos = NULL;
  }
  context->_utf8_pos = amalloc((len + 1) * sizeof(size_t));
  if (context->_utf8_pos == NULL) {
    fprintf(stderr, "error when malloc.\n");
  }
  utf8_word_position((const char *) content, len, context->_utf8_pos);

  matcher_reset_context(context->_head_ctx, (char *) content, len);
  matcher_reset_context(context->_tail_ctx, (char *) content, len);

  mdimap_reset(context->_tail_map);
  deque_init(&context->_tail_cache);
  dynapool_reset(context->_mdiqn_pool);
  context->_tail_node = NULL;

  context->_state = dist_match_state_new_round;

  return true;
}

static inline void dist_output(dist_context_t ctx, size_t _eo) {
  // alias
  context_t hctx = ctx->_head_ctx;

  ctx->hdr.out_index = hctx->out_index->_tag;
  ctx->hdr.out_pos = (strpos_s) {
      .so = hctx->out_pos.so,
      .eo = _eo
  };
}

bool dist_next_on_index(dist_context_t ctx) {
  // alias
  char *content = ctx->hdr.content.ptr;
  context_t hctx = ctx->_head_ctx;
  context_t tctx = ctx->_tail_ctx;
  context_t dctx = ctx->_digit_ctx;

  mdim_node_t tail_node = ctx->_tail_node;

  switch (ctx->_state) {
  case dist_match_state_new_round:
    while (matcher_next(hctx)) {
      // check single
      if (hctx->out_index->prop & mdi_prop_single) {
        ctx->hdr.out_index = hctx->out_index;
        ctx->hdr.out_pos = hctx->out_pos;
        return true; // next round
      }

      // check number
      size_t dist = (size_t) hctx->out_index->extra;
      if (hctx->out_index->prop & mdi_prop_dist_digit) {
        ctx->_state = dist_match_state_check_prefix;
        // skip number
        size_t tail_so = hctx->out_pos.eo;
        if (number_bitmap[(unsigned char) content[tail_so]]) {
          while (dist--) {
            if (!number_bitmap[(unsigned char) content[tail_so]])break;
            tail_so++;
          }
          // check prefix
          ctx->_tail_so = tail_so;
          matcher_reset_context(dctx, &content[tail_so], ctx->hdr.content.len - tail_so);
  case dist_match_state_check_prefix:
          while (matcher_next(dctx)) {
            if (dctx->out_index->_tag == hctx->out_index->_tag) {
              dist_output(ctx, ctx->_tail_so + dctx->out_pos.eo);
              return true;
            }
          }
        }
        ctx->_state = dist_match_state_new_round;
        continue; // next round
      }

      ctx->_state = dist_match_state_check_history;
      // clean history cache
      tail_node = deque_peek_front(&ctx->_tail_cache, mdim_node_s, deque_elem);
      while (tail_node) {
        if (tail_node->pos.eo > hctx->out_pos.eo) break;
        mdimap_delete(ctx->_tail_map, tail_node);
        deque_delete(&ctx->_tail_cache, tail_node, mdim_node_s, deque_elem);
        dynapool_free_node(ctx->_mdiqn_pool, tail_node);
        tail_node = deque_peek_front(&ctx->_tail_cache, mdim_node_s, deque_elem);
      }
      tail_node = mdimap_search(ctx->_tail_map, hctx->out_index->_tag);
  case dist_match_state_check_history:
      while (tail_node) {
        mdi_t matched_index = tail_node->idx;
        long distance = utf8_word_distance(ctx->_utf8_pos, hctx->out_pos.eo, tail_node->pos.so);
        if (distance < 0) {
          tail_node = tail_node->next;
          continue;
        } else if (distance > (size_t) matched_index->extra) {
          // if length of tail longer than max_tail_length, next round.
          size_t wlen = utf8_word_distance(ctx->_utf8_pos, tail_node->pos.so, tail_node->pos.eo);
          if (wlen >= (size_t) hctx->out_index->extra)
            break;
          tail_node = tail_node->next;
          continue;
        }
        ctx->_tail_node = tail_node->next;
        dist_output(ctx, tail_node->pos.eo);
        return true;
      }
      if (tail_node != NULL) {
        ctx->_state = dist_match_state_new_round;
        continue; // next round
      }

      ctx->_state = dist_match_state_check_tail;
  case dist_match_state_check_tail:
      while (matcher_next(tctx)) {
        mdi_t matched_index = tctx->out_index;
        long distance = utf8_word_distance(ctx->_utf8_pos, hctx->out_pos.eo, tctx->out_pos.so);
        if (distance < 0) continue;

        // record history
        tail_node = dynapool_alloc_node(ctx->_mdiqn_pool);
        tail_node->idx = tctx->out_index;
        tail_node->pos = matcher_matched_pos(tctx);
        mdimap_insert(ctx->_tail_map, tail_node);
        deque_push_back(&ctx->_tail_cache, tail_node, mdim_node_s, deque_elem);

        // if diff of end_pos is longer than max_tail_length, next round.
        if (tctx->out_pos.eo - hctx->out_pos.eo > MAX_CHAR_DISTANCE
            + (size_t) hctx->out_index->extra) break;

        if (matched_index->_tag != hctx->out_index->_tag)
          continue;

        if (distance > (size_t) matched_index->extra) {
          // if length of tail longer than max_tail_length, next round.
          size_t wlen = utf8_word_distance(ctx->_utf8_pos, tail_node->pos.so, tail_node->pos.eo);
          if (wlen >= (size_t) hctx->out_index->extra)
            break;
          continue;
        }
        dist_output(ctx, tctx->out_pos.eo);
        return true;
      }
      ctx->_state = dist_match_state_new_round;
    }
  }

  return false;
}
