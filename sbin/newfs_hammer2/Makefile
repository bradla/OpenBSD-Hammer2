#
#
PROG=	newfs_hammer2
MAN=	newfs_hammer2.8
CFLAGS+= -I${.CURDIR}/../../sys -I${.CURDIR}/../hammer2 -I/usr/src/sys/kern 
#CFLAGS+= -I/usr/src/sys/kern -I/usr/src/sys/sys -I${.CURDIR}/../hammer2
SRCS= newfs_hammer2.c

.PATH: ${.CURDIR}/../../sys/lib/libkern /usr/src/sys/kern
#.PATH: ${.CURDIR}/../../sys/hammer2
SRCS+= icrc32.c

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
