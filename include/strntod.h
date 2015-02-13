/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */
#ifndef SEASON_STRNTOD
#define SEASON_STRNTOD

/* pull in size_t and int types */
#include <string.h>
#include <stdint.h>

const char* strntod(const char* src, size_t len, double* result);

#endif
