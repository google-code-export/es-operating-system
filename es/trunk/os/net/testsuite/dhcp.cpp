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

#include <string.h>
#include <es.h>
#include <es/dateTime.h>
#include <es/endian.h>
#include <es/handle.h>
#include <es/list.h>
#include <es/base/IStream.h>
#include <es/base/IThread.h>
#include <es/device/IEthernet.h>
#include <es/naming/IContext.h>
#include <es/net/ISocket.h>
#include <es/net/IInternetConfig.h>
#include <es/net/IResolver.h>
#include <es/net/arp.h>
#include <es/net/dhcp.h>
#include <es/net/udp.h>

const InAddr DHCPHdr::magicCookie = { htonl(99 << 24 | 130 << 16 | 83 << 8 | 99) };

extern int esInit(IInterface** nameSpace);
extern IThread* esCreateThread(void* (*start)(void* param), void* param);
extern void esRegisterInternetProtocol(IContext* context);

static const int MaxDomainName = 256;
static const int MaxStaticRoute = 1;

struct DHCPStaticRoute
{
    InAddr  dest;
    InAddr  router;
};

struct DHCPInfo
{
    InAddr  ipaddr;                 // dhcp->yiaddr

    // BOOTP options
    InAddr  netmask;                // DHCPOption::SubnetMask
    InAddr  router;                 // DHCPOption::Router (1st)
    InAddr  dns1;                   // DHCPOption::DomainNameServer (1st)
    InAddr  dns2;                   // DHCPOption::DomainNameServer (2nd)
    char    host[MaxDomainName];    // DHCPOption::HostName
    char    domain[MaxDomainName];  // DHCPOption::DomainName
    u8      ttl;                    // DHCPOption::DefaultTTL
    u16     mtu;                    // DHCPOption::InterfaceMTU
    InAddr  broadcast;              // DHCPOption::BroadcastAddress

    // DHCP Extensions
    u32     lease;                  // DHCPOption::LeaseTime [sec]
    InAddr  server;                 // DHCPOption::ServerID
    u32     renewal;                // DHCPOption::RenewalTime [sec] (t1)
    u32     rebinding;              // DHCPOption::RebindingTime [sec] (t2)

    DHCPStaticRoute staticRoute[MaxStaticRoute];    // DHCPOption::StaticRoute
};

class DHCPControl
{
    static const int MinWait = 4;   // [sec] -1 to +1
    static const int MaxWait = 64;  // [sec]
    static const int MaxRxmit = 4;

    Handle<IResolver>           resolver;
    Handle<IInternetConfig>     config;
    int                         scopeID;
    u8                          chaddr[16]; // Client hardware address

    Handle<ISocket>             socket;
    Handle<IInternetAddress>    host;
    Handle<IInternetAddress>    server;
    Handle<IInternetAddress>    limited;

    volatile bool               enabled;

    int         state;
    u32         xid;
    u32         flag;

    u8          heap[IP_MIN_MTU - sizeof(IPHdr) - sizeof(UDPHdr)];

    int         rxmitMax;
    int         rxmitCount;

    DateTime    epoch;

    DHCPInfo    info;
    DHCPInfo    temp;

    char        hostName[MaxDomainName];

    u8* requestList(u8* vend)
    {
        u8* ptr = vend;

        // Request list
        *ptr++ = DHCPOption::RequestList;
        *ptr++ = 2; // length
        *ptr++ = DHCPOption::SubnetMask;
        *ptr++ = DHCPOption::Router;
        *ptr++ = DHCPOption::DomainNameServer;
        *ptr++ = DHCPOption::HostName;
        *ptr++ = DHCPOption::DomainName;
        *ptr++ = DHCPOption::BroadcastAddress;
        *ptr++ = DHCPOption::StaticRoute;
        *ptr++ = DHCPOption::RenewalTime;
        *ptr++ = DHCPOption::RebindingTime;
        vend[1] = (u8) (ptr - vend - 2);
        return ptr;
    }

