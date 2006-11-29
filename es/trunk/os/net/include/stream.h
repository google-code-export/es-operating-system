/*
 * Copyright (c) 2006
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

#ifndef STREAM_H_INCLUDED
#define STREAM_H_INCLUDED

#include <es/endian.h>
#include <es/net/udp.h>
#include "inet.h"

class StreamReceiver :
    public InetReceiver
{
public:
    bool input(InetMessenger* m)
    {
        return true;
    }

    bool output(InetMessenger* m)
    {
        return true;
    }

    bool error(InetMessenger* m)
    {
        return true;
    }
};

#endif  // STREAM_H_INCLUDED
