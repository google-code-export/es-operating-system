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

#ifndef TCP_H_INCLUDED
#define TCP_H_INCLUDED

#include <es/endian.h>
#include <es/net/tcp.h>
#include "inet.h"

class TCPReceiver :
    public InetReceiver
{
public:
    /** Validates the received TCP packet.
     */
    bool input(InetMessenger* m)
    {
        TCPHdr* tcphdr = static_cast<TCPHdr*>(m->fix(sizeof(TCPHdr)));
        return true;
    }

    bool output(InetMessenger* m)
    {
        TCPHdr* tcphdr = static_cast<TCPHdr*>(m->fix(sizeof(TCPHdr)));
        return true;
    }

    bool error(InetMessenger* m)
    {
        TCPHdr* tcphdr = static_cast<TCPHdr*>(m->fix(sizeof(TCPHdr)));

        // Reverse src and dst
        u16 tmp = tcphdr->src;
        tcphdr->src = tcphdr->dst;
        tcphdr->dst = tmp;

        return true;
    }
};

#endif  // TCP_H_INCLUDED
