#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "libhpjsrpc.h"
#include "jsmn.h"

#ifdef BRANCHLESS
# define min(x, y)    ((int) y + (((int) x - (int) y) & ((int) x - (int) y) >> 31))
#else
# define min(x, y)    ((x > y) ? y : x)
#endif /* BRANCHLESS */

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

struct hpjsrpc_engine_t {
  art_tree                        method_tree;
  uint32_t                        method_count;
};

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
hpjsrpc_new (hpjsrpc_engine_t **pptr) {
  HPJSRPC_RETURN     rc;
  hpjsrpc_engine_t   *new_engine = calloc(sizeof(*new_engine), 1);

  if (NULL == new_engine) {
    return HPJSRPC_ASSERTION_ERROR;
  }
  
  rc = hpjsrpc_init(new_engine);
  if (HPJSRPC_NO_ERROR != rc) {
    return rc;
  }

  *pptr = new_engine;

  return HPJSRPC_NO_ERROR;

} /* hpjsrpc_new() */

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
hpjsrpc_init (hpjsrpc_engine_t *engine) {

  if (NULL == engine) {
    return HPJSRPC_ASSERTION_ERROR;
  }

  engine->method_count = 0;
  if (0 != init_art_tree(&engine->method_tree)) {
    return HPJSRPC_ASSERTION_ERROR;
  }

  return HPJSRPC_NO_ERROR;

} /* hpjsrpc_init() */

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
hpjsrpc_done (hpjsrpc_engine_t *engine) {

  if (NULL == engine) {
    return HPJSRPC_ASSERTION_ERROR;
  }

  destroy_art_tree(&engine->method_tree);

  return HPJSRPC_NO_ERROR;

} /* hpjsrpc_done() */

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
hpjsrpc_destroy (hpjsrpc_engine_t *engine) {
  HPJSRPC_RETURN rc;

  if (NULL == engine) {
    return HPJSRPC_ASSERTION_ERROR;
  }

  rc = hpjsrpc_done(engine);
  if (HPJSRPC_NO_ERROR != rc) {
    return rc;
  }

  free(engine);

  return HPJSRPC_NO_ERROR;

} /* hpjsrpc_done() */
/* ------------------------------------------------------------------------- */

