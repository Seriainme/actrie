//
// Created by james on 9/28/17.
//

#include "dynabuf.h"

// dynamic buffer
// ========================================

const static size_t dynabuf_extend_space_size =
    ROUND_UP_16(DYNABUF_EXTEND_SPACE_SIZE);


static inline size_t dynabuf_extend_size(size_t old_size, size_t insert_size) {
  return old_size + (insert_size / dynabuf_extend_space_size + 1)
      * dynabuf_extend_space_size;
}

dynabuf_t dynabuf_alloc() {
  return malloc(sizeof(dynabuf_s));
}

bool dynabuf_free(dynabuf_t self) {
  if (self == NULL) return false;
  free(self->_buffer);
  free(self);
  return true;
}

bool dynabuf_init(dynabuf_t self, size_t size, bool fixed) {
  if (self == NULL) return false;

  self->_len = 0;
  self->_prop = dynabuf_prop_empty;
  if (fixed) {
    self->_prop |= dynabuf_prop_fixed;
  }

  if (size != 0) {
    // size 为 16 的倍数
    size = ROUND_UP_16(size);
    self->_buffer = malloc(size);
    if (self->_buffer == NULL) return false;
    self->_size = size;
  } else {
    self->_buffer = NULL;
    self->_size = 0;
  }
  return true;
}

bool dynabuf_clean(dynabuf_t self) {
  if (dynabuf_reset(self)) {
    free(self->_buffer);
    self->_buffer = NULL;
    self->_size = 0;
    return true;
  }
  return false;
}

bool dynabuf_reset(dynabuf_t self) {
  if (self == NULL) return false;
  self->_len = 0;
  return true;
}

strcur_s dynabuf_empty(dynabuf_t self) {
  strcur_s ret;
  if (self->_prop & dynabuf_prop_fixed)
    ret.ptr = NULL;
  else
    ret.idx = (size_t) -1;
  return ret;
}

char *dynabuf_content(dynabuf_t self, strcur_s cursor) {
  if (self->_prop & dynabuf_prop_fixed) {
    return cursor.ptr;
  } else {
    if (cursor.idx == -1)
      return NULL;
    return self->_buffer + cursor.idx;
  }
}

strlen_s dynabuf_split(dynabuf_t self, strpos_s pos) {
  strlen_s ret;
  ret.ptr = self->_buffer + pos.so;
  ret.len = pos.eo - pos.so;
  return ret;
}

strcur_s dynabuf_related(dynabuf_t self, strcur_s base, char *related) {
  if (self->_prop & dynabuf_prop_fixed) {
    return (strcur_s) {.ptr=related};
  } else {
    return (strcur_s) {
        .idx = base.idx + (related - dynabuf_content(self, base))
    };
  }
}

strcur_s dynabuf_cur2ptr(dynabuf_t self, strcur_s cursor) {
  if (self->_prop & dynabuf_prop_fixed) {
    return cursor;
  } else {
    return (strcur_s) {
        .ptr = cursor.idx == -1 ? NULL : self->_buffer + cursor.idx
    };
  }
}

size_t dynabuf_length(dynabuf_t self) {
  return self->_len;
}

strcur_s dynabuf_write(dynabuf_t self, const char *src, size_t len) {
//  if (self == NULL) return NULL;

  if (self->_len + len > self->_size) {
    if (self->_prop & dynabuf_prop_fixed) {
      fprintf(stderr, "%s(%d) - fatal: memory error!\n", __FILE__, __LINE__);
      exit(-1);
    }

    size_t new_size = dynabuf_extend_size(self->_size, len);
    char *new_buffer = malloc(new_size);
    if (new_buffer == NULL) {
      fprintf(stderr, "%s(%d) - fatal: memory error!\n", __FILE__, __LINE__);
      exit(-1);
    }
    memcpy(new_buffer, self->_buffer, self->_len);
    free(self->_buffer);
    self->_buffer = new_buffer;
    self->_size = new_size;
  }

  strcur_s ret;
  if (len > 0) {
    if (self->_prop & dynabuf_prop_fixed) {
      ret.ptr = memcpy(self->_buffer + self->_len, src, len);
    } else {
      ret.idx = self->_len;
      memcpy(self->_buffer + self->_len, src, len);
    }
    self->_len += len;
  } else {
    ret = dynabuf_empty(self);
  }

  return ret;
}

strcur_s dynabuf_write_with_zero(dynabuf_t self, const char *src, size_t len) {
//  if (self == NULL) return NULL;

  if (self->_len + len + 1 > self->_size) {
    if (self->_prop & dynabuf_prop_fixed) {
      fprintf(stderr, "%s(%d) - fatal: memory error!\n", __FILE__, __LINE__);
      exit(-1);
    }

    size_t new_size = dynabuf_extend_size(self->_size, len + 1);
    char *new_buffer = malloc(new_size);
    if (new_buffer == NULL) {
      fprintf(stderr, "%s(%d) - fatal: memory error!\n", __FILE__, __LINE__);
      exit(-1);
    }
    memcpy(new_buffer, self->_buffer, self->_len);
    free(self->_buffer);
    self->_buffer = new_buffer;
    self->_size = new_size;
  }

  strcur_s ret;
  if (len > 0) {
    if (self->_prop & dynabuf_prop_fixed) {
      ret.ptr = memcpy(self->_buffer + self->_len, src, len);
    } else {
      ret.idx = self->_len;
      memcpy(self->_buffer + self->_len, src, len);
    }
    self->_len += len;
  } else {
    if (self->_prop & dynabuf_prop_fixed) {
      ret.ptr = self->_buffer + self->_len;
    } else {
      ret.idx = self->_len;
    }
  }

  // append '\0'
  self->_buffer[self->_len] = '\0';
  self->_len++;
  return ret;
}

int dynabuf_consume_until(dynabuf_t self,
                          stream_t stream,
                          const char *delim,
                          strpos_t out_pos) {
  unsigned char buf[256], *pb = buf;
  unsigned char *s = (unsigned char*) delim;
  int ch;

  if (out_pos) out_pos->so = dynabuf_length(self);

  if (delim == NULL || delim[0] == '\0') {
    ch = 0;
  } else {
    if (delim[1] == '\0') {
      while ((ch = stream_getc(stream)) != EOF && ch != *s) {
        *(pb++) = (unsigned char) ch;
        if ((pb - buf) == 256) {
          dynabuf_write(self, buf, 256);
          pb = buf;
        }
      }
    } else {
      unsigned char table[256];
      unsigned char *p = memset (table, 0, 64);
      memset (p + 64, 0, 64);
      memset (p + 128, 0, 64);
      memset (p + 192, 0, 64);

      do {
        p[*s++] = 1;
      } while (*s);

      while ((ch = stream_getc(stream)) != EOF && !p[ch]) {
        *(pb++) = (unsigned char) ch;
        if ((pb - buf) == 256) {
          dynabuf_write(self, buf, 256);
          pb = buf;
        }
      }
    }

    dynabuf_write(self, buf, pb - buf);
  }

  if (out_pos) out_pos->eo = dynabuf_length(self);

  return ch;
}