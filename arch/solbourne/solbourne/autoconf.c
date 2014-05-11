/*	$OpenBSD: autoconf.c,v 1.12 2010/11/18 21:13:19 miod Exp $	*/
/*	OpenBSD: autoconf.c,v 1.64 2005/03/23 17:10:24 miod Exp 	*/

/*
 * Copyright (c) 1996
 *    The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <net/if.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/pmap.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/timerreg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <machine/idt.h>
#include <machine/kap.h>
#include <machine/prom.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	fbnode;		/* node ID of ROM's console frame buffer */

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

static	int mbprint(void *, const char *);
void	sync_crash(void);
int	mainbus_match(struct device *, void *, void *);
static	void mainbus_attach(struct device *, struct device *, void *);

struct	bootpath bootpath[8];
int	nbootpath;
static	void bootpath_build(void);
char	mainbus_model[30];

u_int	prom_argc;
char  **prom_argv;
char  **prom_environ;
vaddr_t	prom_data;

/*
 * locore.s code calls bootstrap() just before calling main(), after double
 * mapping the kernel to high memory and setting up the trap base register.
 * We must finish mapping the kernel properly and glean any bootstrap info.
 */
void
bootstrap()
{
	char **old_argv, **old_environ;
	u_int i, nenv;
	size_t asize;
	char *dst;

	bzero(&cpuinfo, sizeof(struct cpu_softc));
	cpuinfo.master = 1;
	getcpuinfo(&cpuinfo, 0);

	/*
	 * Compute how much memory is used by the PROM arguments.
	 */
	asize = prom_argc * sizeof(const char *);
	for (i = 0; i < prom_argc; i++)
		asize += 1 + strlen(prom_argv[i]);
	asize = roundup(asize, 4);

	for (nenv = 0; prom_environ[nenv] != NULL; nenv++)
		asize += 1 + strlen(prom_environ[i]);
	asize = roundup(asize, 4);
	asize += (1 + nenv) * sizeof(const char *);
	
	/*
	 * Setup the initial mappings.
	 */
	pmap_bootstrap(asize);

	/*
	 * Now that we have allocated memory for the commandline arguments
	 * and the environment, save them.
	 */
	old_argv = prom_argv;
	prom_argv = (char **)prom_data;
	dst = (char *)(prom_data + prom_argc * sizeof(char *));
	for (i = 0; i < prom_argc; i++) {
		prom_argv[i] = dst;
		asize = 1 + strlen(old_argv[i]);
		bcopy(old_argv[i], dst, asize);
		dst += asize;
	}
	old_environ = prom_environ;
	prom_environ = (char **)roundup((vaddr_t)dst, 4);
	dst = (char *)((vaddr_t)prom_environ + (1 + nenv) * sizeof(char *));
	for (i = 0; i < nenv; i++) {
		prom_environ[i] = dst;
		asize = 1 + strlen(old_environ[i]);
		bcopy(old_environ[i], dst, asize);
		dst += asize;
	}
	prom_environ[nenv] = NULL;

	/* Moved zs_kgdb_init() to zs.c:consinit() */
#ifdef DDB
	db_machine_init();
	ddb_init();
#endif
}

/*
 * bootpath_build: build a bootpath. Used when booting a generic
 * kernel to find our root device.  Newer proms give us a bootpath,
 * for older proms we have to create one.  An element in a bootpath
 * has 4 fields: name (device name), val[0], val[1], and val[2]. Note that:
 * Interpretation of val[] is device-dependent. Some examples:
 *
 * if (val[0] == -1) {
 *	val[1] is a unit number    (happens most often with old proms)
 * } else {
 *	[sbus device] val[0] is a sbus slot, and val[1] is an sbus offset
 *	[scsi disk] val[0] is target, val[1] is lun, val[2] is partition
 *	[scsi tape] val[0] is target, val[1] is lun, val[2] is file #
 * }
 *
 */