static void
dump_jsmn_tree_depth_first (
  const char * const        pcJson,
  const jsmntok_t * const   psToks,
  unsigned int              uiSelf,
  unsigned int              uiLevel
) {

  if (!psToks || !pcJson) {
    assert(0);
    return;
  }

  printf("%*c", uiLevel, ' ');
  printf("%.*s", psToks[uiSelf].end - psToks[uiSelf].start, &pcJson[psToks[uiSelf].start]);
  switch (psToks[uiSelf].type) {
    case JSMN_OBJECT:
      printf("  (object, ");
      break;
    case JSMN_ARRAY:
      printf("  (array, ");
      break;
    case JSMN_STRING:
      printf("  (string, ");
      break;
    case JSMN_PRIMITIVE:
      printf("  (primitive, ");
      break;
    default:
      assert(0);
  }

  printf("size: %d, start: %d, end: %d, first child: %d, next sibling: %d)\n",
    psToks[uiSelf].size, psToks[uiSelf].start, psToks[uiSelf].end,
    psToks[uiSelf].first_child, psToks[uiSelf].next_sibling);

  if (-1 != (uiSelf = psToks[uiSelf].first_child)) {
    uiLevel += 2;
    dump_jsmn_tree_depth_first(pcJson, psToks, uiSelf, uiLevel);
    while (-1 != (uiSelf = psToks[uiSelf].next_sibling)) {
      dump_jsmn_tree_depth_first(pcJson, psToks, uiSelf, uiLevel);
    }
  }
} /* dump_jsmn_tree_depth_first() */

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
rpc_register_methods (
  hpjsrpc_engine_t             *server,
  const hpjsrpc_method_t       *methods,
  size_t                        method_count
) {
  for (size_t ii = 0; ii < method_count; ++ii) {
    if (!((0 != methods[ii].name[0]) & (NULL != methods[ii].func))) {
      return HPJSRPC_RPC_ERROR_INSTALLMETHODS;
    }

    void *method = (hpjsrpc_method_t *) art_search(&server->method_tree,
      (unsigned char *) methods[ii].name, methods[ii].name_length_in_bytes);
    if (unlikely(NULL != method)) {
      return HPJSRPC_RPC_ERROR_INSTALLMETHODS;
    }

    void *rp = art_insert(&server->method_tree, (unsigned char *) methods[ii].name,
      methods[ii].name_length_in_bytes, (void *) &methods[ii]);
    if (NULL != rp) {
      return HPJSRPC_RPC_ERROR_INSTALLMETHODS;
    }
  }

  return HPJSRPC_NO_ERROR;
}

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
rpc_parse_request (
  const char * const      buffer,
  size_t                  buffer_length_in_bytes,
  hpjsrpc_request_t      *req
) {
  int iRes;
  jsmn_parser sParser;

  jsmn_init(&sParser);
  iRes = jsmn_parse(&sParser, buffer, buffer_length_in_bytes, req->tokens,
    req->max_token_count);

  // if error during parse, return translated code
  if (iRes < 0) {
    switch (iRes) {
      case JSMN_ERROR_INVAL:
        return HPJSRPC_PARSE_ERROR_INVAL;
      case JSMN_ERROR_NOMEM:
        return HPJSRPC_PARSE_ERROR_NOMEM;
      case JSMN_ERROR_PART:
        return HPJSRPC_PARSE_ERROR_PART;
      default:
        assert(0);
    }
  }

  // ** DEBUG **
  if (iRes < 0) {
    dump_jsmn_tree_depth_first(buffer, req->tokens, 0, 0);
  }

  req->buffer = buffer;
  req->buffer_length_in_bytes = buffer_length_in_bytes;
  req->tokens = req->tokens;
  req->token_count = iRes;
  req->versionToken = NULL;
  req->methodToken = NULL;
  req->paramsToken = NULL;
  req->idToken = NULL;

  return HPJSRPC_NO_ERROR;
}

// -------------------------------------------------------------------------- //
//
// Here we check the RPC requirements (JSON-RPC Version 2)
//
// INPUT:   command string, which contains exactly one line (\n terminated)
// EFFECTS: loads static RPC-related tokens
// OUTPUT:  status code
//
// ------------------------------------------------------------------------- //
/**
 * Validate the RPC request to ensure it matches the specification.
 *
 * An RPC call is represented by sending a Request object to a Server. The
 * Request object contains the following members:
 *
 *  jsonrpc
 *    A String specifying the version of the JSON-RPC protocol. MUST be exactly
 *    "2.0"
 *
 *  method
 *    A String containing the name of the method to be invoked. Method names
 *    that begin with the word rpc followed by a period character (U+002E or
 *    ASCII 46) are reserved for rpc-internal methods and extensions and MUST
 *    NOT be used for anything else.
 *
 *  params
 *    A structured value that holds the parameter values to be used during the
 *    invocation of the method. This member MAY be omitted.
 *
 *  id
 *    An identifier established by the Client that MUST contain a String,
 *    Number, or NULL value if included. If it is not included it is assumed
 *    to be a notification. The value SHOULD normally not be Null [1] and
 *    Numbers SHOULD NOT contain fractional parts [2].
 */
