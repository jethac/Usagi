/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "NetPlatform_ps.h"
#include "Engine/Network/NetPlatform.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>

namespace usg {

void net_startup()
{
	// BSD sockets need no global initialisation
}

void net_shutdown()
{
}

// A stable per-machine id: the MAC address of the first non-loopback
// interface, as the Windows build uses its first adapter's address
sint64 net_get_uid()
{
	struct ifaddrs* pAddrs = nullptr;
	if (getifaddrs(&pAddrs) != 0)
	{
		return -1;
	}

	sint64 uid = -1;
	for (struct ifaddrs* p = pAddrs; p; p = p->ifa_next)
	{
		if (!p->ifa_addr || p->ifa_addr->sa_family != AF_PACKET || (p->ifa_flags & IFF_LOOPBACK))
		{
			continue;
		}

		const struct sockaddr_ll* pLink = (const struct sockaddr_ll*)p->ifa_addr;
		uid = 0;
		for (int i = 0; i < pLink->sll_halen && i < 8; i++)
		{
			uid |= (sint64)pLink->sll_addr[i] << (8 * i);
		}
		break;
	}

	freeifaddrs(pAddrs);
	return uid;
}

}
