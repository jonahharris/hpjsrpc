/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include "strntod.h"

/*
 *
 * Highly inspired by
 *
 *  Google Chrome V8 -- http://v8.googlecode.com/svn/trunk/src/json-parser.h
 *   for parser correctness for JSON oddities.
 *
 *  BSD strtod.c -- http://www.opensource.apple.com/source/tcl/tcl-10/tcl/compat/strtod.c (as one example)
 *   for computation.
 */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

#include <string.h>

static int maxExponent = 511;/* Largest possible base 10 exponent.  Any
                              * exponent larger than this will already
                              * produce underflow or overflow, so there's
                              * no need to worry about additional digits.
                              */

static double powersOf10[] = {/* Table giving binary powers of 10.  Entry */
    10.,/* is 10^2^i.  Used to convert decimal */
    100.,/* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

const char* strntod(const char* src, size_t len, double* result)
{
    int sign = FALSE;
    int expSign = FALSE;

    int exp = 0;
    int digits = 0;
    int fracExp = 0;
    double dblExp;

    double fraction = 0.0;

    double *x;
    const char* p = src;
    const char* pend = src + len;

    // empty string
    if (p == pend) {
        return NULL;
    }

    // JSON number can start with a '-', not a '+'
    if (*p == '-') {
        sign = TRUE;
        p++;
    }

    // only a minus sign
    if (p == pend) {
        return NULL;
    }

    // Prefix zero is only allowed if it's the only digit before
    // a decimal point or exponent.
    if (*p == '0') {
        p++;
        if (p == pend) {
            *result = (sign ? -0.0 : 0.0);
            return p;
        }
        if ('0' <= *p && *p <= '9') {
            // 0 followed by another digit is not allowed
            return NULL;
        }
    } else {
        if (*p < '1' || *p > '9') {
            return NULL;
        };
        do {
            fraction = 10* fraction + (*p - '0');
            digits++;
            p++;
            if (p == pend) {
                if (digits < 10) {
                    *result = (sign ? -fraction : fraction);
                    return p;
                }
                // overflow
                return NULL;
            }
        } while (*p >= '0' && *p <= '9');

        if (*p != '.' && *p != 'e' && *p != 'E' && digits < 10) {
            *result = sign ? -fraction : fraction;
            return p;
        }
    }

    if (*p == '.') {
        p++;
        if (p == pend) {
            // 1234.  -> invalid
            return NULL;
        }
        if (*p < '0' || *p > '9') {
            // 1234.X --> invalid
            return NULL;
        }
        do {
            fraction = 10*fraction + (*p - '0');
            fracExp--;
            p++;
            if (p == pend) {
                goto done;
            }
        } while (*p >= '0' && *p <= '9');
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (p == pend) {
            return NULL;
        }
        if (*p == '-' || *p == '+') {
            expSign = (*p == '-' ? 1 : 0);
            p++;
            if (p == pend) {
                // 123.34e+
                return NULL;
            }
        }
        if (*p < '0' || *p > '9') {
            // 1234.34e+X
            return NULL;
        }
        do {
            exp = exp * 10 + *p - '0';
            p++;
            if (p == pend) {
                goto done;
            }
        } while ( *p >= '0' && *p < '9');
    }

done:

    if (expSign) {
        exp = fracExp - exp;
    } else {
        exp = fracExp + exp;
    }

    /*
     * Generate a floating-point number that represents the exponent.
     * Do this by processing the exponent one bit at a time to combine
     * many powers of 2 of 10. Then combine the exponent with the
     * fraction.
     */

    if (exp < 0) {
        expSign = TRUE;
        exp = -exp;
    } else {
        expSign = FALSE;
    }

    if (exp > maxExponent) {
        // overflow
        return NULL;
    }
    dblExp = 1.0;
    for (x = powersOf10; exp != 0; exp >>= 1, x += 1) {
        if (exp & 01) {
            dblExp *= *x;
        }
    }
    if (expSign) {
        fraction /= dblExp;
    } else {
        fraction *= dblExp;
    }
    *result = sign ? -fraction : fraction;
    return p;
}

/*

  http://www.opensource.apple.com/source/tcl/tcl-10/tcl/license.terms
  http://www.opensource.apple.com/source/tcl/tcl-10/tcl/compat/strtod.c

  This software is copyrighted by the Regents of the University of
  California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
  Corporation and other parties.  The following terms apply to all files
  associated with the software unless explicitly disclaimed in
  individual files.

  The authors hereby grant permission to use, copy, modify, distribute,
  and license this software and its documentation for any purpose, provided
  that existing copyright notices are retained in all copies and that this
  notice is included verbatim in any distributions. No written agreement,
  license, or royalty fee is required for any of the authorized uses.
  Modifications to this software may be copyrighted by their authors
  and need not follow the licensing terms described here, provided that
  the new terms are clearly indicated on the first page of each file where
  they apply.

  IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
  FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
  ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
  DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
  IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
  NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
  MODIFICATIONS.

  GOVERNMENT USE: If you are acquiring this software on behalf of the
  U.S. government, the Government shall have only "Restricted Rights"
  in the software and related documentation as defined in the Federal
  Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
  are acquiring the software on behalf of the Department of Defense, the
  software shall be classified as "Commercial Computer Software" and the
  Government shall have only "Restricted Rights" as defined in Clause
  252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
  authors grant the U.S. Government and others acting in its behalf
  permission to use and distribute the software in accordance with the
  terms specified in this license.
*/