static HPJSRPC_RETURN
rpc_validate_request_format (
  hpjsrpc_request_t   *req
) {
  req->versionToken = NULL;
  req->methodToken = NULL;
  req->paramsToken = NULL;
  req->idToken = NULL;

  /*
   * In this function, we're validating a single request object. In the case
   * where the client has submitted a batch request, each request in the batch
   * array is validated individually.
   */
  if (unlikely(!((0 < req->token_count) & (JSMN_OBJECT == req->tokens[0].type)))) {
    return HPJSRPC_RPC_ERROR_INVALIDOUTER;
  }

  if (likely(0 < req->tokens[0].size)) {
    int sibling = req->tokens[0].first_child;
    do {
      switch (req->tokens[sibling].end - req->tokens[sibling].start) {
        case 6:
          if (0 == memcmp("method", &req->buffer[req->tokens[sibling].start], 6)) {
            req->methodToken = &req->tokens[sibling];
          } else if (0 == memcmp("params", &req->buffer[req->tokens[sibling].start], 6)) {
            req->paramsToken = &req->tokens[sibling];
          }
          break;
        case 7:
          if (0 == memcmp("jsonrpc", &req->buffer[req->tokens[sibling].start], 7)) {
            req->versionToken = &req->tokens[sibling];
          }
          break;
        case 2:
          if (0 == memcmp("id", &req->buffer[req->tokens[sibling].start], 2)) {
            req->idToken = &req->tokens[sibling];
          }
          break;
      }
    } while (-1 != (sibling = req->tokens[sibling].next_sibling));
  }

  if (unlikely(!((req->versionToken != NULL)
      && ((1 == req->versionToken->size)
          & (JSMN_STRING == req->tokens[req->versionToken->first_child].type)
          & (3 == (req->tokens[req->versionToken->first_child].end
            - req->tokens[req->versionToken->first_child].start))
          & (0 == memcmp("2.0", &req->buffer[req->tokens[req->versionToken->first_child].start],
            min(3, (req->tokens[req->versionToken->first_child].end
              - req->tokens[req->versionToken->first_child].start)))))))) {
    return HPJSRPC_RPC_ERROR_INVALIDVERSION;
  }

  if (unlikely(!((NULL != req->methodToken)
      && ((1 == req->methodToken->size)
          & (JSMN_STRING == req->tokens[req->methodToken->first_child].type))))) {
    return HPJSRPC_RPC_ERROR_INVALIDMETHOD;
  }

  if (unlikely(!((NULL != req->paramsToken)
      & (1 == req->paramsToken->size)
      & (0 != ((JSMN_OBJECT == req->tokens[req->paramsToken->first_child].type)
               | (JSMN_ARRAY == req->tokens[req->paramsToken->first_child].type)))))) {
    return HPJSRPC_RPC_ERROR_INVALIDPARAMS;
  }

  if (NULL != req->idToken) {
    if (unlikely(!((1 == req->idToken->size)
        & (0 != ((JSMN_STRING == req->tokens[req->idToken->first_child].type)
                  | (JSMN_PRIMITIVE == req->tokens[req->idToken->first_child].type)))))) {
      return HPJSRPC_RPC_ERROR_INVALIDID;
    }

    req->is_notification = false;
    if (JSMN_PRIMITIVE == req->tokens[req->idToken->first_child].type) {
      static const uint8_t validIdPrimitiveTable[256] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      };
      uint8_t condvar = 0x01;
      for (size_t ii = req->tokens[req->idToken->first_child].start
            ; ii < req->tokens[req->idToken->first_child].end
            ; ++ii) {
        condvar &= validIdPrimitiveTable[(uint8_t) req->buffer[ii]];
      }
      if (0x01 != condvar) {
        return HPJSRPC_RPC_ERROR_INVALIDID;
      }
      if ((4 == (req->idToken->end - req->idToken->start))
        && (0 == memcmp("NULL", &req->buffer[req->tokens[req->idToken->first_child].start], 4))) {
        req->is_notification = true;
      }
    }
  } else {
    req->is_notification = true;
  }

  return HPJSRPC_NO_ERROR;

} /* rpc_validate_request_format() */

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
rpc_validate_method (
  hpjsrpc_request_t   *req
) {
  unsigned char  *methodName[MAX_METHOD_NAME_LENGTH_IN_BYTES] = { 0 };
  jsmntok_t      *methodToken = &req->tokens[req->methodToken->first_child];
  size_t          methodNameLen = (size_t) (methodToken->end - methodToken->start);

  if (methodNameLen >= MAX_METHOD_NAME_LENGTH_IN_BYTES) {
    return HPJSRPC_RPC_ERROR_METHODNOTFOUND;
  }

  memcpy(methodName, &req->buffer[methodToken->start], methodNameLen);
  req->method = (hpjsrpc_method_t *) art_search(&req->engine->method_tree,
    (unsigned char *) methodName, (methodNameLen + 1));
  if (unlikely(NULL == req->method)) {
    return HPJSRPC_RPC_ERROR_METHODNOTFOUND;
  }

  __builtin_prefetch(req->method->func, 0, 1);
  return HPJSRPC_NO_ERROR;

} /* rpc_validate_method() */

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
rpc_validate_method_call (
  hpjsrpc_request_t   *req
) {
  /* JHH FIX */
  return HPJSRPC_NO_ERROR;

  if (req->paramsToken == NULL
      || req->paramsToken->size == 0
      || req->tokens[req->paramsToken->first_child].size <= 0) {
    if (0 != req->method->param_count) {
      return HPJSRPC_RPC_ERROR_PARAMSMISMATCH;
    }
    return HPJSRPC_NO_ERROR;
  }

printf("param_count = %zd, size = %d\n",
  req->method->param_count,
  req->tokens[req->paramsToken->first_child].size);

  if (req->method->param_count !=
    req->tokens[req->paramsToken->first_child].size) {
    return HPJSRPC_RPC_ERROR_PARAMSMISMATCH;
  }

  size_t param_number = 0;
  int sibling = req->tokens[req->paramsToken->first_child].first_child;
  do {
printf("type = %d, type = %d, [%.*s]\n",
  req->tokens[sibling].type,
  req->method->param[param_number],
  (req->tokens[sibling].end - req->tokens[sibling].start),
  &req->buffer[req->tokens[sibling].start]);
    if (req->tokens[sibling].type != req->method->param[param_number++]) {
      return HPJSRPC_RPC_ERROR_PARAMSMISMATCH;
    }
  } while (-1 != (sibling = req->tokens[sibling].next_sibling));

  return HPJSRPC_NO_ERROR;
}

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
rpc_invoke_method (
  hpjsrpc_request_t   *req,
  hpjsrpc_response_t  *res
) {
  HPJSRPC_RETURN rc;

  if (true == req->is_notification || 0 == res->buffer.capacity_in_bytes) {
    return req->method->func(req, res);
  }

  if (JSMN_STRING == req->tokens[req->idToken->first_child].type) {
    rc = hpjsrpc_buffer_printf(&res->buffer,
      "{\"jsonrpc\":\"2.0\",\"id\":%s%.*s%s,\"result\":",
      (JSMN_STRING == req->tokens[req->idToken->first_child].type) ? "\"" : "",
      (req->tokens[req->idToken->first_child].end
        - req->tokens[req->idToken->first_child].start),
      &req->buffer[req->tokens[req->idToken->first_child].start],
      (JSMN_STRING == req->tokens[req->idToken->first_child].type) ? "\"" : "");
  } else {
    rc = hpjsrpc_buffer_printf(&res->buffer,
      "{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"result\":",
      (req->tokens[req->idToken->first_child].end
        - req->tokens[req->idToken->first_child].start),
      &req->buffer[req->tokens[req->idToken->first_child].start]);
  }
  if (HPJSRPC_NO_ERROR != rc) {
    return rc;
  }

  rc = req->method->func(req, res);
  if (HPJSRPC_NO_ERROR != rc) {
    return rc;
  }

  rc = hpjsrpc_buffer_printf(&res->buffer, "}\0");

  return rc;

} /* rpc_invoke_method() */

