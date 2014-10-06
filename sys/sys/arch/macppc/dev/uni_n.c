/*	$OpenBSD: uni_n.c,v 1.16 2012/11/15 21:50:00 mpi Exp $	*/

/*
 * Copyright (c) 1998-2001 Dale Rahn.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

struct memc_softc {
	struct device sc_dev;
	struct ppc_bus_space sc_membus_space;

};

int	memcmatch(struct device *, void *, void *);
void	memcattach(struct device *, struct device *, void *);
void	memc_attach_children(struct memc_softc *sc, int memc_node);
int	memc_print(void *aux, const char *name);

/* Driver definition */
struct cfdriver memc_cd = {
	NULL, "memc", DV_DULL
};
/* Driver definition */
struct cfattach memc_ca = {
	sizeof(struct memc_softc), memcmatch, memcattach
};

void uni_n_config(char *, int);

int
memcmatch(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	static int memc_attached = 0;

	/* allow only one instance */
	if (memc_attached == 0) {
		if (0 == strcmp (ca->ca_name, "memc"))
			return 1;
	}
	return 0;
}

void
memcattach(struct device *parent, struct device *self, void *aux)
{
	struct memc_softc *sc = (struct memc_softc *)self;
	struct confargs *ca = aux;
	u_int32_t rev;
	char name[64];
	int len;

	len = OF_getprop(ca->ca_node, "name", name, sizeof(name));
	if (len > 0)
		name[len] = 0;

	len = OF_getprop(ca->ca_node, "device-rev", &rev, sizeof(rev));
	if (len < 0)
		rev = 0;

	uni_n_config(name, ca->ca_node);

	printf (": %s rev 0x%x\n", name, rev);

	memc_attach_children(sc, ca->ca_node);
}

void
memc_attach_children(struct memc_softc *sc, int memc_node)
{
	struct confargs ca;
	int node, namelen;
	u_int32_t reg[20];
	char	name[32];

        sc->sc_membus_space.bus_base = ca.ca_baseaddr;

	ca.ca_iot = &sc->sc_membus_space;
	ca.ca_dmat = 0; /* XXX */
	ca.ca_baseaddr = 0; /* XXX */
	sc->sc_membus_space.bus_base = ca.ca_baseaddr;

        for (node = OF_child(memc_node); node; node = OF_peer(node)) {
		namelen = OF_getprop(node, "name", name, sizeof(name));
		if (namelen < 0)
			continue;
		if (namelen >= sizeof(name))
			continue;
		name[namelen] = 0;

		ca.ca_name = name;
		ca.ca_node = node;
		ca.ca_nreg  = OF_getprop(node, "reg", reg, sizeof(reg));
		ca.ca_reg = reg;
		ca.ca_nintr = 0; /* XXX */
		ca.ca_intr = NULL; /* XXX */

		config_found((struct device *)sc, &ca, memc_print);
	}
}

int
memc_print(void *aux, const char *name)
{
	struct confargs *ca = aux;
	/* we dont want extra stuff printing */
	if (name)
		printf("\"%s\" at %s", ca->ca_name, name);
	if (ca->ca_nreg > 0)
		printf(" offset 0x%x", ca->ca_reg[0]);
	return UNCONF;
}

void
uni_n_config(char *name, int handle)
{
	char *baseaddr;
	int *ctladdr;
	u_int32_t address;

	/* sanity test */
	if (strcmp (name, "uni-n") == 0 || strcmp (name, "u3") == 0
	    || strcmp (name, "u4") == 0) {
		if (OF_getprop(handle, "reg", &address,
		    sizeof address) > 0) {
			baseaddr = mapiodev(address, NBPG);
			ctladdr = (void *)(baseaddr + 0x20);
			*ctladdr |= 0x02;
		}
	}
}
