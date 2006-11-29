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

#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include <errno.h>
#include <es/endian.h>
#include <es/ref.h>
#include <es/base/IStream.h>
#include <es/net/ISocket.h>
#include <es/net/udp.h>
#include "inet.h"
#include "interface.h"
#include "address.h"

class AddressFamily;
class Socket;

class AddressFamily
{
    Link<AddressFamily> link;

public:
    virtual int getAddressFamily() = 0;
    virtual Conduit* getProtocol(Socket* socket) = 0;
    virtual void addInterface(Interface* interface) = 0;

    friend class Socket;
    typedef List<AddressFamily, &AddressFamily::link> List;
};

class Socket :
    public ISocket,
    public InetReceiver,
    public InetMessenger
{
public:
    static const int INTERFACE_MAX = 8;

private:
    static AddressFamily::List  addressFamilyList;
    static Interface*           interfaces[INTERFACE_MAX];

    Ref             ref;
    int             family;
    int             type;
    int             protocol;
    Adapter*        adapter;
    AddressFamily*  af;

public:
    static const int SOCK_STREAM = 1;
    static const int SOCK_DGRAM = 2;
    static const int SOCK_RAW = 3;

    static void addAddressFamily(AddressFamily* af)
    {
        addressFamilyList.addLast(af);
    }

    static void removeAddressFamily(AddressFamily* af)
    {
        addressFamilyList.remove(af);
    }

    static AddressFamily* getAddressFamily(int key)
    {
        AddressFamily* af;
        AddressFamily::List::Iterator iter = addressFamilyList.begin();
        while ((af = iter.next()))
        {
            if (af->getAddressFamily() == key)
            {
                return af;
            }
        }
        return 0;
    }

    static int addInterface(IStream* stream, int hrd);
    static void removeInterface(IStream* stream);
    static Interface* getInterface(int scopeID)
    {
        if (scopeID < 0 || INTERFACE_MAX <= scopeID)
        {
            return 0;
        }
        return interfaces[scopeID];
    }

    Socket(int family, int type, int protocol = 0);
    ~Socket();

    bool input(InetMessenger* m);
    bool output(InetMessenger* m);
    bool error(InetMessenger* m);

    Adapter* getAdapter() const
    {
        return adapter;
    }
    void setAdapter(Adapter* adapter)
    {
        this->adapter = adapter;
    }

    //
    // ISocket
    //

    int getAddressFamily()
    {
        return family;
    }
    IInternetAddress* getLocalAddress()
    {
        return getLocal();
    }
    int getLocalPort()
    {
        return InetMessenger::getLocalPort();
    }
    int getProtocolType()
    {
        return protocol;
    }
    IInternetAddress* getRemoteAddress()
    {
        return getRemote();
    }
    int getRemotePort()
    {
        return InetMessenger::getRemotePort();
    }
    int getSocketType()
    {
        return type;
    }

    ISocket* accept();
    void bind(IInternetAddress* addr, int port);
    void close();
    void connect(IInternetAddress* addr, int port);
    void listen(int backlog);
    int read(void* dst, int count);
    int recvFrom(void* dst, int count, int flags, IInternetAddress** addr, int* port);
    int sendTo(const void* src, int count, int flags, IInternetAddress* addr, int port);
    void shutdownInput();
    void shutdownOutput();
    int write(const void* src, int count);

    //
    // IInterface
    //
    bool queryInterface(const Guid& riid, void** objectPtr);
    unsigned int addRef();
    unsigned int release();
};

class SocketBinder : public Visitor
{
    Socket* socket;
    int     code;

public:
    SocketBinder(Socket* s) :
        Visitor(s),
        socket(s),
        code(0)
    {
    }
    bool at(Protocol* p, Conduit* c)
    {
        if (p->getB())
        {
            code = EADDRINUSE;
            return true;
        }
        Adapter* adapter = new Adapter;
        adapter->setReceiver(socket);
        socket->setAdapter(adapter);
        Conduit::connectAB(adapter, p);
        return true;
    }
    bool at(ConduitFactory* f, Conduit* c)
    {
        Mux* mux = dynamic_cast<Mux*>(c);
        if (mux)
        {
            ASSERT(mux->getFactory() == f);
            void* key = mux->getKey(getMessenger());
            Conduit* e = f->create(key);
            if (e)
            {
                Conduit::connectAB(e, mux, key);
                return false;
            }
        }
        return true;
    }
    int getErrorCode() const
    {
        return code;
    }
};

class SocketDisconnector : public Visitor
{
    Socket*     socket;
    Conduit*    protocol;

public:
    SocketDisconnector(Socket* s) :
        Visitor(s),
        socket(s),
        protocol(0)
    {
    }
    bool at(Mux* mux, Conduit* c)
    {
        ASSERT(c->getA() == mux);
        c->setA(0);
        mux->removeB(mux->getKey(getMessenger()));
        protocol = c;
        return false;   // To stop this visitor
    }
    Conduit* getProtocol() const
    {
        return protocol;
    }
};

class SocketConnector : public Visitor
{
    Socket*     socket;
    int         code;
    Conduit*    protocol;

public:
    SocketConnector(Socket* s, Conduit* p) :
        Visitor(s),
        socket(s),
        code(0),
        protocol(p)
    {
    }
    bool at(Adapter* a, Conduit* c)
    {
        if (a->getReceiver() != socket)
        {
            code = EADDRINUSE;
        }
        return true;
    }
    bool at(ConduitFactory* f, Conduit* c)
    {
        Mux* mux = dynamic_cast<Mux*>(c);
        if (mux)
        {
            ASSERT(mux->getFactory() == f);
            void* key = mux->getKey(getMessenger());
            if (dynamic_cast<InetRemoteAddressAccessor*>(mux->getAccessor()))
            {
                Conduit::connectAB(protocol, mux, key);
                return false;
            }
            else
            {
                Conduit* e = f->create(key);
                if (e)
                {
                    Conduit::connectAB(e, mux, key);
                    return false;
                }
            }
        }
        return true;
    }
    int getErrorCode() const
    {
        return code;
    }
};

class SocketMessenger;
class SocketReceiver : public InetReceiver
{
public:
    virtual bool read(SocketMessenger* m)
    {
        return true;
    }
    virtual bool write(SocketMessenger* m)
    {
        return true;
    }

    typedef bool (SocketReceiver::*Command)(SocketMessenger*);
};

class SocketMessenger : public InetMessenger
{
    SocketReceiver::Command op;

public:
    SocketMessenger(SocketReceiver::Command op,
                    void* chunk = 0, long len = 0, long pos = 0) :
        InetMessenger(0, chunk, len, pos),
        op(op)
    {
    }

    virtual bool apply(Conduit* c)
    {
        if (op)
        {
            SocketReceiver* receiver = dynamic_cast<SocketReceiver*>(c->getReceiver());
            if (receiver)
            {
                return (receiver->*op)(this);
            }
        }
        return Messenger::apply(c);
    }
};

#endif  // SOCKET_H_INCLUDED
