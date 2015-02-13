#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "libhpjsrpc.h"
#include "strntod.h"

static HPJSRPC_RETURN echo (hpjsrpc_request_t *req, hpjsrpc_response_t *res);
static HPJSRPC_RETURN rpc_pow (hpjsrpc_request_t *req, hpjsrpc_response_t *res);
static HPJSRPC_RETURN subtract_positional (hpjsrpc_request_t *req, hpjsrpc_response_t *res);
static HPJSRPC_RETURN subtract_named (hpjsrpc_request_t *req, hpjsrpc_response_t *res);

static hpjsrpc_method_t test_methods[] = {
  {"echo", sizeof("echo"), echo, false, 1, { JSMN_STRING }},
  {"pow", sizeof("pow"), rpc_pow, false, 2, { JSMN_PRIMITIVE, JSMN_PRIMITIVE }},
  {"subtract.positional", sizeof("subtract.positional"), subtract_positional, false, 2, { JSMN_PRIMITIVE, JSMN_PRIMITIVE }},
  {"subtract.named", sizeof("subtract.named"), subtract_named, false, 2, { JSMN_PRIMITIVE, JSMN_PRIMITIVE }},
};

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
echo (
  hpjsrpc_request_t          *req,
  hpjsrpc_response_t         *res
) {

  const jsmntok_t *paramsToken = &req->tokens[req->paramsToken->first_child];

  if (JSMN_ARRAY != paramsToken->type) {
    printf("Params token is NOT array!\n");
    return HPJSRPC_ASSERTION_ERROR;
  }

  const jsmntok_t *echoToken = &req->tokens[paramsToken->first_child];

  hpjsrpc_buffer_printf(&res->buffer, "\"%.*s\"",
    (echoToken->end - echoToken->start),
    &req->buffer[echoToken->start]);

  return HPJSRPC_NO_ERROR;

} /* echo() */

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
subtract_positional (
  hpjsrpc_request_t          *req,
  hpjsrpc_response_t         *res
) {

  const jsmntok_t *paramsToken = &req->tokens[req->paramsToken->first_child];

  if (JSMN_ARRAY != paramsToken->type) {
    printf("Params token is NOT array!\n");
    return HPJSRPC_ASSERTION_ERROR;
  }

  const jsmntok_t *num1Token = &req->tokens[paramsToken->first_child];
  const jsmntok_t *num2Token = &req->tokens[num1Token->next_sibling];
  double num1 = 0.0f;
  double num2 = 0.0f;

  strntod(&req->buffer[num1Token->start],
    (num1Token->end - num1Token->start), &num1);
  strntod(&req->buffer[num2Token->start],
    (num2Token->end - num2Token->start), &num2);

  hpjsrpc_buffer_printf(&res->buffer, "%f", (num1 - num2));

  return HPJSRPC_NO_ERROR;

} /* subtract_positional() */

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
subtract_named (
  hpjsrpc_request_t          *req,
  hpjsrpc_response_t         *res
) {

  const jsmntok_t *paramsToken = &req->tokens[req->paramsToken->first_child];

  if (JSMN_OBJECT != paramsToken->type) {
    printf("Params token is NOT object!\n");
    return HPJSRPC_ASSERTION_ERROR;
  }

  const jsmntok_t *num1Token = &req->tokens[paramsToken->first_child];
  const jsmntok_t *num2Token = &req->tokens[num1Token->next_sibling];
  double num1 = 0.0f;
  double num2 = 0.0f;

  strntod(&req->buffer[num1Token->start],
    (num1Token->end - num1Token->start), &num1);
  strntod(&req->buffer[num2Token->start],
    (num2Token->end - num2Token->start), &num2);

  hpjsrpc_buffer_printf(&res->buffer, "%f", (num1 - num2));

  return HPJSRPC_NO_ERROR;

} /* subtract_named() */

/* ------------------------------------------------------------------------- */

static HPJSRPC_RETURN
rpc_pow (
  hpjsrpc_request_t          *req,
  hpjsrpc_response_t         *res
) {

  const jsmntok_t *paramsToken = &req->tokens[req->paramsToken->first_child];

  if (JSMN_ARRAY != paramsToken->type) {
    printf("Params token is NOT array!\n");
    return HPJSRPC_ASSERTION_ERROR;
  }

  const jsmntok_t *num1Token = &req->tokens[paramsToken->first_child];
  const jsmntok_t *num2Token = &req->tokens[num1Token->next_sibling];
  char num1Buf[80] = { 0 };
  char num2Buf[80] = { 0 };
  double num1 = 0.0f;
  double num2 = 0.0f;

  memcpy(num1Buf, &req->buffer[num1Token->start],
    (num1Token->end - num1Token->start));
  memcpy(num2Buf, &req->buffer[num2Token->start],
    (num2Token->end - num2Token->start));

  num1 = atof(num1Buf);
  num2 = atof(num2Buf);

  hpjsrpc_buffer_printf(&res->buffer, "\"pow(%.*s, %.*s) = %8.6f\"",
    (num1Token->end - num1Token->start), &req->buffer[num1Token->start],
    (num2Token->end - num2Token->start), &req->buffer[num2Token->start],
    pow(num1, num2));

  return HPJSRPC_NO_ERROR;

} /* pow() */

#define MY_BUF_SIZE 2048
static char g_input[MY_BUF_SIZE];
static char g_output[MY_BUF_SIZE];

int
main (int argc, const char ** const argv) {
  HPJSRPC_RETURN      rc;
  hpjsrpc_engine_t   *hpjsrpc;
  hpjsrpc_request_t   req;
  hpjsrpc_response_t  res;

  rc = hpjsrpc_new(&hpjsrpc);
  if (HPJSRPC_NO_ERROR != rc) {
    fprintf(stderr, "Failed to initialize RPC engine\n");
    return 1;
  }

  rc = rpc_register_methods(hpjsrpc, (const hpjsrpc_method_t *) test_methods,
    (sizeof(test_methods) / sizeof(test_methods[0])));
  if (HPJSRPC_NO_ERROR != rc) {
    fprintf(stderr, "Failed to register methods with RPC engine\n");
    return 1;
  }

  size_t status = fread(g_input, 1, sizeof(g_input),  stdin);
  if (status == 0) {
    fprintf(stderr, "fread(): errno=%d\n", errno);
    return 1;
  }

  req.engine = hpjsrpc;
  req.tokens = calloc(sizeof(jsmntok_t), 1024);
  req.max_token_count = 1024;
  rc = rpc_parse_request(g_input, status, &req);
  if (HPJSRPC_NO_ERROR != rc) {
    fprintf(stderr, "Failed to parse (rc = %d)\n", rc);
    return 1;
  }

  res.buffer.data = (uint8_t *) g_output;
  res.buffer.size_in_bytes = 0;
  res.buffer.capacity_in_bytes = MY_BUF_SIZE;

  rc = rpc_process_request(&req, &res);

  if (res.buffer.size_in_bytes > 0) {
    printf(">> %s\n", g_output);
  } else {
    printf(">> no reply\n");
  }
  printf("%s\n", hpjsrpc_error_string(rc));

  free(req.tokens);
  rc = hpjsrpc_destroy(hpjsrpc);
  if (HPJSRPC_NO_ERROR != rc) {
    fprintf(stderr, "Failed to initialize RPC engine\n");
    return 1;
  }

  return 0;

} /* main () */
/* vi: set et sw=2 ts=2: */

