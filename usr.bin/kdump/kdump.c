/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1988, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)kdump.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/kdump/kdump.c,v 1.29 2006/05/20 14:27:22 netchild Exp $
 * $DragonFly: src/usr.bin/kdump/kdump.c,v 1.12 2008/10/15 16:04:11 hasso Exp $
 */

#define _KERNEL_STRUCTURES

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include "ktrace.h"
#include "kdump_subr.h"

int timestamp, decimal, fancy = 1, tail, maxdata = 64;
int fixedformat;
char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

main(int argc, char **argv)
{
	int ch, col, ktrlen, size;
	pid_t do_pid = -1;
	register void *m;
	int trpoints = ALL_POINTS;
	char *cp;

	(void) setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc,argv,"f:djlm:np:RTt:")) != -1)
		switch((char)ch) {
		case 'f':
			tracefile = optarg;
			break;
		case 'j':
			fixedformat = 1;
			break;
		case 'd':
			decimal = 1;
			break;
		case 'l':
			tail = 1;
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'n':
			fancy = 0;
			break;
		case 'p':
			do_pid = strtoul(optarg, &cp, 0);
			if (*cp != 0)
				errx(1,"invalid number %s", optarg);
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		default:
			usage();
		}

	if (argc > optind)
		usage();

	m = (void *)malloc(size = 1025);
	if (m == NULL)
		errx(1, "%s", strerror(ENOMEM));
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	while (fread_tail(&ktr_header, sizeof(struct ktr_header), 1)) {
		if (trpoints & (1 << ktr_header.ktr_type) &&
		    (do_pid == -1 || ktr_header.ktr_pid == do_pid))
			col = dumpheader(&ktr_header);
		else
			col = -1;
		if ((ktrlen = ktr_header.ktr_len) < 0)
			errx(1, "bogus length 0x%x", ktrlen);
		if (ktrlen > size) {
			m = (void *)realloc(m, ktrlen+1);
			if (m == NULL)
				errx(1, "%s", strerror(ENOMEM));
			size = ktrlen;
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if ((trpoints & (1<<ktr_header.ktr_type)) == 0)
			continue;
		if (col == -1)
			continue;
		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall((struct ktr_syscall *)m);
			break;
		case KTR_SYSRET:
			ktrsysret((struct ktr_sysret *)m);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio((struct ktr_genio *)m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig((struct ktr_psig *)m);
			break;
		case KTR_CSW:
			ktrcsw((struct ktr_csw *)m);
			break;
		case KTR_USER:
			ktruser(ktrlen, m);
			break;
		}
		if (tail)
			(void)fflush(stdout);
	}
}

fread_tail(char *buf, int size, int num)
{
	int i;

	while ((i = fread(buf, size, num, stdin)) == 0 && tail) {
		(void)sleep(1);
		clearerr(stdin);
	}
	return (i);
}

dumpheader(struct ktr_header *kth)
{
	static char unknown[64];
	static struct timeval prevtime, temp;
	char *type;
	int col;

	switch (kth->ktr_type) {
	case KTR_SYSCALL:
		type = "CALL";
		break;
	case KTR_SYSRET:
		type = "RET ";
		break;
	case KTR_NAMEI:
		type = "NAMI";
		break;
	case KTR_GENIO:
		type = "GIO ";
		break;
	case KTR_PSIG:
		type = "PSIG";
		break;
	case KTR_CSW:
		type = "CSW";
		break;
	case KTR_USER:
		type = "USER";
		break;
	default:
		(void)sprintf(unknown, "UNKNOWN(%d)", kth->ktr_type);
		type = unknown;
	}

	if (kth->ktr_tid || (kth->ktr_flags & KTRH_THREADED) || fixedformat)
		col = printf("%5d:%-4d", kth->ktr_pid, kth->ktr_tid);
	else
		col = printf("%5d", kth->ktr_pid);
	col += printf(" %-8.*s ", MAXCOMLEN, kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			temp = kth->ktr_time;
			timevalsub(&kth->ktr_time, &prevtime);
			prevtime = temp;
		}
		col += printf("%ld.%06ld ",
		    kth->ktr_time.tv_sec, kth->ktr_time.tv_usec);
	}
	col += printf("%s  ", type);
	return col;
}

