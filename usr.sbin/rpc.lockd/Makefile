#	$NetBSD: Makefile,v 1.12 2000/08/07 16:23:31 thorpej Exp $
# $FreeBSD: src/usr.sbin/rpc.lockd/Makefile,v 1.19 2003/10/26 06:10:44 peter Exp $

PROG=	rpc.lockd
MAN=	rpc.lockd.8
MLINKS=	rpc.lockd.8 lockd.8
SRCS=	nlm_prot_svc.c lockd.c lock_proc.c lockd_lock.c

CFLAGS+=-I${DESTDIR}/usr/include/rpcsvc
WARNS?= 3

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

CLEANFILES= nlm_prot_svc.c test

RPCSRC= ${DESTDIR}/usr/include/rpcsvc/nlm_prot.x
RPCGEN= rpcgen -L -C

nlm_prot_svc.c: ${RPCSRC}
	${RPCGEN} -m -o ${.TARGET} ${RPCSRC}

test: ${.CURDIR}/test.c
	cc -o test ${.CURDIR}/test.c -lrpcsvc

.include <bsd.prog.mk>
