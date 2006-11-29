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

#include <string.h>
#include <new>
#include <es/handle.h>
#include "inet4.h"

ARPFamily::ARPFamily(InFamily* inFamily) :
    inFamily(inFamily),
    scopeMux(&scopeAccessor, &scopeFactory),
    arpReceiver(inFamily),
    arpFactory(&arpAdapter),
    arpMux(&arpAccessor, &arpFactory)
{
    ASSERT(inFamily);

    arpProtocol.setReceiver(&arpReceiver);
    arpAdapter.setReceiver(&inFamily->addressAny);

    Conduit::connectAA(&scopeMux, &arpProtocol);
    Conduit::connectBA(&arpProtocol, &arpMux);

    Socket::addAddressFamily(this);
}

// StateInit

void Inet4Address::
StateInit::start(Inet4Address* a)
{
    a->setState(stateIncomplete);
    a->run();   // Invoke expired
}

bool Inet4Address::
StateInit::input(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateInit::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateInit::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StateIncomplete

void Inet4Address::
StateIncomplete::expired(Inet4Address* a)
{
    // Send request
    if (++a->timeoutCount <= 6)
    {
        esReport("StateIncomplete::timeoutCount: %d\n", a->timeoutCount);

        Handle<Inet4Address> src = a->inFamily->selectSourceAddress(a);
        if (!src)
        {
            a->setState(stateInit);
            return;
        }

        u8 chunk[sizeof(DIXHdr) + sizeof(ARPHdr)];
        InetMessenger m(&InetReceiver::output, chunk, sizeof chunk, sizeof(DIXHdr));
        m.setScopeID(a->getScopeID());

        ARPHdr* arphdr = static_cast<ARPHdr*>(m.fix(sizeof(ARPHdr)));
        memset(arphdr, 0, sizeof(ARPHdr));
        arphdr->tpa = a->getAddress();

        arphdr->spa = src->getAddress();
        src->getMacAddress(arphdr->sha);

        arphdr->op = htons(ARPHdr::OP_REQUEST);

        Visitor v(&m);
        a->adapter->accept(&v);
        a->alarm(5000000 << a->timeoutCount);
    }
    else
    {
        // Not found:
        a->setState(stateInit);
    }
}

bool Inet4Address::
StateIncomplete::input(InetMessenger* m, Inet4Address* a)
{
    ARPHdr* arphdr = static_cast<ARPHdr*>(m->fix(sizeof(ARPHdr)));
    a->setMacAddress(arphdr->sha);
    a->cancel();
    a->setState(stateReachable);

    esReport("StateIncomplete::input: %02x:%02x:%02x:%02x:%02x:%02x\n",
             arphdr->sha[0], arphdr->sha[1], arphdr->sha[2],
             arphdr->sha[3], arphdr->sha[4], arphdr->sha[5]);

    return true;
}

bool Inet4Address::
StateIncomplete::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateIncomplete::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StateReachable

void Inet4Address::
StateReachable::expired(Inet4Address* a)
{
}

bool Inet4Address::
StateReachable::input(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateReachable::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateReachable::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StateProbe

void Inet4Address::
StateProbe::expired(Inet4Address* a)
{
}

bool Inet4Address::
StateProbe::input(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateProbe::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateProbe::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StateTentative

void Inet4Address::
StateTentative::start(Inet4Address* a)
{
    a->alarm(10000000);   // PROBE_WAIT
}

void Inet4Address::
StateTentative::expired(Inet4Address* a)
{
    ASSERT(a->adapter);
    if (++a->timeoutCount <= ARPHdr::PROBE_NUM)
    {
        esReport("StateTentative::timeoutCount: %d\n", a->timeoutCount);

        // Send probe
        u8 chunk[sizeof(DIXHdr) + sizeof(ARPHdr)];
        InetMessenger m(&InetReceiver::output, chunk, sizeof chunk, sizeof(DIXHdr));
        m.setScopeID(a->getScopeID());

        ARPHdr* arphdr = static_cast<ARPHdr*>(m.fix(sizeof(ARPHdr)));
        memset(arphdr, 0, sizeof(ARPHdr));
        arphdr->tpa = a->getAddress();
        arphdr->spa = InAddrAny;
        a->getMacAddress(arphdr->sha);
        arphdr->op = htons(ARPHdr::OP_REQUEST);

        Visitor v(&m);
        a->adapter->accept(&v);
        a->alarm(10000000);     // XXX PROBE_MIN to PROBE_MAX, ANNOUNCE_WAIT
    }
    else
    {
        a->setState(statePreferred);
    }
}

bool Inet4Address::
StateTentative::input(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateTentative::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateTentative::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StatePreferred

void Inet4Address::
StatePreferred::expired(Inet4Address* a)
{
    ASSERT(a->adapter);
    if (++a->timeoutCount <= ARPHdr::ANNOUNCE_NUM)
    {
        esReport("StatePreferred::timeoutCount: %d\n", a->timeoutCount);

        // Send announcement
        u8 chunk[sizeof(DIXHdr) + sizeof(ARPHdr)];
        InetMessenger m(&InetReceiver::output, chunk, sizeof chunk, sizeof(DIXHdr));
        m.setScopeID(a->getScopeID());

        ARPHdr* arphdr = static_cast<ARPHdr*>(m.fix(sizeof(ARPHdr)));
        memset(arphdr, 0, sizeof(ARPHdr));
        arphdr->tpa = a->getAddress();
        a->getMacAddress(arphdr->tha);
        arphdr->spa = a->getAddress();;
        a->getMacAddress(arphdr->sha);
        arphdr->op = htons(ARPHdr::OP_REQUEST);

        Visitor v(&m);
        a->adapter->accept(&v);
        a->alarm(20000000);     // XXX ANNOUNCE_INTERVAL
    }
}

bool Inet4Address::
StatePreferred::input(InetMessenger* m, Inet4Address* a)
{
    ARPHdr* arphdr = static_cast<ARPHdr*>(m->fix(sizeof(ARPHdr)));
    if (ntohs(arphdr->op) == ARPHdr::OP_REQUEST)
    {
        // Send reply
        u8 chunk[sizeof(DIXHdr) + sizeof(ARPHdr)];
        InetMessenger r(&InetReceiver::output, chunk, sizeof chunk, sizeof(DIXHdr));
        r.setScopeID(a->getScopeID());

        ARPHdr* rephdr = static_cast<ARPHdr*>(r.fix(sizeof(ARPHdr)));
        memset(rephdr, 0, sizeof(ARPHdr));

        rephdr->tpa = arphdr->spa;
        memmove(rephdr->tha, arphdr->sha, 6);

        rephdr->spa = a->getAddress();;
        a->getMacAddress(rephdr->sha);
        rephdr->op = htons(ARPHdr::OP_REPLY);

        Visitor v(&r);
        a->adapter->accept(&v);
    }
    return true;
}

bool Inet4Address::
StatePreferred::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StatePreferred::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// StateDeprecated

bool Inet4Address::
StateDeprecated::input(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateDeprecated::output(InetMessenger* m, Inet4Address* a)
{
    return true;
}

bool Inet4Address::
StateDeprecated::error(InetMessenger* m, Inet4Address* a)
{
    return true;
}

// ARPReceiver

bool ARPReceiver::
input(InetMessenger* m)
{
    ASSERT(m);

    // Validates the received ARP packet.
    ARPHdr* arphdr = static_cast<ARPHdr*>(m->fix(sizeof(ARPHdr)));
    if (ntohs(arphdr->hrd) != ARPHdr::HRD_ETHERNET ||
        ntohs(arphdr->pro) != ARPHdr::PRO_IP ||
        arphdr->hln != 6 ||
        arphdr->pln != 4)
    {
        return false;
    }

    m->setType(AF_ARP);
    m->setRemote(inFamily->getAddress(arphdr->spa, m->getScopeID()));

    return true;
}
