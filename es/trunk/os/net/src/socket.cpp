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
#include <string.h>
#include <es/net/arp.h>
#include "dix.h"
#include "inet4address.h"
#include "loopback.h"
#include "socket.h"
#include "visualizer.h"

AddressFamily::List Socket::addressFamilyList;
Interface*          Socket::interfaces[Socket::INTERFACE_MAX];

LoopbackAccessor    LoopbackInterface::loopbackAccessor;

int Socket::addInterface(IStream* stream, int hrd)
{
    int n;
    for (n = 1; n < Socket::INTERFACE_MAX; ++n)
    {
        if (interfaces[n] == 0)
        {
            break;
        }
    }
    if (Socket::INTERFACE_MAX <= n)
    {
        return -1;
    }

    switch (hrd)
    {
      case ARPHdr::HRD_ETHERNET:
      {
        DIXInterface* dixInterface = new DIXInterface(stream);
        dixInterface->setScopeID(n);
        interfaces[n] = dixInterface;

        // Connect address families to the loopback interface.
        AddressFamily* af;
        AddressFamily::List::Iterator iter = addressFamilyList.begin();
        while ((af = iter.next()))
        {
            af->addInterface(dixInterface);
        }

        {
            Visualizer v;
            dixInterface->getAdapter()->accept(&v);
        }

        dixInterface->start();
        break;
      }
      case ARPHdr::HRD_LOOPBACK:
      {
        LoopbackInterface* loopbackInterface = new LoopbackInterface(stream);
        loopbackInterface->setScopeID(n);
        interfaces[n] = loopbackInterface;

        // Connect address families to the loopback interface.
        AddressFamily* af;
        AddressFamily::List::Iterator iter = addressFamilyList.begin();
        while ((af = iter.next()))
        {
            af->addInterface(loopbackInterface);
        }

        {
            Visualizer v;
            loopbackInterface->getAdapter()->accept(&v);
        }

        loopbackInterface->start();
        break;
      }
      default:
        return -1;
    }
    return n;
}

void Socket::removeInterface(IStream* stream)
{
    int n;
    for (n = 1; n < Socket::INTERFACE_MAX; ++n)
    {
        Interface* i = interfaces[n];
        if (i && i->stream == stream)
        {
            interfaces[0] = 0;
            delete i;
            break;
        }
    }
}

Socket::
Socket(int family, int type, int protocol) :
    InetMessenger(0, chunk, sizeof chunk, 0),
    family(family),
    type(type),
    protocol(protocol),
    adapter(0)
{
    af = getAddressFamily(family);
}

Socket::
~Socket()
{
}

bool Socket::
input(InetMessenger* m)
{
    return true;
}

bool Socket::
output(InetMessenger* m)
{
    return true;
}

bool Socket::
error(InetMessenger* m)
{
    return true;
}

//
// ISocket
//

void Socket::
bind(IInternetAddress* addr, int port)
{
    Conduit* protocol = af->getProtocol(this);
    if (!protocol)
    {
        return;
    }

    setLocal(dynamic_cast<Address*>(addr));   // XXX
    setLocalPort(port);

    SocketBinder binder(this);
    protocol->accept(&binder);
}

void Socket::
connect(IInternetAddress* addr, int port)
{
    Conduit* protocol = af->getProtocol(this);
    if (!protocol)
    {
        return;
    }

    SocketDisconnector disconnector(this);
    Adapter* adapter = getAdapter();
    adapter->accept(&disconnector);

    setRemote(dynamic_cast<Address*>(addr));   // XXX
    setRemotePort(port);

    SocketConnector connector(this, disconnector.getProtocol());
    protocol->accept(&connector);
}

ISocket* Socket::
accept()
{
}

void Socket::
close()
{
}

void Socket::
listen(int backlog)
{
}

int Socket::
read(void* dst, int count)
{
    SocketMessenger m(&SocketReceiver::read, dst, count);

    Visitor v(&m);
    adapter->accept(&v);
    return m.getLength();
}

int Socket::
recvFrom(void* dst, int count, int flags, IInternetAddress** addr, int* port)
{
}

int Socket::
sendTo(const void* src, int count, int flags, IInternetAddress* addr, int port)
{
}

void Socket::
shutdownInput()
{
}

void Socket::
shutdownOutput()
{
}

int Socket::
write(const void* src, int count)
{
    int pos = 14 + 60 + 60; // XXX Assume MAC, IPv4, TCP
    u8 chunk[pos + count];

    memmove(chunk + pos, src, count);
    InetMessenger m(&InetReceiver::output, chunk, pos + count, pos);

    m.setLocal(getLocal());
    m.setRemote(getRemote());
    m.setLocalPort(getLocalPort());
    m.setRemotePort(getRemotePort());

    Visitor v(&m);
    adapter->accept(&v);
}

//
// IInterface
//

bool Socket::
queryInterface(const Guid& riid, void** objectPtr)
{
    if (riid == IID_ISocket)
    {
        *objectPtr = static_cast<ISocket*>(this);
    }
    else if (riid == IID_IInterface)
    {
        *objectPtr = static_cast<ISocket*>(this);
    }
    else
    {
        *objectPtr = NULL;
        return false;
    }
    static_cast<IInterface*>(*objectPtr)->addRef();
    return true;
}

unsigned int Socket::
addRef(void)
{
    return ref.addRef();
}

unsigned int Socket::
release(void)
{
    unsigned int count = ref.release();
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return count;
}
