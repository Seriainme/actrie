/**
 * tokenizer.c
 *
 * @author James Yin <ywhjames@hotmail.com>
 */
#include "tokenizer.h"

#include <alib/concurrent/threadlocal.h>
#include <alib/string/dynabuf.h>

// clang-format off
const bool oct_number_bitmap[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const bool dec_number_bitmap[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const bool hex_number_bitmap[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int hex_char2num[256] = {
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
// clang-format on

static once_flag rept_flag = ONCE_FLAG_INIT;
static tss_t rept_key;

static void rept_init(void) {
  if (tss_create(&rept_key, free) != thrd_success) {
    ALOG_FATAL("rept_init failed!");
  }
}

/**
 * token_oct_num - octal escape sequence
 * @param ch - first char
 */
int token_oct_num(int ch, stream_t stream) {
  int num = ch - '0';
  for (int i = 0; i < 2; i++) {
    ch = stream_getc(stream);
    if (ch == EOF || !oct_number_bitmap[ch]) {
      return TOKEN_ERR;
    }
    num = num * 8 + (ch - '0');
  }
  return num;
}

/**
 * token_hex_num -  hexadecimal escape sequence
 * @param ch - first char
 */
int token_hex_num(int ch, stream_t stream) {
  int num = ch - '0';
  for (int i = 0; i < 2; i++) {
    ch = stream_getc(stream);
    if (ch == EOF || !hex_number_bitmap[ch]) {
      return TOKEN_ERR;
    }
    num = num * 16 + hex_char2num[ch];
  }
  return num;
}

/**
 * token_expect - compare next sequence with excepted bytes
 */
bool token_expect(stream_t stream, const uchar* except, size_t len) {
  if (len == 0) {
    return true;
  }
  int ch = stream_getc(stream);
  if (ch != EOF) {
    if (ch == except[0] && token_expect(stream, except + 1, len - 1)) {
      return true;
    }
    stream_ungetc(stream, ch);
  }
  return false;
}

/**
 * token_expect_char - compare next char with excepted next
 */
bool token_expect_char(stream_t stream, const uchar ch) {
  return token_expect(stream, &ch, 1);
}

/**
 * token_skip_set - skip next sequence when each byte is excepted
 */
bool token_skip_set(stream_t stream, const uchar* except, size_t len) {
  int ch;
  if (except == NULL || len == 0 || except[0] == '\0') {
    // empty set
    return false;
  } else {
    if (len == 1) {
      while ((ch = stream_getc(stream)) != EOF && ch == *except)
        ;
    } else {
      uchar table[256];
      uchar* p = memset(table, 0, 64);
      memset(p + 64, 0, 64);
      memset(p + 128, 0, 64);
      memset(p + 192, 0, 64);

      for (int i = 0; i < len; i++) {
        p[except[i]] = 1;
      }

      while ((ch = stream_getc(stream)) != EOF && p[ch])
        ;
    }

    if (ch != EOF) {
      stream_ungetc(stream, ch);
    }

    return true;
  }
}

/**
 * token_skip_space - skip next space sequence
 */
bool token_skip_space(stream_t stream) {
  return token_skip_set(stream, (uchar*)" ", 1);
}

bool token_consume_integer(stream_t stream, int* integer) {
  int ch = stream_getc(stream);
  bool is_neg = false;
  if (ch == '-') {
    is_neg = true;
    ch = stream_getc(stream);
  }
  if (ch != EOF && dec_number_bitmap[ch]) {
    int num = 0;
    do {
      num = num * 10 + (ch - '0');
    } while ((ch = stream_getc(stream)) != EOF && dec_number_bitmap[ch]);
    stream_ungetc(stream, ch);

    if (integer != NULL) {
      *integer = is_neg ? -num : num;
    }

    return true;
  }
  return false;
}

int token_escape(int ch, stream_t stream) {
  switch (ch) {
    case '\\':
      return '\\';
    case 't':
      return '\t';
    case 'r':
      return '\r';
    case 'n':
      return '\n';
    case 'd':
      return TOKEN_NUM;
    // case ' ':
    case '(':
    case ')':
    case '{':
    // case '}':
    case '.':
    case '|':
      return ch;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      return token_oct_num(ch, stream);
    case 'x':
      return token_hex_num('0', stream);
    default:
      return TOKEN_ERR;
  }
}

typedef struct _token_rept_s {
  int min, max;
} rept_s, *rept_t;

void token_set_rept(int min, int max) {
  call_once(&rept_flag, rept_init);
  rept_t rept = tss_get(rept_key);
  if (rept == NULL) {
    rept = malloc(sizeof(rept_s));
    tss_set(rept_key, rept);
  }
  rept->min = min;
  rept->max = max;
}

int token_rept_min() {
  rept_t rept = tss_get(rept_key);
  if (rept == NULL) {
    return -1;
  }
  return rept->min;
}

int token_rept_max() {
  rept_t rept = tss_get(rept_key);
  if (rept == NULL) {
    return -1;
  }
  return rept->max;
}

int token_rept(int ch, stream_t stream) {
  // rept: {min,max}
  do {
    int min, max;

    token_skip_space(stream);
    if (!token_consume_integer(stream, &min) || min < 0) {
      break;
    }
    token_skip_space(stream);
    if (!token_expect_char(stream, ',')) {
      break;
    }
    token_skip_space(stream);
    if (!token_consume_integer(stream, &max) || max < min) {
      break;
    }
    token_skip_space(stream);
    if (!token_expect_char(stream, '}')) {
      break;
    }

    // set min and max
    token_set_rept(min, max);

    return TOKEN_REPT;
  } while (0);
  return TOKEN_ERR;
}

int token_subs(int ch, stream_t stream) {
  if (token_expect_char(stream, '?')) {
    if (token_expect(stream, (uchar*)"&!", 2)) {
      // ambi-pattern: (?&!pattern)
      return TOKEN_AMBI;
    } else if (token_expect(stream, (uchar*)"<!", 2)) {
      // anto-pattern: (?<!pattern)
      return TOKEN_ANTO;
    }
  }
  return TOKEN_SUBS;
}

int token_meta(int ch, stream_t stream) {
  switch (ch) {
    case '(':  // sub-pattern
      return token_subs(ch, stream);
    case ')':
      return TOKEN_SUBE;
    case '{':  // rept
      return token_rept(ch, stream);
    case '.':
      return TOKEN_ANY;
    case '|':
      return TOKEN_ALT;
    default:
      return ch;
  }
}

int token_next(stream_t stream, dstr_t* token) {
  int ch;
  dynabuf_s buffer;

  dynabuf_init(&buffer, 31);

  while ((ch = stream_getc(stream)) != EOF) {
    if (ch == '\\') {  // 处理转义字符
      ch = token_escape(stream_getc(stream), stream);
    } else {
      ch = token_meta(ch, stream);
    }

    if (ch <= TOKEN_ERR) {
      break;
    }

    char ch0 = (uchar)ch;
    dynabuf_write(&buffer, &ch0, 1);
  }

  if (ch == EOF) {
    if (dynabuf_empty(&buffer)) {
      ch = TOKEN_EOF;
    } else {
      ch = TOKEN_TEXT;
    }
  }

  if (token != NULL) {
    if (dynabuf_empty(&buffer) || ch == TOKEN_ERR) {
      // 出错不返回
      *token = NULL;
    } else {
      strlen_s tok = dynabuf_content(&buffer);
      *token = dstr(&tok);
    }
  }

  dynabuf_clean(&buffer);

  return ch;
}