    int discover()
    {
        DHCPHdr* dhcp = reinterpret_cast<DHCPHdr*>(heap);
        dhcp->op = DHCPHdr::Request;
        dhcp->htype = 1;        // Ethernet
        dhcp->hlen = 6;         // Ethernet
        dhcp->hops = 0;
        dhcp->xid = htonl(xid);
        dhcp->secs = 0;
        dhcp->flags = 0;
        dhcp->ciaddr = dhcp->yiaddr = dhcp->siaddr = dhcp->giaddr = InAddrAny;
        memmove(dhcp->chaddr, chaddr, sizeof chaddr);
        memset(dhcp->sname, 0, 64);
        memset(dhcp->file,  0, 128);

        dhcp->cookie = DHCPHdr::magicCookie;

        u8* vend = heap + sizeof(DHCPHdr);

        *vend++ = DHCPOption::Type;
        *vend++ = 1;
        *vend++ = DHCPType::Discover;

        vend = requestList(vend);

        *vend++ = DHCPOption::End;
        int len = vend - heap;
        if (len < DHCPHdr::BootpHdrSize)
        {
            memset(vend, DHCPOption::Pad, DHCPHdr::BootpHdrSize - len);
            len = DHCPHdr::BootpHdrSize;
        }

        return len;
    }

    int request()
    {
        DHCPHdr* dhcp = reinterpret_cast<DHCPHdr*>(heap);
        dhcp->op = DHCPHdr::Request;
        dhcp->htype = 1;        // Ethernet
        dhcp->hlen = 6;         // Ethernet
        dhcp->hops = 0;
        dhcp->xid = htonl(xid);
        dhcp->secs = 0;
        dhcp->flags = 0;
        if (state == DHCPState::Renewing || state == DHCPState::Rebinding)
        {
            dhcp->ciaddr = temp.ipaddr;
        }
        else
        {
            dhcp->ciaddr = InAddrAny;
        }
        dhcp->yiaddr = dhcp->siaddr = dhcp->giaddr = InAddrAny;
        memmove(dhcp->chaddr, chaddr, sizeof chaddr);
        memset(dhcp->sname, 0, 64);
        memset(dhcp->file,  0, 128);

        dhcp->cookie = DHCPHdr::magicCookie;

        u8* vend = heap + sizeof(DHCPHdr);

        *vend++ = DHCPOption::Type;
        *vend++ = 1;
        *vend++ = DHCPType::Request;

        vend = requestList(vend);

        // Server Identifier
        if (state == DHCPState::Requesting)
        {
            *vend++ = DHCPOption::ServerID;
            *vend++ = sizeof(InAddr);
            memmove(vend, &temp.server, sizeof(InAddr));
            vend += sizeof(InAddr);
        }

        // Requested IP Address
        if (state == DHCPState::Requesting || state == DHCPState::Rebooting)
        {
            *vend++ = DHCPOption::RequestedAddress;
            *vend++ = sizeof(InAddr);
            memmove(vend, &temp.ipaddr, sizeof(InAddr));
            vend += sizeof(InAddr);
        }

        *vend++ = DHCPOption::End;
        int len = vend - heap;
        if (len < DHCPHdr::BootpHdrSize)
        {
            memset(vend, DHCPOption::Pad, DHCPHdr::BootpHdrSize - len);
            len = DHCPHdr::BootpHdrSize;
        }

        return len;
    }

