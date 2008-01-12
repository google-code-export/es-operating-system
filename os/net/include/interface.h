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

#ifndef INTERFACE_H_INCLUDED
#define INTERFACE_H_INCLUDED

#include <es/handle.h>
#include <es/base/IStream.h>
#include <es/base/IThread.h>
#include <es/device/INetworkInterface.h>
#include "inet.h"
#include "interface.h"
#include "address.h"

class AddressFamily;

IThread* esCreateThread(void* (*start)(void* param), void* param);

/** This class represents a network interface like Ethernet interface,
 *  loopback interface, etc.
 */
class Interface
{
    static const int MRU = 1518;

    Handle<INetworkInterface>   networkInterface;

    IThread*        thread;

    u8              mac[6];             // MAC address

    Accessor*       accessor;
    Receiver*       receiver;

    ConduitFactory  factory;
    Adapter         adapter;
    int             scopeID;

    void* vent()
    {
        Handle<InetMessenger> m = new InetMessenger(&InetReceiver::input, MRU);
        Handle<IStream> stream = networkInterface;
        for (;;)
        {
            int len = stream->read(m->fix(MRU), MRU);
            if (0 < len)
            {
                esReport("# input\n");
                esDump(m->fix(len), len);

                m->setSize(len);
                m->setScopeID(scopeID);
                Transporter v(m);
                adapter.accept(&v);
                m->setSize(MRU);    // Restore the size
                m->setPosition(0);
            }
        }
        return 0;
    }

    static void* run(void* param)
    {
        Interface* interface = static_cast<Interface*>(param);
        return interface->vent();
    }

protected:
    Mux             mux;

public:
    Interface(INetworkInterface* networkInterface, Accessor* accessor, Receiver* receiver) :
        networkInterface(networkInterface, true),
        accessor(accessor),
        receiver(receiver),
        mux(accessor, &factory),
        scopeID(0),
        thread(0)
    {
        memset(mac, 0, sizeof mac);

        adapter.setReceiver(receiver);
        Conduit::connectAA(&adapter, &mux);
    }

    INetworkInterface* getNetworkInterface()
    {
        return networkInterface;    // XXX Check reference count
    }

    Adapter* getAdapter()
    {
        return &adapter;
    }

    int getScopeID()
    {
        return scopeID;
    }
    void setScopeID(int id)
    {
        scopeID = id;
    }

    void getMacAddress(u8 mac[6]) const
    {
        memmove(mac, this->mac, sizeof this->mac);
    }

    void setMacAddress(u8 mac[6])
    {
        memmove(this->mac, mac, sizeof this->mac);
    }

    void addMulticastAddress(u8 mac[6])
    {
        networkInterface->addMulticastAddress(mac);
    }

    void removeMulticastAddress(u8 mac[6])
    {
        networkInterface->removeMulticastAddress(mac);
    }

    /** Run a thread that reads from the stream and creates an input
     * messenger to be accepted by the interface adapter.
     */
    void start()
    {
        thread = esCreateThread(run, this);
        thread->setPriority(IThread::Highest);
        thread->start();
    }

    virtual Conduit* addAddressFamily(AddressFamily* af, Conduit* c) = 0;

    friend class Socket;
};

#endif  // INTERFACE_H_INCLUDED