static void
bootpath_build()
{
	u_int i;
	char *cp;

/* XXX needs a rewrite for S4000 - we do not need to do things that way */

	bzero(bootpath, sizeof(bootpath));

	/*
	 * The boot path is contained in argv[0] only.
	 * It has the form:
	 * [subdevice.]device([[ctrl],[unit],[partition]])[/]path
	 */

	printf("bootpath: %s\n", prom_argv[0]);

	/*
	 * Remaining arguments are interpreted as options, with or without
	 * leading dashes.
	 */
	for (i = 1; i < prom_argc; i++) {
		for (cp = prom_argv[i]; *cp != '\0'; cp++)
			switch (*cp) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;

			case 'c':
				boothowto |= RB_CONFIG;
				break;

#ifdef DDB
			case 'd':
				Debugger();
				break;
#endif

			case 's':
				boothowto |= RB_SINGLE;
				break;
			}
	}
}

/*
 * save or read a bootpath pointer from the boothpath store.
 *
 * XXX. required because of SCSI... we don't have control over the "sd"
 * device, so we can't set boot device there.   we patch in with
 * device_register(), and use this to recover the bootpath.
 */

struct bootpath *
bootpath_store(storep, bp)
	int storep;
	struct bootpath *bp;
{
	static struct bootpath *save;
	struct bootpath *retval;

	retval = save;
	if (storep)
		save = bp;

	return (retval);
}

/*
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
void
cpu_configure()
{
	struct confargs oca;
	register int node = 0;
	register char *cp;
	int s;
	extern struct user *proc0paddr;

	/* build the bootpath */
	bootpath_build();

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

	node = findroot();

	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = cp = "mainbus";
	if (config_rootfound(cp, (void *)&oca) == NULL)
		panic("mainbus not configured");

	/* Enable device interrupts */
	sta(GLU_ICR, ASI_PHYS_IO,
	    ((lda(GLU_ICR, ASI_PHYS_IO) >> 24) & ~GICR_DISABLE_ALL) << 24);
	(void)spl0();

	cold = 0;

	/*
	 * Re-zero proc0's user area, to nullify the effect of the
	 * stack running into it during auto-configuration.
	 * XXX - should fix stack usage.
	 */
	s = splhigh();
	bzero(proc0paddr, sizeof(struct user));

	pmap_redzone();
	splx(s);
}

void
diskconf(void)
{
	struct bootpath *bp;
	struct device *bootdv;
	int bootpart;

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	bp = nbootpath == 0 ? NULL : &bootpath[nbootpath-1];
	bootdv = (bp == NULL) ? NULL : bp->dev;
	bootpart = (bp == NULL) ? 0 : bp->val[2];

	setroot(bootdv, bootpart, RB_USERREQ | RB_HALT);
	dumpconf();
}

/*
 * Console `sync' command.  SunOS just does a `panic: zero' so I guess
 * no one really wants anything fancy...
 */
void
sync_crash()
{

	panic("PROM sync command");
}

char *
clockfreq(freq)
	register int freq;
{
	register char *p;
	static char buf[10];

	freq /= 1000;
	snprintf(buf, sizeof buf, "%d", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		snprintf(p, buf + sizeof buf - p, "%d", freq);
		*p = '.';	/* now buf = %d.%3d */
	}
	return (buf);
}

/* ARGSUSED */
static int
mbprint(aux, name)
	void *aux;
	const char *name;
{
	register struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	if (ca->ca_ra.ra_paddr)
		printf(" %saddr 0x%x", ca->ca_ra.ra_iospace ? "io" : "",
		    (int)ca->ca_ra.ra_paddr);
	return (UNCONF);
}

/*
 * Given a `first child' node number, locate the node with the given name.
 * Return the node number, or 0 if not found.
 */
int
findnode(first, name)
	int first;
	register const char *name;
{
	register int node;

	for (node = first; node; node = nextsibling(node))
		if (strcmp(getpropstring(node, "name"), name) == 0)
			return (node);
	return (0);
}

