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

#ifndef ADDRESS_H_INCLUDED
#define ADDRESS_H_INCLUDED

#include <string.h>
#include <es/net/IInternetAddress.h>
#include "conduit.h"

class Address : public IInternetAddress
{
    u8  mac[6];

public:
    Address()
    {
        memset(mac, 0, sizeof mac);
    }

    virtual ~Address() {}

    void getMacAddress(u8 mac[6]) const
    {
        memmove(mac, this->mac, sizeof this->mac);
    }

    void setMacAddress(u8 mac[6])
    {
        memmove(this->mac, mac, sizeof this->mac);
    }

    virtual s32 sumUp() = 0;

    virtual void start() = 0;

    virtual Address* getNextHop() = 0;

    // void hold(Messenger* m);

};

#endif  // ADDRESS_H_INCLUDED
