/*
 * Copyright (c) 2007
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#ifndef GUID_H_INCLUDED
#define GUID_H_INCLUDED

#include <es/uuid.h>

bool parseGuid(const char* str, Guid* u);
void printGuid(const Guid& guid);

#endif  // GUID_H_INCLUDED