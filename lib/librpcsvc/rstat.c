/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 *
 * @(#)rstat.c	1.2 91/03/11 TIRPC 1.0; from 1.6 89/03/24 SMI
 * $FreeBSD: src/lib/librpcsvc/rstat.c,v 1.4 2003/10/26 03:43:35 peter Exp $
 * $DragonFly: src/lib/librpcsvc/rstat.c,v 1.3 2007/11/25 14:33:02 swildner Exp $
 */

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
 * "High" level programmatic interface to rstat RPC service.
 */
#include <rpc/rpc.h>
#include <rpcsvc/rstat.h>

enum clnt_stat
rstat(char *host, struct statstime *statp)
{
	return (callrpc(host, RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
			(xdrproc_t)xdr_void, NULL,
			(xdrproc_t)xdr_statstime, (char *) statp));
}

int
havedisk(char *host)
{
	long have;
	
	if (callrpc(host, RSTATPROG, RSTATVERS_SWTCH, RSTATPROC_HAVEDISK,
			(xdrproc_t)xdr_void, NULL,
			(xdrproc_t)xdr_long, (char *) &have) != 0)
		return (-1);
	else
		return (have);
}
