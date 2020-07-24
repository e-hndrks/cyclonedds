/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdarg.h>
#include "idl/export.h"

int
IDL_EXPORT idl_strcasecmp(
  const char *s1,
  const char *s2);

int
IDL_EXPORT idl_strncasecmp(
  const char *s1,
  const char *s2,
  size_t n);

int
IDL_EXPORT idl_asprintf(
  char **strp,
  const char *fmt,
  ...);

int
IDL_EXPORT idl_vasprintf(
  char **strp,
  const char *fmt,
  va_list ap);

char
IDL_EXPORT *idl_strsep(char **str, const char *sep);

void
IDL_EXPORT *idl_memdup(const void *src, size_t n);

char
IDL_EXPORT *idl_strdup(
  const char *str);

IDL_EXPORT extern char *optarg;
IDL_EXPORT extern int optind, opterr;

int
IDL_EXPORT idl_getopt(
  int argc,
  char **argv,
  const char *opts);

#endif /* STRING_UTILS_H */
