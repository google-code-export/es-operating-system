/*
 * Copyright (c) 2006, 2007
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

#include "icmp4.h"

//
// ICMPReceiver
//

s16 ICMPReceiver::checksum(InetMessenger* m)
{
    int len = m->getLength();
    ICMPHdr* icmphdr = static_cast<ICMPHdr*>(m->fix(len));
    s32 sum = m->sumUp(len);
    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

bool ICMPReceiver::input(InetMessenger* m)
{
    int len = m->getLength();
    if (len < sizeof(ICMPHdr) || checksum(m) != 0)
    {
        return false;       // XXX
    }
    return true;
}

bool ICMPReceiver::output(InetMessenger* m)
{
    ICMPHdr* icmphdr = static_cast<ICMPHdr*>(m->fix(sizeof(ICMPHdr)));
    icmphdr->sum = 0;
    icmphdr->sum = checksum(m);
    m->setType(IPPROTO_ICMP);
    return true;
}

//
// ICMPEchoRequestReceiver
//

bool ICMPEchoRequestReceiver::input(InetMessenger* m)
{
    esReport("ICMPEchoRequestReceiver::input\n");

    int len = m->getLength();
    ICMPEcho* icmphdr = static_cast<ICMPEcho*>(m->fix(len));

    int pos = 14 + 60;      // XXX Assume MAC, IPv4
    u8 chunk[pos + len];
    memmove(chunk + pos, icmphdr, len);
    icmphdr = reinterpret_cast<ICMPEcho*>(chunk + pos);
    icmphdr->type = ICMPHdr::EchoReply;

    InetMessenger r(&InetReceiver::output, chunk, pos + len, pos);
    r.setRemote(m->getRemote());
    r.setLocal(m->getLocal());

    Visitor v(&r);
    adapter->accept(&v);

    return true;
}

//
// ICMPEchoReplyReceiver
//

bool ICMPEchoReplyReceiver::input(InetMessenger* m)
{
    // Resume Inet4Address::isReachable()
    notify();

    return true;
}

//
// ICMPEchoRequestReceiver
//

bool ICMPUnreachReceiver::input(InetMessenger* m)
{
    esReport("ICMPUnreachReceiver::input\n");

    int len = m->getLength();
    if (len < sizeof(ICMPUnreach))
    {
        return false;
    }
    ICMPUnreach* icmphdr = static_cast<ICMPUnreach*>(m->fix(sizeof(ICMPUnreach)));

    m->movePosition(sizeof(ICMPUnreach));
    m->setCommand(&InetReceiver::error);

    return true;
}

bool ICMPUnreachReceiver::output(InetMessenger* m)
{
    esReport("ICMPUnreachReceiver::output\n");

    m->movePosition(-sizeof(ICMPUnreach));
    ICMPUnreach* icmphdr = static_cast<ICMPUnreach*>(m->fix(sizeof(ICMPUnreach)));
    icmphdr->type = ICMPHdr::Unreach;
    icmphdr->code = ICMPUnreach::Port;
    icmphdr->sum = 0;
    icmphdr->unused = 0;
    icmphdr->mtu = 0;

    return true;
}
