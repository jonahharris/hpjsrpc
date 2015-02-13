
#ifndef HPJSRPC_H
#define	HPJSRPC_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "art.h"
#include "jsmn.h"
#include "tictoc.h"

#ifdef	__cplusplus
"C" {
#endif

#define MAX_PARAMS                        16
#define MAX_METHOD_NAME_LENGTH_IN_BYTES   127

typedef enum {
  // -- These are in the range of local error codes [-32000..-32900]
  /* No error */
  HPJSRPC_NO_ERROR = 0,
  /* Not enough tokens were provided */
  HPJSRPC_PARSE_ERROR_NOMEM = -32000,
  /* Invalid character inside JSON string */
  HPJSRPC_PARSE_ERROR_INVAL = -32001,
  /* The string is not a full JSON packet, more bytes expected */
  HPJSRPC_PARSE_ERROR_PART = -32002,
  /* Outer "shell" in incoming JSON must always be an object */
  HPJSRPC_RPC_ERROR_INVALIDOUTER = -32003,
  /* Version string must be present */
  HPJSRPC_RPC_ERROR_INVALIDVERSION = -32004,
  /* Id, if present, must be string/number/null */
  HPJSRPC_RPC_ERROR_INVALIDID = -32005,
  /* Method, must be present and must be a string */
  HPJSRPC_RPC_ERROR_INVALIDMETHOD = -32006,
  /* Params, if present, must be array/object */
  HPJSRPC_RPC_ERROR_INVALIDPARAMS = -32007,
  /* Requested RPC method is not available locally */
  HPJSRPC_RPC_ERROR_METHODNOTFOUND = -32008,
  /* Given params for method do not match those defined locally */
  HPJSRPC_RPC_ERROR_PARAMSMISMATCH = -32009,
  /* RPC method install failed, check name/sig/function prototype */
  HPJSRPC_RPC_ERROR_INSTALLMETHODS = -32010,
  /* Ran out of buffer printing JSON response */
  HPJSRPC_RPC_ERROR_OUTOFRESBUF = -32011,
  /* Assertion */
  HPJSRPC_ASSERTION_ERROR = -32012,

  // -- These are reserved JSONRPC code values
  /* The JSON sent is not a valid Request object */
  JSONRPC_20_INVALID_REQUEST      = -32600,
  /* The method does not exist / is not available */
  JSONRPC_20_METHODNOTFOUND       = -32601,
  /* Invalid method parameters(s) */
  JSONRPC_20_INVALIDPARAMS        = -32602,
  /* Internal JSON-RPC error */
  JSONRPC_20_INTERNALERROR        = -32603,
  /* Invalid JSON was received by the server. Error while parsing JSON text */
  JSONRPC_20_PARSE_ERROR          = -32700,
} HPJSRPC_RETURN;

typedef struct hpjsrpc_engine_t hpjsrpc_engine_t;
typedef struct hpjsrpc_method_t hpjsrpc_method_t;
typedef struct hpjsrpc_request_t hpjsrpc_request_t;
typedef struct hpjsrpc_response_t hpjsrpc_response_t;

typedef struct {
  uint8_t                        *data;
  size_t                          size_in_bytes;
  size_t                          capacity_in_bytes;
} hpjsrpc_buffer_t;

static inline void
hpjsrpc_buffer_rewind (hpjsrpc_buffer_t *buf) {
  buf->size_in_bytes = 0;
}

static inline HPJSRPC_RETURN
hpjsrpc_buffer_printf (hpjsrpc_buffer_t *buf, char *format, ...) {
  size_t  avail_size_in_bytes = (buf->capacity_in_bytes - buf->size_in_bytes);
  va_list args;
  va_start(args, format);
  int len = vsnprintf((char *)(buf->data + buf->size_in_bytes),
    avail_size_in_bytes, format, args);
  va_end(args);
  if (len >= avail_size_in_bytes) {
    return HPJSRPC_RPC_ERROR_OUTOFRESBUF;
  }
  buf->size_in_bytes += len;
  va_end(args);
  return HPJSRPC_NO_ERROR;
}

typedef HPJSRPC_RETURN (*hpjsrpc_method_prototype) (hpjsrpc_request_t *, hpjsrpc_response_t *);

struct hpjsrpc_method_t {
  uint8_t                         name[(MAX_METHOD_NAME_LENGTH_IN_BYTES + 1)];
  size_t                          name_length_in_bytes;
  hpjsrpc_method_prototype        func;
  bool                            is_notification;
  size_t                          param_count;
  jsmntype_t                      param[MAX_PARAMS];
};

struct hpjsrpc_request_t {
  hpjsrpc_engine_t               *engine;
  const char                     *buffer;
  jsmntok_t                      *tokens;
  const jsmntok_t                *versionToken;
  const jsmntok_t                *methodToken;
  const jsmntok_t                *paramsToken;
  const jsmntok_t                *idToken;
  size_t                          token_count;
  size_t                          max_token_count;
  size_t                          buffer_length_in_bytes;
  const hpjsrpc_method_t         *method;
  bool                            is_notification;
  uint64_t                        stat_validate_request_time;
  uint64_t                        stat_validate_method_time;
  uint64_t                        stat_invoke_method_time;
  uint64_t                        stat_process_request_time;
};

struct hpjsrpc_response_t {
  hpjsrpc_buffer_t                buffer;
};

HPJSRPC_RETURN rpc_register_methods (
  hpjsrpc_engine_t             *engine,
  const hpjsrpc_method_t       *methods,
  size_t                        method_count);

HPJSRPC_RETURN hpjsrpc_new (hpjsrpc_engine_t **pptr);
HPJSRPC_RETURN hpjsrpc_init (hpjsrpc_engine_t *pptr);
HPJSRPC_RETURN hpjsrpc_done (hpjsrpc_engine_t *pptr);
HPJSRPC_RETURN hpjsrpc_destroy (hpjsrpc_engine_t *pptr);

HPJSRPC_RETURN rpc_parse_request (
  const char * const      buffer,
  size_t                  buffer_length_in_bytes,
  hpjsrpc_request_t      *req);

const char *hpjsrpc_error_string (HPJSRPC_RETURN rc);
HPJSRPC_RETURN rpc_process_request (hpjsrpc_request_t *req, hpjsrpc_response_t *res);

#ifdef	__cplusplus
}
#endif

#endif	/* HPJSRPC_H */
/* vi: set et sw=2 ts=2: */