    int release()
    {
        DHCPHdr* dhcp = reinterpret_cast<DHCPHdr*>(heap);
        dhcp->op = DHCPHdr::Request;
        dhcp->htype = 1;        // Ethernet
        dhcp->hlen = 6;         // Ethernet
        dhcp->hops = 0;
        dhcp->xid = htonl(xid);
        dhcp->secs = 0;
        dhcp->flags = 0;
        dhcp->ciaddr = temp.ipaddr;
        dhcp->yiaddr = dhcp->siaddr = dhcp->giaddr = InAddrAny;
        memmove(dhcp->chaddr, chaddr, sizeof chaddr);
        memset(dhcp->sname, 0, 64);
        memset(dhcp->file,  0, 128);

        dhcp->cookie = DHCPHdr::magicCookie;

        u8* vend = heap + sizeof(DHCPHdr);

        *vend++ = DHCPOption::Type;
        *vend++ = 1;
        *vend++ = DHCPType::Release;

        // Server Identifier
        *vend++ = DHCPOption::ServerID;
        *vend++ = sizeof(InAddr);
        memmove(vend, &temp.server, sizeof(InAddr));
        vend += sizeof(InAddr);

        *vend++ = DHCPOption::End;
        int len = vend - heap;
        if (len < DHCPHdr::BootpHdrSize)
        {
            memset(vend, DHCPOption::Pad, DHCPHdr::BootpHdrSize - len);
            len = DHCPHdr::BootpHdrSize;
        }

        return len;
    }

