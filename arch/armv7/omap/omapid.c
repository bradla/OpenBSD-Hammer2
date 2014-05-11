/* $OpenBSD: omapid.c,v 1.2 2013/11/06 19:03:07 syl Exp $ */
/*
 * Copyright (c) 2013 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <armv7/armv7/armv7var.h>

/* registers */
#define O4_ID_SIZE	0x1000
#define O4_FUSE_ID0	0x200
#define O4_ID_CODE	0x204
#define O4_FUSE_ID1	0x208
#define O4_FUSE_ID2	0x20C
#define O4_FUSE_ID3	0x210
#define O4_FUSE_PRODID0	0x214
#define O4_FUSE_PRODID1	0x218


struct omapid_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct omapid_softc *omapid_sc;


void omapid_attach(struct device *parent, struct device *self, void *args);
void omapid_wpending(int flags);

struct cfattach	omapid_ca = {
	sizeof (struct omapid_softc), NULL, omapid_attach
};

struct cfdriver omapid_cd = {
	NULL, "omapid", DV_DULL
};

void amptimer_set_clockrate(int32_t new_frequency); /* XXX */

void
omapid_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct omapid_softc *sc = (struct omapid_softc *) self;
	uint32_t rev;
	uint32_t newclockrate = 0;
	char *board;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("omapid: bus_space_map failed!");

	omapid_sc = sc;

	board = "unknown";
	switch (board_id) {
	case BOARD_ID_OMAP4_PANDA:
		rev = bus_space_read_4(sc->sc_iot, sc->sc_ioh, O4_ID_CODE);
		switch ((rev >> 12) & 0xffff) {
		case 0xB852:
		case 0xB95C:
			board = "omap4430";
			newclockrate = 400 * 1000 * 1000;
			break;
		case 0xB94E:
			board = "omap4460";
			newclockrate = 350 * 1000 * 1000;
			break;
		}
		break;
	default:
		break;
	}
	printf(": %s\n", board);
	if (newclockrate != 0)
		amptimer_set_clockrate(newclockrate);
}