int
mainbus_match(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct cfdata *cf = self;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach the mainbus.
 *
 * Our main job is to attach the CPU (the root node we got in cpu_configure())
 * and iterate down the list of `mainbus devices' (children of that node).
 * We also record the `node id' of the default frame buffer, if any.
 */
static void
mainbus_attach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct confargs oca;
	struct confargs *ca = aux;
	int node0;
	const char *model;

	node0 = ca->ca_ra.ra_node;	/* i.e., the root node */

	model = getpropstring(node0, "model");
	if (model == NULL)
		model = sysmodel == SYS_S4000 ? "S4000" : "S4100";
	strlcpy(mainbus_model, model, sizeof mainbus_model);
	printf(": %s\n", mainbus_model);

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */

	/* Configure the CPU. */
	bzero(&oca, sizeof(oca));
	oca.ca_bustype = BUS_MAIN;
	oca.ca_ra.ra_node = node0;
	oca.ca_ra.ra_name = "cpu";
	(void)config_found(dev, (void *)&oca, mbprint);

	/*
	 * XXX we don't support frame buffers, yet
	 */
	fbnode = 0;

	bzero(&oca, sizeof(oca));
	oca.ca_bustype = BUS_MAIN;
	oca.ca_ra.ra_node = node0;
	oca.ca_ra.ra_name = "obio";
	(void)config_found(dev, (void *)&oca, mbprint);
}

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/*
 * findzs() is called from the zs driver (which is, at least in theory,
 * generic to any machine with a Zilog ZSCC chip).  It should return the
 * address of the corresponding zs channel.  It may not fail, and it
 * may be called before the VM code can be used.  Here we count on the
 * FORTH PROM to map in the required zs chips.
 */
void *
findzs(zs)
	int zs;
{

	if (CPU_ISKAP) {
		struct rom_reg rr;
		register void *vaddr;

		switch (zs) {
		case 0:
			rr.rr_paddr = (void *)ZS0_BASE;
			break;
		case 1:
			rr.rr_paddr = (void *)ZS1_BASE;
			break;
		default:
			panic("findzs: unknown zs device %d", zs);
		}

		rr.rr_iospace = PMAP_OBIO;
		rr.rr_len = PAGE_SIZE;
		vaddr = mapiodev(&rr, 0, PAGE_SIZE);
		if (vaddr != NULL)
			return (vaddr);
	}

	panic("findzs: cannot find zs%d", zs);
	/* NOTREACHED */
}

/*
 * Return a string property.  There is a (small) limit on the length;
 * the string is fetched into a static buffer which is overwritten on
 * subsequent calls.
 */
char *
getpropstring(node, name)
	int node;
	char *name;
{
	register int len;
	static char stringbuf[32];

	len = getprop(node, name, (void *)stringbuf, sizeof stringbuf - 1);
	if (len == -1)
		len = 0;
	stringbuf[len] = '\0';	/* usually unnecessary */
	return (stringbuf);
}

/*
 * Fetch an integer (or pointer) property.
 * The return value is the property, or the default if there was none.
 */
int
getpropint(node, name, deflt)
	int node;
	char *name;
	int deflt;
{
	register int len;
	char intbuf[16];

	len = getprop(node, name, (void *)intbuf, sizeof intbuf);
	if (len != 4)
		return (deflt);
	return (*(int *)intbuf);
}

int
node_has_property(node, prop)	/* returns 1 if node has given property */
	register int node;
	register const char *prop;
{

	return (getproplen(node, (char *)prop) != -1);
}

void
callrom()
{

#ifdef notyet
	promvec->pv_abort();
#endif
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = TAILQ_FIRST(&alldevs);
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	snprintf(num, sizeof num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strlcpy(fullname, name, sizeof fullname);
	strlcat(fullname, num, sizeof fullname);

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = TAILQ_NEXT(dev, dv_list)) == NULL)
			return NULL;
	}
	return dev;
}

void
device_register(struct device *dev, void *aux)
{
}

struct nam2blk nam2blk[] = {
	{ "sd",		 7 },
	{ "st",		11 },
	{ "fd",		16 },
	{ "rd",		17 },
	{ "cd",		18 },
	{ NULL,		-1 }
};
