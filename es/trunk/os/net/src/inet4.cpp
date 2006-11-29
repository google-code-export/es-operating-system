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

#include <new>
#include "inet4.h"

u16 InReceiver::identification;

InFamily::InFamily() :
    scopeMux(&scopeAccessor, &scopeFactory),
    inReceiver(this),
    inMux(&inAccessor, &inFactory),
    icmpMux(&icmpAccessor, &icmpFactory),
    igmpMux(&igmpAccessor, &igmpFactory),
    echoReplyMux(&echoReplyAccessor, &echoReplyFactory),
    echoRequestReceiver(&echoRequestAdapter, 0),
    echoRequestFactory(&echoRequestAdapter),
    echoRequestMux(&echoRequestAccessor, &echoRequestFactory),
    udpRemoteAddressFactory(&datagramProtocol),
    udpRemoteAddressMux(&udpRemoteAddressAccessor, &udpRemoteAddressFactory),
    udpRemotePortFactory(&udpRemoteAddressMux),
    udpRemotePortMux(&udpRemotePortAccessor, &udpRemotePortFactory),
    udpLocalAddressFactory(&udpRemotePortMux),
    udpLocalAddressMux(&udpLocalAddressAccessor, &udpLocalAddressFactory),
    udpLocalPortFactory(&udpLocalAddressMux),
    udpLocalPortMux(&udpLocalPortAccessor, &udpLocalPortFactory),
    addressAny(InAddrAny, Inet4Address::statePreferred),
    arpFamily(this)
{
    timer = new Timer;

    inProtocol.setReceiver(&inReceiver);
    icmpProtocol.setReceiver(&icmpReceiver);
    echoRequestAdapter.setReceiver(&echoRequestReceiver);
    igmpProtocol.setReceiver(&igmpReceiver);
    udpProtocol.setReceiver(&udpReceiver);
    datagramProtocol.setReceiver(&datagramReceiver);

    Conduit::connectAA(&scopeMux, &inProtocol);
    Conduit::connectBA(&inProtocol, &inMux);
    Conduit::connectBA(&inMux, &icmpProtocol, reinterpret_cast<void*>(IPPROTO_ICMP));
    Conduit::connectBA(&inMux, &igmpProtocol, reinterpret_cast<void*>(IPPROTO_IGMP));
    Conduit::connectBA(&inMux, &udpProtocol, reinterpret_cast<void*>(IPPROTO_UDP));

    // ICMP
    Conduit::connectBA(&icmpProtocol, &icmpMux);
    Conduit::connectBA(&icmpMux, &echoReplyMux, reinterpret_cast<void*>(ICMP_ECHO_REPLY));
    Conduit::connectBA(&icmpMux, &echoRequestMux, reinterpret_cast<void*>(ICMP_ECHO_REQUEST));

    // IGMP
    Conduit::connectBA(&igmpProtocol, &igmpMux);

    // UDP
    Conduit::connectBA(&udpProtocol, &udpLocalPortMux);

    Socket::addAddressFamily(this);
}

Inet4Address* InFamily::getAddress(InAddr addr, int scopeID)
{
    ASSERT(0 <= scopeID && scopeID < Socket::INTERFACE_MAX);

    Inet4Address* address;
    try
    {
        address = addressTable[scopeID].get(addr);
    }
    catch (SystemException<ENOENT>)
    {
        if (scopeID == 0)
        {
            address = 0;
        }
        else
        {
            try
            {
                address = addressTable[0].get(addr);
            }
            catch (SystemException<ENOENT>)
            {
                address = 0;
            }
        }
    }
    if (address)
    {
        address->addRef();
    }
    return address;
}

void InFamily::addAddress(Inet4Address* address)
{
    int scopeID = address->getScopeID();
    addressTable[scopeID].add(address->getAddress(), address);
    address->addRef();
    address->inFamily = this;

    // If address is for one of the IP address assigned to this host, ...
    if (address->isLocalAddress())
    {
        // Install an ICMP echo request adapter for this address.
        InetMessenger m;
        m.setLocal(address);
        Installer installer(&m);
        echoRequestMux.accept(&installer, &icmpMux);

        if (!address->isLoopback()) // XXX we should check if the address uses ARP or not.
        {
            // Set the interface MAC address to this address.
            Interface* nic = Socket::getInterface(scopeID);
            ASSERT(nic);
            u8 mac[6];
            nic->getMacAddress(mac);
            address->setMacAddress(mac);

            // Install ARP cache for this address.
            arpFamily.addAddress(address);
            address->start();
        }
    }
    else if (1 < scopeID)   // XXX
    {
        arpFamily.addAddress(address);
    }
}

void InFamily::removeAddress(Inet4Address* address)
{
    int scopeID = address->getScopeID();
    addressTable[scopeID].remove(address->getAddress());
    address->inFamily = 0;
    address->release();
}