#include <sys/syscall.h>
#define KTRACE
#include <sys/kern/syscalls.c>
#undef KTRACE
int nsyscalls = sizeof (syscallnames) / sizeof (syscallnames[0]);

static char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_STEP",	"PT_ATTACH",	"PT_DETACH",
};

ktrsyscall(register struct ktr_syscall *ktr)
{
	register narg = ktr->ktr_narg;
	register register_t *ip;
	char *ioctlname();

	if (ktr->ktr_code >= nsyscalls || ktr->ktr_code < 0)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", syscallnames[ktr->ktr_code]);
	ip = &ktr->ktr_args[0];
	if (narg) {
		char c = '(';
		if (fancy) {

#define print_number(i,n,c) do {                      \
	if (decimal)                                  \
		(void)printf("%c%ld", c, (long)*i);   \
	else                                          \
		(void)printf("%c%#lx", c, (long)*i);  \
	i++;                                          \
	n--;                                          \
	c = ',';                                      \
	} while (0);

			if (ktr->ktr_code == SYS_ioctl) {
				char *cp;
				print_number(ip,narg,c);
				if ((cp = ioctlname(*ip)) != NULL)
					(void)printf(",%s", cp);
				else {
					if (decimal)
						(void)printf(",%ld", (long)*ip);
					else
						(void)printf(",%#lx ", (long)*ip);
				}
				c = ',';
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_access) {
				print_number(ip,narg,c);
				(void)putchar(',');
				accessmodename ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_open) {
				int	flags;
				int	mode;
				print_number(ip,narg,c);
				flags = *ip;
				mode = *++ip;
				(void)putchar(',');
				flagsandmodename (flags, mode, decimal);
				ip++;
				narg-=2;
			} else if (ktr->ktr_code == SYS_wait4) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				wait4optname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_chmod ||
				   ktr->ktr_code == SYS_fchmod ||
				   ktr->ktr_code == SYS_lchmod) {
				print_number(ip,narg,c);
				(void)putchar(',');
				modename ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_mknod) {
				print_number(ip,narg,c);
				(void)putchar(',');
				modename ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_getfsstat) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				getfsstatflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_mount) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				mountflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_unmount) {
				print_number(ip,narg,c);
				(void)putchar(',');
				mountflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_recvmsg ||
				   ktr->ktr_code == SYS_sendmsg) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				sendrecvflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_recvfrom ||
				   ktr->ktr_code == SYS_sendto) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				sendrecvflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_chflags ||
				   ktr->ktr_code == SYS_fchflags) {
				print_number(ip,narg,c);
				(void)putchar(',');
				modename((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_kill) {
				print_number(ip,narg,c);
				(void)putchar(',');
				signame((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_reboot) {
				(void)putchar('(');
				rebootoptname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_umask) {
				(void)putchar('(');
				modename((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_msync) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				msyncflagsname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_mmap) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				mmapprotname ((int)*ip);
				(void)putchar(',');
				ip++;
				narg--;
				mmapflagsname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_mprotect) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				mmapprotname ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_madvise) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				madvisebehavname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_setpriority) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				prioname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_fcntl) {
				int cmd;
				int arg;
				print_number(ip,narg,c);
				cmd = *ip;
				arg = *++ip;
				(void)putchar(',');
				fcntlcmdname(cmd, arg, decimal);
				ip++;
				narg-=2;
			} else if (ktr->ktr_code == SYS_socket) {
				(void)putchar('(');
				sockdomainname((int)*ip);
				ip++;
				narg--;
				(void)putchar(',');
				socktypename((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_setsockopt ||
				   ktr->ktr_code == SYS_getsockopt) {
				print_number(ip,narg,c);
				(void)putchar(',');
				sockoptlevelname((int)*ip, decimal);
				ip++;
				narg--;
				(void)putchar(',');
				sockoptname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_lseek) {
				print_number(ip,narg,c);
				/* Hidden 'pad' argument, not in lseek(2) */
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				(void)putchar(',');
				whencename ((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_flock) {
				print_number(ip,narg,c);
				(void)putchar(',');
				flockname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_mkfifo ||
				   ktr->ktr_code == SYS_mkdir) {
				print_number(ip,narg,c);
				(void)putchar(',');
				modename((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_shutdown) {
				print_number(ip,narg,c);
				(void)putchar(',');
				shutdownhowname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_socketpair) {
				(void)putchar('(');
				sockdomainname((int)*ip);
				ip++;
				narg--;
				(void)putchar(',');
				socktypename((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_getrlimit ||
				   ktr->ktr_code == SYS_setrlimit) {
				(void)putchar('(');
				rlimitname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_quotactl) {
				print_number(ip,narg,c);
				quotactlname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_rtprio) {
				(void)putchar('(');
				rtprioname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS___semctl) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				semctlname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_semget) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				semgetname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_msgctl) {
				print_number(ip,narg,c);
				shmctlname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_shmat) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				shmatname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_shmctl) {
				print_number(ip,narg,c);
				shmctlname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_minherit) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				minheritname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_rfork) {
				(void)putchar('(');
				rforkname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_lio_listio) {
				(void)putchar('(');
				lio_listioname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_mlockall) {
				(void)putchar('(');
				mlockallname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_sched_setscheduler) {
				print_number(ip,narg,c);
				schedpolicyname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_sched_get_priority_max ||
				   ktr->ktr_code == SYS_sched_get_priority_min) {
				(void)putchar('(');
				schedpolicyname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_sendfile) {
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				print_number(ip,narg,c);
				sendfileflagsname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_kldsym) {
				print_number(ip,narg,c);
				kldsymcmdname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_sigprocmask) {
				(void)putchar('(');
				sigprocmaskhowname((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS___acl_get_file ||
				   ktr->ktr_code == SYS___acl_set_file ||
				   ktr->ktr_code == SYS___acl_get_fd ||
				   ktr->ktr_code == SYS___acl_set_fd ||
				   ktr->ktr_code == SYS___acl_delete_file ||
				   ktr->ktr_code == SYS___acl_delete_fd ||
				   ktr->ktr_code == SYS___acl_aclcheck_file ||
				   ktr->ktr_code == SYS___acl_aclcheck_fd) {
				print_number(ip,narg,c);
				acltypename((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_sigaction) {
				(void)putchar('(');
				signame((int)*ip);
				ip++;
				narg--;
				c = ',';
			} else if (ktr->ktr_code == SYS_extattrctl) {
				print_number(ip,narg,c);
				extattrctlname((int)*ip);
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_ptrace) {
				if (*ip < sizeof(ptrace_ops) /
				    sizeof(ptrace_ops[0]) && *ip >= 0)
					(void)printf("(%s", ptrace_ops[*ip]);
#ifdef PT_GETREGS
				else if (*ip == PT_GETREGS)
					(void)printf("(%s", "PT_GETREGS");
#endif
#ifdef PT_SETREGS
				else if (*ip == PT_SETREGS)
					(void)printf("(%s", "PT_SETREGS");
#endif
#ifdef PT_GETFPREGS
				else if (*ip == PT_GETFPREGS)
					(void)printf("(%s", "PT_GETFPREGS");
#endif
#ifdef PT_SETFPREGS
				else if (*ip == PT_SETFPREGS)
					(void)printf("(%s", "PT_SETFPREGS");
#endif
#ifdef PT_GETDBREGS
				else if (*ip == PT_GETDBREGS)
					(void)printf("(%s", "PT_GETDBREGS");
#endif
#ifdef PT_SETDBREGS
				else if (*ip == PT_SETDBREGS)
					(void)printf("(%s", "PT_SETDBREGS");
#endif
				else
					(void)printf("(%ld", (long)*ip);
				c = ',';
				ip++;
				narg--;
			}
		}
		while (narg > 0) {
			print_number(ip,narg,c);
		}
		(void)putchar(')');
	}
	(void)putchar('\n');
}

ktrsysret(struct ktr_sysret *ktr)
{
	register register_t ret = ktr->ktr_retval;
	register int error = ktr->ktr_error;
	register int code = ktr->ktr_code;

	if (code >= nsyscalls || code < 0)
		(void)printf("[%d] ", code);
	else
		(void)printf("%s ", syscallnames[code]);

	if (error == 0) {
		if (fancy) {
			(void)printf("%d", ret);
			if (ret < 0 || ret > 9)
				(void)printf("/%#lx", (long)ret);
		} else {
			if (decimal)
				(void)printf("%ld", (long)ret);
			else
				(void)printf("%#lx", (long)ret);
		}
	} else if (error == ERESTART)
		(void)printf("RESTART");
	else if (error == EJUSTRETURN)
		(void)printf("JUSTRETURN");
	else {
		(void)printf("-1 errno %d", ktr->ktr_error);
		if (fancy)
			(void)printf(" %s", strerror(ktr->ktr_error));
	}
	(void)putchar('\n');
}

ktrnamei(char *cp, int len)
{
	(void)printf("\"%.*s\"\n", len, cp);
}

ktrgenio(struct ktr_genio *ktr, int len)
{
	register int datalen = len - sizeof (struct ktr_genio);
	register char *dp = (char *)ktr + sizeof (struct ktr_genio);
	register char *cp;
	register int col = 0;
	register width;
	char visbuf[5];
	static screenwidth = 0;

	if (screenwidth == 0) {
		struct winsize ws;

		if (fancy && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}
	printf("fd %d %s %d byte%s\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen,
		datalen == 1 ? "" : "s");
	if (maxdata && datalen > maxdata)
		datalen = maxdata;
	(void)printf("       \"");
	col = 8;
	for (;datalen > 0; datalen--, dp++) {
		(void) vis(visbuf, *dp, VIS_CSTYLE, *(dp+1));
		cp = visbuf;
		/*
		 * Keep track of printables and
		 * space chars (like fold(1)).
		 */
		if (col == 0) {
			(void)putchar('\t');
			col = 8;
		}
		switch(*cp) {
		case '\n':
			col = 0;
			(void)putchar('\n');
			continue;
		case '\t':
			width = 8 - (col&07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth-2)) {
			(void)printf("\\\n\t");
			col = 8;
		}
		col += width;
		do {
			(void)putchar(*cp++);
		} while (*cp);
	}
	if (col == 0)
		(void)printf("       ");
	(void)printf("\"\n");
}

char *signames[NSIG] = {
	"NULL", "HUP", "INT", "QUIT", "ILL", "TRAP", "IOT",	/*  1 - 6  */
	"EMT", "FPE", "KILL", "BUS", "SEGV", "SYS",		/*  7 - 12 */
	"PIPE", "ALRM",  "TERM", "URG", "STOP", "TSTP",		/* 13 - 18 */
	"CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU",		/* 19 - 24 */
	"XFSZ", "VTALRM", "PROF", "WINCH", "29", "USR1",	/* 25 - 30 */
	"USR2", NULL,						/* 31 - 32 */
};

ktrpsig(struct ktr_psig *psig)
{
	(void)printf("SIG%s ", signames[psig->signo]);
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL\n");
	else
		(void)printf("caught handler=0x%lx mask=0x%x code=0x%x\n",
		    (u_long)psig->action, psig->mask, psig->code);
}

ktrcsw(struct ktr_csw *cs)
{
	(void)printf("%s %s\n", cs->out ? "stop" : "resume",
		cs->user ? "user" : "kernel");
}

struct utrace_malloc {
	void *p;
	size_t s;
	void *r;
};

void
ktruser_malloc(int len, unsigned char *p)
{
	struct utrace_malloc *ut = (struct utrace_malloc *)p;

	if (ut->p == NULL) {
		if (ut->s == 0 && ut->r == NULL)
			printf("malloc_init()\n");
		else
			printf("%p = malloc(%zu)\n", ut->r, ut->s);
	} else {
		if (ut->s == 0)
			printf("free(%p)\n", ut->p);
		else
			printf("%p = realloc(%p, %zu)\n", ut->r, ut->p, ut->s);
	}
}

ktruser(int len, unsigned char *p)
{
	if (len == sizeof(struct utrace_malloc)) {
		ktruser_malloc(len, p);
		return;
	}

	(void)printf("%d ", len);
	while (len--)
		(void)printf(" %02x", *p++);
	(void)printf("\n");
}

usage(void)
{
	(void)fprintf(stderr,
	    "usage: kdump [-dnlRT] [-f trfile] [-m maxdata] [-t [cnisuw]] [-p pid]\n");
	exit(1);
}

timevalsub(struct timeval *t1, struct timeval *t2)
{
	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

timevalfix(struct timeval *t1)
{
	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}