/* ------------------------------------------------------------------------- */

static int
rpc_print_error_json (
  hpjsrpc_request_t    *req,
  hpjsrpc_response_t   *res,
  HPJSRPC_RETURN        return_code
) {
  res->buffer.size_in_bytes = 0;
  HPJSRPC_RETURN rc = hpjsrpc_buffer_printf(&res->buffer,
    "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%s%.*s%s}",
    return_code,
    hpjsrpc_error_string(return_code),
    (JSMN_STRING == req->tokens[req->idToken->first_child].type) ? "\"" : "",
    (req->tokens[req->idToken->first_child].end
      - req->tokens[req->idToken->first_child].start),
    &req->buffer[req->tokens[req->idToken->first_child].start],
    (JSMN_STRING == req->tokens[req->idToken->first_child].type) ? "\"" : "");

  return rc;
}

/* ------------------------------------------------------------------------- */

HPJSRPC_RETURN
rpc_process_request (
  hpjsrpc_request_t      *req,
  hpjsrpc_response_t     *res
) {

  HPJSRPC_RETURN  rc = HPJSRPC_NO_ERROR;
  TicTocTimer     command_clock = tic();
  TicTocTimer     clock;

  __builtin_prefetch(req->buffer, 0, 1);
  __builtin_prefetch(&req->tokens, 0, 1);
  __builtin_prefetch(&res->buffer, 0, 1);
  __builtin_prefetch(&res->buffer.data, 0, 1);

  req->stat_validate_request_time = 0;
  req->stat_validate_method_time = 0;
  req->stat_invoke_method_time = 0;


  // handle batch
/*
  rc = rpc_parse_request(pcCommand, iCommandLen);
  if (rc != HPJSRPC_NO_ERROR) {
      goto L_done;
  }
*/

  clock = tic();
  rc = rpc_validate_request_format(req);
  req->stat_validate_request_time = (uint64_t) (toc(&clock) * 1E6f);
  if (rc != HPJSRPC_NO_ERROR) {
      goto L_done;
  }

  clock = tic();
  rc = rpc_validate_method(req);
  req->stat_validate_method_time = (uint64_t) (toc(&clock) * 1E6f);
  if (rc != HPJSRPC_NO_ERROR) {
      goto L_done;
  }

#if 0
  rc = rpc_validate_method_call(req);
  if (rc != HPJSRPC_NO_ERROR) {
      goto L_done;
  }
#endif

  clock = tic();
  rc = rpc_invoke_method(req, res);
  req->stat_invoke_method_time = (uint64_t) (toc(&clock) * 1E6f);
  if (rc != HPJSRPC_NO_ERROR) {
      goto L_done;
  }

L_done:

  //form json response
  if (true == req->is_notification) {
    if (res->buffer.data && res->buffer.capacity_in_bytes > 0) {
      res->buffer.data[0] = 0;
    }
  } else if (likely(HPJSRPC_NO_ERROR != rc)) {
    switch (rc) {
      case HPJSRPC_NO_ERROR:
        break;

      case HPJSRPC_PARSE_ERROR_NOMEM:
      case HPJSRPC_PARSE_ERROR_INVAL:
      case HPJSRPC_PARSE_ERROR_PART:
        rc = rpc_print_error_json(req, res, JSONRPC_20_PARSE_ERROR);
        break;

      //request malformed
      case HPJSRPC_RPC_ERROR_INVALIDOUTER:
      case HPJSRPC_RPC_ERROR_INVALIDVERSION:
      case HPJSRPC_RPC_ERROR_INVALIDID:
      case HPJSRPC_RPC_ERROR_INVALIDMETHOD:
      case HPJSRPC_RPC_ERROR_INVALIDPARAMS:
        rc = rpc_print_error_json(req, res, JSONRPC_20_INVALID_REQUEST);
        break;

      case HPJSRPC_RPC_ERROR_PARAMSMISMATCH:
        rc = rpc_print_error_json(req, res, JSONRPC_20_INVALIDPARAMS);
        break;

      case HPJSRPC_RPC_ERROR_METHODNOTFOUND:
        rc = rpc_print_error_json(req, res, JSONRPC_20_METHODNOTFOUND);
        break;

      case HPJSRPC_RPC_ERROR_INSTALLMETHODS:
      case HPJSRPC_RPC_ERROR_OUTOFRESBUF:
        rc = rpc_print_error_json(req, res, JSONRPC_20_INTERNALERROR);
        break;
      default:
        assert(0);
    }

    //plus a special return code
    if (HPJSRPC_RPC_ERROR_OUTOFRESBUF == rc) {
      if (res->buffer.data && res->buffer.capacity_in_bytes > 0) {
        res->buffer.data[0] = 0;
      }
    }
  }

  req->stat_process_request_time = (uint64_t) (toc(&command_clock) * 1E6f);
  return rc;
}