    u8 reply(int len)
    {
        if (len < sizeof(DHCPHdr))
        {
            return 0;
        }
        DHCPHdr* dhcp = reinterpret_cast<DHCPHdr*>(heap);
        if (dhcp->op != DHCPHdr::Reply ||
            dhcp->htype != 1 ||
            dhcp->hlen != 6 ||
            dhcp->xid != htonl(xid) ||
            dhcp->cookie != DHCPHdr::magicCookie)
        {
            return 0;
        }
        if (sizeof heap < len)
        {
            len = sizeof heap;
        }

        u8 messageType = 0;
        DHCPInfo* info = &temp;
        memset(info, 0, sizeof(DHCPInfo));

        bool overloaded = false;
        u8* sname = 0;
        u8* file  = 0;
        u8  ttl;
        u16 mtu;

        u8* vend = heap + sizeof(DHCPHdr);
        int optlen = len - sizeof(sizeof(DHCPHdr));

    Overload:
        while (0 < optlen && *vend != DHCPOption::End)
        {
            u8 type = *vend++;
            --optlen;
            switch (type)
            {
            case DHCPOption::Pad:
                len = 1;
                break;
            default:
                if (optlen < 1)
                {
                    return 0;
                }
                len = *vend++;
                --optlen;
                if (optlen < len)
                {
                    return 0;
                }
                break;
            }

            switch (type)
            {
            //
            // BOOTP options
            //
            case DHCPOption::SubnetMask:
                if (len != sizeof(InAddr))
                {
                    return 0;
                }
                info->netmask = *reinterpret_cast<InAddr*>(vend);
                break;
            case DHCPOption::Router:
                if (len <= 0 || len % sizeof(InAddr))
                {
                    return 0;
                }
                // Choose the 1st one
                info->router = *reinterpret_cast<InAddr*>(vend);
                break;
            case DHCPOption::DomainNameServer:
                if (len <= 0 || len % sizeof(InAddr))
                {
                    return 0;
                }
                // Choose the 1st two
                info->dns1 = *reinterpret_cast<InAddr*>(vend);
                if (8 <= len)
                {
                    info->dns2 = *reinterpret_cast<InAddr*>(vend + sizeof(InAddr));
                }
                break;
            case DHCPOption::HostName:
                memmove(info->host, vend, len);
                info->host[len] = '\0';
                break;
            case DHCPOption::DomainName:
                memmove(info->domain, vend, len);
                info->domain[len] = '\0';
                break;
            case DHCPOption::DefaultTTL:
                ttl = *(u8*) vend;
                if (0 < ttl)
                {
                    info->ttl = ttl;
                }
                break;
            case DHCPOption::InterfaceMTU:
                mtu = ntohs(*(u16*) vend);
                if (68 <= mtu)
                {
                    info->mtu = mtu;
                }
                break;

            case DHCPOption::BroadcastAddress:
                if (len != sizeof(InAddr))
                {
                    return 0;
                }
                info->broadcast = *reinterpret_cast<InAddr*>(vend);
                break;

            case DHCPOption::StaticRoute:
                if (len <= 0 || len % sizeof(DHCPStaticRoute))
                {
                    return 0;
                }
                memmove(info->staticRoute, vend, std::min((int) (MaxStaticRoute * sizeof(DHCPStaticRoute)), len));
                break;

            //
            // DHCP Extensions
            //
            case DHCPOption::LeaseTime:
                if (len != 4)
                {
                    return 0;
                }
                info->lease = ntohl(*(u32*) vend);
                break;
            case DHCPOption::Overload:
                if (len != 1 || overloaded)
                {
                    return 0;
                }
                overloaded = true;
                switch (*vend)
                {
                  case DHCPHdr::File:
                    file = dhcp->file;
                    break;
                  case DHCPHdr::Sname:
                    sname = dhcp->sname;
                    break;
                  case DHCPHdr::FileAndSname:
                    file = dhcp->file;
                    sname = dhcp->sname;
                    break;
                }
                break;
            case DHCPOption::Type:
                if (len != 1)
                {
                    return 0;
                }
                messageType = *vend;
                break;
            case DHCPOption::ServerID:
                if (len != sizeof(InAddr))
                {
                    return 0;
                }
                info->server = *reinterpret_cast<InAddr*>(vend);
                break;
            case DHCPOption::RenewalTime:
                if (len != 4)
                {
                    return 0;
                }
                info->renewal = ntohl(*(u32*) vend);
                break;
            case DHCPOption::RebindingTime:
                if (len != 4)
                {
                    return 0;
                }
                info->rebinding = ntohl(*(u32*) vend);
                break;
            default:
                break;
            }

            vend += len;
            optlen -= len;
        }

        if (sname)
        {
            vend = sname;
            optlen = sizeof(dhcp->sname);
            sname = 0;
            goto Overload;
        }

        if (file)
        {
            vend = file;
            optlen = sizeof(dhcp->file);
            file = 0;
            goto Overload;
        }

        switch (messageType)
        {
        case DHCPType::Offer:
        case DHCPType::Ack:
            // Check dhcp->yiaddr
            if (IN_IS_ADDR_MULTICAST(dhcp->yiaddr) ||
                IN_IS_ADDR_RESERVED(dhcp->yiaddr) ||
                IN_ARE_ADDR_EQUAL(dhcp->yiaddr, InAddrAny) ||
                IN_ARE_ADDR_EQUAL(dhcp->yiaddr, InAddrLoopback) ||
                IN_ARE_ADDR_EQUAL(dhcp->yiaddr, InAddrBroadcast))
            {
                return 0;
            }
            break;
        }

        info->ipaddr = dhcp->yiaddr;

        if (info->renewal == 0 || info->lease <= info->renewal)
        {
            // T1 defaults to (0.5 * duration_of_lease)
            info->renewal = info->lease / 2;
        }

        if (info->rebinding == 0 || info->lease <= info->rebinding)
        {
            // T2 defaults to (0.875 * duration_of_lease)
            info->rebinding = (u32) ((7 * (u64) info->lease) / 8);
        }
        if (info->rebinding <= info->renewal)
        {
            info->renewal = (u32) ((4 * (u64) info->rebinding) / 7);
        }

        return messageType;
    }

    bool sleep(DateTime till)
    {
        DateTime now = DateTime::getNow();
        while (enabled && now < till)
        {
            socket->setTimeout(till - now);
            socket->read(heap, sizeof heap);
            now = DateTime::getNow();
        }
        return enabled;
    }

public:
    DHCPControl(IContext* context, int scopeID, u8 chaddr[16]) :
        scopeID(scopeID),
        enabled(false),
        state(DHCPState::Init),
        xid(0),
        flag(0),
        rxmitMax(MaxRxmit),
        rxmitCount(0)
    {
        memmove(this->chaddr, chaddr, sizeof this->chaddr);

        resolver = context->lookup("network/resolver");
        config = context->lookup("network/config");

        limited = resolver->getHostByAddress(&InAddrBroadcast.addr, sizeof(InAddr), scopeID);
    }

