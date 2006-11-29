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

#ifndef NINTENDO_ES_NET_TCP_H_INCLUDED
#define NINTENDO_ES_NET_TCP_H_INCLUDED

#include <es/endian.h>

//
// TCP RFC 793
//

typedef struct TCPHdr
{
    u16 src;    // source port
    u16 dst;    // destination port

    s32 seq;    // sequence number
    s32 ack;    // acknowledgment number

    u16 flag;   // 4:data offset, 6:reserved, 1:URG, 1:ACK, 1:PSH, 1:RST, 1:SYN, 1:FIN
    u16 win;    // window
    u16 sum;    // checksum
    u16 urg;    // urgent pointer

    // option (if any)

    // data (if any)

    int getHdrSize() const
    {
        u16 size = ntohs(flag);
        return ((size & 0xf000) >> 10);
    }

} TCPHdr;

#define TCP_MIN_HLEN        20
#define TCP_MAX_HLEN        60

#define TCP_IIS_CLOCK       (1000000 / 4)   // incremented every 4 usec following RFC793.

#define TCP_FLAG_FIN        0x0001  // No more data from sender
#define TCP_FLAG_SYN        0x0002  // Synchronize sequence numbers
#define TCP_FLAG_RST        0x0004  // Reset the connection
#define TCP_FLAG_PSH        0x0008  // Push function
#define TCP_FLAG_ACK        0x0010  // Acknowledgment field significant
#define TCP_FLAG_URG        0x0020  // Urgent pointer field significant
#define TCP_FLAG_793        0x003f

#define TCP_FLAG_ECE        0x0040  // [RFC 3168]
#define TCP_FLAG_CWR        0x0080  // [RFC 3168]
#define TCP_FLAG_3168       0x00ff

#define TCP_FLAG_DATAOFFSET 0xf000

// Option kind
#define TCP_OPT_EOL         0       // End of option list
#define TCP_OPT_NOP         1       // No-Operation.
#define TCP_OPT_MSS         2       // Maximum Segment Size (length = 4, can only apper in SYNs)

// RFC 1323 TCP Extensions for High Performance
#define TCP_OPT_WS          3       // Window scale (length = 3)
#define TCP_OPT_TS          8       // Timestamps (length = 10)

// RFC 2018 TCP Selective Acknowledgement Options
#define TCP_OPT_SACKP       4       // Sack-Permitted (length = 2)
#define TCP_OPT_SACK        5       // Sack (length = variable)

#define TCP_DEF_SSTHRESH    65535           // Default slow start threshold value
#define TCP_MSL             120             // Maximum segment lifetime in second
#define TCP_MIN_RTT         1000            // Minimum value of restransmission timer in millisecond
                                            // It must be larger than delayed ACK time + RTT.
                                            // RFC 2988 requires 1 second [SHOULD].
#define TCP_MAX_RTT         (2 * TCP_MSL)   // Maximum value of restransmission timer in second
#define TCP_DEF_RTT         3               // Default RTT in second
#define TCP_R1              3               // At least 3 retransmissions [RFC 1122]
#define TCP_PMTUD_BACKOFF   4               // Path MTU discovery blackhole detection
#define TCP_MAX_BACKOFF     16              // Maximum retransmission count allowed.
                                            // Must be more than R1, add less than 31.
#define TCP_MAX_PERSIST     (TCP_MAX_BACKOFF * TCP_MAX_RTT)
                                            // Maximum idle time in persist state
#define TCP_RXMIT_THRESH    3               // Fast restransmission threshold
#define TCP_LIMITED_THRESH  2               // Limited Transmit threshold

#define TCP_DACK_TIME       200             // Delayed ACK timer in millisecond

// a < b
#define TCP_SEQ_GT(a, b)    (0 < ((b) - (a)))

// a <= b
#define TCP_SEQ_GE(a, b)    (0 <= ((b) - (a)))

// max(a, b)
#define TCP_SEQ_MAX(a, b)   (TCP_SEQ_GT((a), (b)) ? (b) : (a))

// min(a, b)
#define TCP_SEQ_MIN(a, b)   (TCP_SEQ_GT((a), (b)) ? (a) : (b))

#endif  // NINTENDO_ES_NET_TCP_H_INCLUDED
