/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef NET_PLATFORM_POSIX_H
#define NET_PLATFORM_POSIX_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef sockaddr_in SocketIn;
typedef in_addr_t SocketInAddr;
typedef ip_mreq SocketMreq;

typedef int SOCKET;
#define SOCKET_ERROR (-1)

#endif