    ~DHCPControl()
    {
        socket = 0;
    }

    void run()
    {
        enabled = true;

        xid = (u32) DateTime::getNow().getTicks();
        xid ^= *(u32*) chaddr;

        Handle<IInternetAddress> any = resolver->getHostByAddress(&InAddrAny.addr, sizeof(InAddr), scopeID);
        socket = any->socket(AF_INET, ISocket::Datagram, DHCPHdr::ClientPort);
        u8 type = 0;

        // Send DHCPDISCOVER
        state = DHCPState::Selecting;
        for (rxmitCount = 0; enabled && rxmitCount < rxmitMax; ++rxmitCount)
        {
            int len = discover();
            socket->sendTo(heap, len, 0, limited, DHCPHdr::ServerPort);
            socket->setTimeout(TimeSpan(0, 0, MinWait << rxmitCount));
            len = socket->read(heap, sizeof heap);
            type = reply(len);
            if (type == DHCPType::Offer)
            {
                break;
            }
        }

        if (type != DHCPType::Offer)
        {
            esReport("DHCP: failed.\n");
            state = DHCPState::Init;
            socket->close();
            return;
        }

        // Send DHCPREQUEST
        state = DHCPState::Requesting;
        DateTime now = DateTime::getNow();
        for (rxmitCount = 0; enabled && rxmitCount < rxmitMax; ++rxmitCount)
        {
            int len = request();
            socket->sendTo(heap, len, 0, limited, DHCPHdr::ServerPort);
            socket->setTimeout(TimeSpan(0, 0, MinWait << rxmitCount));
            do
            {
                len = socket->read(heap, sizeof heap);
                type = reply(len);
            } while (type == DHCPType::Offer);
            if (type == DHCPType::Ack || type == DHCPType::Nak)
            {
                break;
            }
        }

        if (type != DHCPType::Ack || temp.lease == 0)
        {
            esReport("DHCP: failed.\n");
            state = DHCPState::Init;
            socket->close();
            return;
        }

        state = DHCPState::Bound;

        unsigned int prefix = 32 - (ffs(ntohl(temp.netmask.addr)) - 1);

        esReport("lease %u\n", temp.lease);
        esReport("renewal %u\n", temp.renewal);
        esReport("rebinding: %u\n", temp.rebinding);
        esReport("prefix: %u\n", prefix);

        // Register host address (temp.ipaddr)
        host = resolver->getHostByAddress(&temp.ipaddr.addr, sizeof(InAddr), scopeID);
        config->addAddress(host, prefix);

        if (!sleep(DateTime::getNow() + 90000000))  // Wait for the host address to be settled.
        {
            config->removeAddress(host);
            socket->close();
            return;
        }
        // XXX Send decline if configuration was failed.

        // Register a default router (temp.router)
        Handle<IInternetAddress> router;
        if (!IN_IS_ADDR_UNSPECIFIED(temp.router))
        {
            router = resolver->getHostByAddress(&temp.router.addr, sizeof(InAddr), scopeID);
            config->addRouter(router);
        }

        socket->close();
        socket = host->socket(AF_INET, ISocket::Datagram, DHCPHdr::ClientPort);
        server = resolver->getHostByAddress(&temp.server, sizeof(InAddr), scopeID);

        while (enabled)
        {
            state = DHCPState::Bound;
            epoch = now;

            // Wait for T1 to move to RENEWING state.
            if (!sleep(epoch + TimeSpan(0, 0, temp.renewal)))
            {
                continue;
            }

            state = DHCPState::Renewing;
            type = 0;
            while (enabled && (now = DateTime::getNow()) < epoch + TimeSpan(0, 0, temp.rebinding))
            {
                int len = request();
                socket->sendTo(heap, len, 0, server, DHCPHdr::ServerPort);
                TimeSpan wait = (epoch + TimeSpan(0, 0, temp.rebinding) - now) / 2;
                if (wait < TimeSpan(0, 0, 60))
                {
                    break;
                }
                socket->setTimeout(wait);
                len = socket->read(heap, sizeof heap);
                type = reply(len);
                if (type == DHCPType::Ack || type == DHCPType::Nak)
                {
                    break;
                }
            }

            if (type == DHCPType::Nak)
            {
                esReport("DHCP: failed renewing.\n");
                state = DHCPState::Init;
                break;
            }

            if (type == DHCPType::Ack)
            {
                continue;
            }

            // Wait for T2 to move to REBINDING state.
            if (!sleep(epoch + TimeSpan(0, 0, temp.rebinding)))
            {
                continue;
            }

            state = DHCPState::Rebinding;
            type = 0;
            while (enabled && (now = DateTime::getNow()) < epoch + TimeSpan(0, 0, temp.rebinding))
            {
                int len = request();
                socket->sendTo(heap, len, 0, limited, DHCPHdr::ServerPort);
                TimeSpan wait = (epoch + TimeSpan(0, 0, temp.lease) - now) / 2;
                if (wait < TimeSpan(0, 0, 60))
                {
                    break;
                }
                socket->setTimeout(wait);
                len = socket->read(heap, sizeof heap);
                type = reply(len);
                if (type == DHCPType::Ack || type == DHCPType::Nak)
                {
                    break;
                }
            }

            if (type == DHCPType::Nak)
            {
                esReport("DHCP: failed renewing.\n");
                state = DHCPState::Init;
                break;
            }

            if (type != DHCPType::Ack)
            {
                esReport("DHCP: failed rebinding.\n");
                break;
            }
        }

        if (state == DHCPState::Bound ||
            state == DHCPState::Renewing ||
            state == DHCPState::Rebinding)
        {
            int len = release();
            socket->sendTo(heap, len, 0, server, DHCPHdr::ServerPort);
            sleep(DateTime::getNow() + 10000000);
        }

        socket->close();
        socket = 0;

        config->removeAddress(host);
        if (router)
        {
            config->removeRouter(router);
        }
        state = DHCPState::Init;

        host = 0;
        server = 0;
        router = 0;
    }

