/**
 * wrap.c
 *
 * @author James Yin <ywhjames@hotmail.com>
 *
 * description:
 *   We will parsing arguments and building values in this file for convert
 *   string between python and c.
 *
 *   For parse s,s#:
 *     In python3, Unicode objects are converted to C strings using 'utf-8' encoding.
 *     But in python2, Unicode objects pass back a pointer to the default encoded string
 *       version of the object if such a conversion is possible. All other read-buffer
 *       compatible objects pass back a reference to the raw internal data representation.
 *
 *   For build s#:
 *     In python3, Convert a C string and its length to a Python str object using 'utf-8' encoding.
 *     But in python2, Convert a C string and its length to a Python object.
 */
#include <Python.h>

#include "utf8ctx.h"

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

PyObject* wrap_construct_by_file(PyObject* dummy, PyObject* args) {
  const char* path;
  matcher_t matcher;

  if (!PyArg_ParseTuple(args, "s", &path)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  matcher = matcher_construct_by_file(path);
  if (matcher == NULL) {
    Py_RETURN_NONE;
  }

  return Py_BuildValue("K", matcher);
}

PyObject* wrap_construct_by_string(PyObject* dummy, PyObject* args) {
  const char* string;
  int length;
  matcher_t matcher;

  if (!PyArg_ParseTuple(args, "s#", &string, &length)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  strlen_s vocab = {.ptr = (char*)string, .len = (size_t)length};
  matcher = matcher_construct_by_string(&vocab);
  if (matcher == NULL) {
    Py_RETURN_NONE;
  }

  return Py_BuildValue("K", matcher);
}

PyObject* wrap_destroy(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  matcher_t matcher;

  if (!PyArg_ParseTuple(args, "K", &temp)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_FALSE;
  }

  matcher = (matcher_t)temp;

  matcher_destruct(matcher);

  Py_RETURN_TRUE;
}

PyObject* wrap_alloc_context(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  matcher_t matcher;
  wctx_t wctx;

  if (!PyArg_ParseTuple(args, "K", &temp)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  matcher = (matcher_t)temp;

  wctx = alloc_context(matcher);
  if (wctx == NULL) {
    Py_RETURN_NONE;
  }

  return Py_BuildValue("K", wctx);
}

PyObject* wrap_free_context(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  wctx_t wctx;

  if (!PyArg_ParseTuple(args, "K", &temp)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_FALSE;
  }

  wctx = (wctx_t)temp;

  free_context(wctx);

  Py_RETURN_TRUE;
}

PyObject* wrap_reset_context(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  wctx_t wctx;
  char* content;
  int length;

  if (!PyArg_ParseTuple(args, "Ks#", &temp, &content, &length)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  wctx = (wctx_t)temp;

  if (reset_context(wctx, content, length)) {
    Py_RETURN_TRUE;
  }

  Py_RETURN_FALSE;
}

static inline PyObject* build_matched_output(wctx_t wctx, word_t matched_word) {
  return Py_BuildValue("(s#,i,i,s#)", matched_word->keyword.ptr, matched_word->keyword.len,
                       wctx->utf8_ctx.pos[matched_word->pos.so], wctx->utf8_ctx.pos[matched_word->pos.eo],
                       matched_word->extra.ptr, matched_word->extra.len);
}

PyObject* wrap_next(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  wctx_t wctx;

  if (!PyArg_ParseTuple(args, "K", &temp)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  wctx = (wctx_t)temp;

  word_t matched_word = next(wctx);
  if (matched_word != NULL) {
    return build_matched_output(wctx, matched_word);
  }

  Py_RETURN_NONE;
}

PyObject* wrap_find_all(PyObject* dummy, PyObject* args) {
  unsigned long long temp;
  matcher_t matcher;
  char* content;
  int length;

  if (!PyArg_ParseTuple(args, "Ks#", &temp, &content, &length)) {
    fprintf(stderr, "%s:%d wrong args\n", __FUNCTION__, __LINE__);
    Py_RETURN_NONE;
  }

  matcher = (matcher_t)temp;

  wctx_t wctx = alloc_context(matcher);
  if (wctx == NULL) {
    Py_RETURN_NONE;
  }

  if (!reset_context(wctx, content, length)) {
    Py_RETURN_NONE;
  }

  PyObject* list = PyList_New(0);
  word_t matched_word = next(wctx);
  while (matched_word != NULL) {
    PyObject* item = build_matched_output(wctx, matched_word);
    PyList_Append(list, item);
    Py_DECREF(item);
  }

  free_context(wctx);

  return list;
}

static PyMethodDef wrapMethods[] = {
    {"ConstructByFile", wrap_construct_by_file, METH_VARARGS, "construct matcher by file"},
    {"ConstructByString", wrap_construct_by_string, METH_VARARGS, "construct matcher by string"},
    {"Destruct", wrap_destroy, METH_VARARGS, "destruct matcher"},
    {"AllocContext", wrap_alloc_context, METH_VARARGS, "alloc iterator context"},
    {"FreeContext", wrap_free_context, METH_VARARGS, "free iterator context"},
    {"ResetContext", wrap_reset_context, METH_VARARGS, "reset iterator context"},
    {"Next", wrap_next, METH_VARARGS, "iterator next"},
    {"FindAll", wrap_find_all, METH_VARARGS, "find all matched strings"},
    {NULL, NULL}};

#ifdef IS_PY3K
static PyModuleDef matchModule = {PyModuleDef_HEAD_INIT, "_actrie", NULL, -1, wrapMethods};

PyMODINIT_FUNC PyInit__actrie() {
#else
PyMODINIT_FUNC init_actrie() {
#endif

  PyObject* m;

#ifdef IS_PY3K
  m = PyModule_Create(&matchModule);
  return m;
#else
  m = Py_InitModule("_actrie", wrapMethods);
#endif
}