Inet4Address* InFamily::selectSourceAddress(Inet4Address* dst)
{
    if (dst->isLoopback())
    {
        return getAddress(InAddrLoopback, 1);
    }

    int scopeID = dst->getScopeID();
    if (scopeID == 0)
    {
        // XXX Select a router

    }

    // Look up preferred address of the same scope ID.
    Inet4Address* src = 0;
    Tree<InAddr, Inet4Address*>::Node* node;
    Tree<InAddr, Inet4Address*>::Iterator iter = addressTable[scopeID].begin();
    while ((node = iter.next()))
    {
        Inet4Address* address = node->getValue();
        if (address->isPreferred())
        {
            src = address;  // XXX Check link-local
            break;
        }
    }

    return src;
}

bool InFamily::isReachable(Inet4Address* address, long long timeout)
{
    Handle<Inet4Address> src = selectSourceAddress(address);
    if (!src)
    {
        return false;
    }

    Adapter* adapter = new Adapter;
    ICMPEchoReplyReceiver* receiver = new ICMPEchoReplyReceiver(adapter, address);
    adapter->setReceiver(receiver);
    Conduit::connectBA(&echoReplyMux, adapter, address);

    // Send ICMP Echo request to this address
    int pos = 14 + 60;      // XXX Assume MAC, IPv4
    int len = sizeof(ICMPEcho) + 10;
    u8 chunk[pos + len];
    memmove(chunk + pos + sizeof(ICMPEcho), "0123456789", 10);
    ICMPEcho* icmphdr = reinterpret_cast<ICMPEcho*>(chunk + pos);
    icmphdr->type = ICMP_ECHO_REQUEST;
    icmphdr->code = 0;
    icmphdr->id = 0;    // XXX
    icmphdr->seq = 0;   // XXX

    InetMessenger m(&InetReceiver::output, chunk, pos + len, pos);
    m.setRemote(src);
    m.setLocal(address);
    m.setType(IPPROTO_ICMP);

    Visitor v(&m);
    adapter->accept(&v);

    receiver->wait();

    echoReplyMux.removeB(address);
    delete adapter;
    delete receiver;

    return true;
}

s16 InReceiver::
checksum(const InetMessenger* m, int hlen)
{
    s32 sum = m->sumUp(hlen);
    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

bool InReceiver::
input(InetMessenger* m)
{
    Handle<Inet4Address> addr;

    IPHdr* iphdr = static_cast<IPHdr*>(m->fix(sizeof(IPHdr)));
    if (!iphdr)
    {
        return false;   // XXX
    }
    int hlen = iphdr->getHdrSize();
    if (m->getLength() < hlen || checksum(m, hlen) != 0)
    {
        return false;   // XXX
    }

    int scopeID = m->getScopeID();

    addr = inFamily->getAddress(iphdr->dst, scopeID);
    if (!addr)
    {
        return false;   // XXX
    }
    m->setLocal(addr);

    addr = inFamily->getAddress(iphdr->src, scopeID);
    if (!addr)
    {
        // XXX scopeID should be zero when forwarded by a router
        addr = new Inet4Address(iphdr->src, Inet4Address::stateInit, scopeID);
        inFamily->addAddress(addr);
    }
    m->setRemote(addr);

    return true;
}

bool InReceiver::
output(InetMessenger* m)
{
    Handle<Address> addr;

    long len = m->getLength();
    len += sizeof(IPHdr);
    if (65535 < len)
    {
        // XXX Return an error code to the upper layers.
        return false;
    }

    // Add IPHdr
    m->movePosition(-sizeof(IPHdr));
    IPHdr* iphdr = static_cast<IPHdr*>(m->fix(sizeof(IPHdr)));
    iphdr->verlen = 0x45;
    iphdr->tos = 0;
    iphdr->len = htons(len);    // octets in header and data
    iphdr->id = htons(++identification);
    iphdr->frag = 0;
    iphdr->ttl = 64;
    iphdr->proto = m->getType();
    iphdr->sum = 0;
    addr = m->getLocal();
    addr->getAddress(&iphdr->src, sizeof(InAddr));
    addr = m->getRemote();
    addr->getAddress(&iphdr->dst, sizeof(InAddr));
    iphdr->sum = checksum(m, iphdr->getHdrSize());
    m->setType(AF_INET);

    // XXX Set the remote address to the next hop router address
    //     if necessary.

    return true;
}

bool InReceiver::
error(InetMessenger* m)
{
    IPHdr* iphdr = static_cast<IPHdr*>(m->fix(sizeof(IPHdr)));

    // Reverse src and dst
    Handle<Inet4Address> addr;
    addr = inFamily->getAddress(iphdr->src, m->getScopeID());
    m->setLocal(addr);
    addr = inFamily->getAddress(iphdr->dst, m->getScopeID());
    m->setRemote(addr);

    return true;
}