    void stop()
    {
        if (enabled)
        {
            enabled = false;
            socket->notify();
            esSleep(100000000);
        }
    }
};

static void* dhcpLoop(void* param)
{
    // Test listen and accept operations
    DHCPControl* dhcp = static_cast<DHCPControl*>(param);
    dhcp->run();
    return 0;
}

int main()
{
    IInterface* root = NULL;
    esInit(&root);
    Handle<IContext> context(root);

    esRegisterInternetProtocol(context);

    // Lookup resolver object
    Handle<IResolver> resolver = context->lookup("network/resolver");

    // Lookup internet config object
    Handle<IInternetConfig> config = context->lookup("network/config");

    // Setup DIX interface
    Handle<IStream> ethernetStream = context->lookup("device/ethernet");
    Handle<IEthernet> nic(ethernetStream);
    nic->start();
    int dixID = config->addInterface(ethernetStream, ARPHdr::HRD_ETHERNET);

    u8 chaddr[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    nic->getMacAddress(chaddr);
    DHCPControl* dhcp = new DHCPControl(context, dixID, chaddr);

    Handle<IThread> thread = esCreateThread(dhcpLoop, dhcp);
    thread->start();
    esSleep(120000000);

    // Test remote ping (192.195.204.26 / www.nintendo.com)
    InAddr addrRemote = { htonl(192 << 24 | 195 << 16 | 204 << 8 | 26) };
    Handle<IInternetAddress> remote = resolver->getHostByAddress(&addrRemote.addr, sizeof addrRemote, 0);
    esReport("ping #1\n");
    remote->isReachable(10000000);
    esReport("ping #2\n");
    remote->isReachable(10000000);
    esReport("ping #3\n");
    remote->isReachable(10000000);

    dhcp->stop();
    delete dhcp;

    nic->stop();

    esReport("done.\n");
}