/* ------------------------------------------------------------------------- */

const char *
hpjsrpc_error_string (HPJSRPC_RETURN rc) {
  switch (rc) {
    case HPJSRPC_NO_ERROR:
      return "HPJSRPC_NO_ERROR: no error";
    case HPJSRPC_PARSE_ERROR_NOMEM:
      return "HPJSRPC_PARSE_ERROR_NOMEM: not enough tokens available";
    case HPJSRPC_PARSE_ERROR_INVAL:
      return "HPJSRPC_PARSE_ERROR_INVAL: invalid json character encountered";
    case HPJSRPC_PARSE_ERROR_PART:
      return "HPJSRPC_PARSE_ERROR_PART: json string not terminated";
    case HPJSRPC_RPC_ERROR_INVALIDOUTER:
      return "HPJSRPC_PARSE_ERROR_NOTOBJECT: outer json layer is not an object";
    case HPJSRPC_RPC_ERROR_INVALIDVERSION:
      return "HPJSRPC_RPC_ERROR_INVALIDVERSION: Version string must be present and equal to 2.0";
    case HPJSRPC_RPC_ERROR_INVALIDID:
      return "HPJSRPC_RPC_ERROR_INVALIDID: Id, if present, must be string/number/null";
    case HPJSRPC_RPC_ERROR_INVALIDMETHOD:
      return "HPJSRPC_RPC_ERROR_INVALIDMETHOD: Method, must be present and must be a string";
    case HPJSRPC_RPC_ERROR_INVALIDPARAMS:
      return "HPJSRPC_RPC_ERROR_INVALIDPARAMS: Params, if present, must be array/object";
    case HPJSRPC_RPC_ERROR_METHODNOTFOUND:
      return "HPJSRPC_RPC_ERROR_METHODNOTDEFINED: no such method defined, or attempting to define NULL method";
    case HPJSRPC_RPC_ERROR_PARAMSMISMATCH:
      return "HPJSRPC_RPC_ERROR_PARAMSMISMATCH: params mismatch for requested method";
    case HPJSRPC_RPC_ERROR_INSTALLMETHODS:
      return "HPJSRPC_RPC_ERROR_METHODFORMAT: RPC method install failed, check name/sig/function prototype";
    case HPJSRPC_RPC_ERROR_OUTOFRESBUF:
      return "HPJSRPC_RPC_ERROR_PRINTRESPONSE: Ran out of buffer printing JSON response";

    /* These messages are purposefully short, as they will be sent over the wire */
    case JSONRPC_20_PARSE_ERROR:
      return "json parsing error";
    case JSONRPC_20_INVALID_REQUEST:
      return "json rpc structure error";
    case JSONRPC_20_METHODNOTFOUND:
      return "remote method not found";
    case JSONRPC_20_INVALIDPARAMS:
      return "wrong params for remote method";
    case JSONRPC_20_INTERNALERROR:
      return "internal error";

    default:
      assert(0);
  }
  return NULL;
}
/* vi: set et sw=2 ts=2: */

