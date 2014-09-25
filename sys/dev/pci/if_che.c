/*	$OpenBSD: if_che.c,v 1.10 2011/04/03 15:36:02 jasper Exp $ */

/*
 * Copyright (c) 2007 Claudio Jeker <claudio@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/* registers & defines */

#define CHE_PCI_BAR		0x10
#define CHE_PCI_CAP_ID_VPD	0x03
#define CHE_PCI_VPD_DATA	0x4
#define CHE_PCI_F_VPD_ADDR	0x80000000
#define CHE_PCI_VPD_BASE	0xc00

#define CHE_REG_T3DBG_GPIO_EN	0xd0
#define CHE_T3DBG_F_GPIO11_OEN		0x08000000
#define CHE_T3DBG_F_GPIO10_OEN		0x04000000
#define CHE_T3DBG_F_GPIO9_OEN		0x02000000
#define CHE_T3DBG_F_GPIO8_OEN		0x01000000
#define CHE_T3DBG_F_GPIO7_OEN		0x00800000
#define CHE_T3DBG_F_GPIO6_OEN		0x00400000
#define CHE_T3DBG_F_GPIO5_OEN		0x00200000
#define CHE_T3DBG_F_GPIO4_OEN		0x00100000
#define CHE_T3DBG_F_GPIO3_OEN		0x00080000
#define CHE_T3DBG_F_GPIO2_OEN		0x00040000
#define CHE_T3DBG_F_GPIO1_OEN		0x00020000
#define CHE_T3DBG_F_GPIO0_OEN		0x00010000
#define CHE_T3DBG_F_GPIO11_OUT_VAL	0x00000800
#define CHE_T3DBG_F_GPIO10_OUT_VAL	0x00000400
#define CHE_T3DBG_F_GPIO9_OUT_VAL	0x00000200
#define CHE_T3DBG_F_GPIO8_OUT_VAL	0x00000100
#define CHE_T3DBG_F_GPIO7_OUT_VAL	0x00000080
#define CHE_T3DBG_F_GPIO6_OUT_VAL	0x00000040
#define CHE_T3DBG_F_GPIO5_OUT_VAL	0x00000020
#define CHE_T3DBG_F_GPIO4_OUT_VAL	0x00000010
#define CHE_T3DBG_F_GPIO3_OUT_VAL	0x00000008
#define CHE_T3DBG_F_GPIO2_OUT_VAL	0x00000004
#define CHE_T3DBG_F_GPIO1_OUT_VAL	0x00000002
#define CHE_T3DBG_F_GPIO0_OUT_VAL	0x00000001
#define CHE_REG_I2C_CFG		0x6a0
#define CHE_I2C_CLKDIV(_x)	((_x) && 0xfff)
#define CHE_REG_MI1_CFG		0x6b0
#define CHE_REG_MI1_ADDR	0x6b4
#define CHE_REG_MI1_DATA	0x6b8
#define CHE_REG_MI1_OP		0x6bc
#define CHE_MI1_F_BUSY		(1U << 31)
#define CHE_MI1_F_ST		0x8
#define CHE_MI1_F_PREEN		0x4
#define CHE_MI1_F_MDIINV	0x2
#define CHE_MI1_F_MDIEN		0x1
#define CHE_MI1_CLKDIV(_x)	((_x) << 5)
#define CHE_MI1_PHYADDR(_x)	((_x) << 5)
#define CHE_MI1_OP(_x)		((_x) & 0x3)
#define CHE_REG_PL_RST		0x6f0
#define CHE_RST_F_CRSTWRM	0x2
#define CHE_RST_F_CRSTWRMMODE	0x1
#define CHE_REG_PL_REV		0x6f4
#define CHE_REG_XGM_PORT_CFG	0x8b8
#define CHE_XGMAC0_0_BASE_ADDR	0x800
#define CHE_XGMAC0_1_BASE_ADDR	0xa00
#define CHE_XGM_REG(_r, _i)	\
    ((_r) + (_i) * (CHE_XGMAC0_1_BASE_ADDR - CHE_XGMAC0_0_BASE_ADDR))
#define CHE_XGM_PORTSPEED(_x)	((_x) << 1)
#define CHE_XGM_F_ENRGMII	0x1
#define CHE_XGM_F_CLKDIVRESET	0x8

/* serial flash and firmware definitions */
#define CHE_REG_SF_DATA		0x6d8
#define CHE_REG_SF_OP		0x6dc
#define CHE_SF_SEC_SIZE		(64 * 1024)	/* serial flash sector size */
#define CHE_SF_SIZE		(8 * CHE_SF_SEC_SIZE)	/* serial flash size */
#define CHE_SF_PROG_PAGE	2
#define CHE_SF_WR_DISABLE	4
#define CHE_SF_RD_STATUS	5	/* read status register */
#define CHE_SF_WR_ENABLE	6
#define CHE_SF_RD_DATA		11
#define CHE_SF_SEC_ERASE	216
#define CHE_SF_F_BUSY		(1U << 31)
#define CHE_SF_F_OP		0x1
#define CHE_SF_CONT(_x)		((_x) << 3)
#define CHE_SF_BYTECNT_MASK	0x3
#define CHE_SF_BYTECNT(_x)	(((_x) & CHE_SF_BYTECNT_MASK) << 1)

#define FW_FLASH_BOOT_ADDR	0x70000	/* start address of FW in flash */
#define FW_VERS_ADDR		0x77ffc	/* flash address holding FW version */
#define FW_VERS_TYPE_N3		0
#define FW_VERS_TYPE_T3		1
#define FW_VERS_TYPE(_x)	(((_x) >> 28) & 0xf)
#define FW_VERS_MAJOR(_x)	(((_x) >> 16) & 0xfff)
#define FW_VERS_MINOR(_x)	(((_x) >> 8) & 0xff)
#define FW_VERS_MICRO(_x)	((_x) & 0xff)

/* Partial EEPROM Vital Product Data structure. */
struct che_vpd {
	u_int8_t	id_tag;
	u_int8_t	id_len[2];
	u_int8_t	id_data[16];
	u_int8_t	vpdr_tag;
	u_int8_t	vpdr_len[2];
	u_int8_t	pn_name[2];		/* part number */
	u_int8_t	pn_len;
	u_int8_t	pn_data[16];
	u_int8_t	ec_name[2];		/* EC level */
	u_int8_t	ec_len;
	u_int8_t	ec_data[16];
	u_int8_t	sn_name[2];		/* serial number */
	u_int8_t	sn_len;
	u_int8_t	sn_data[16];
	u_int8_t	na_name[2];		/* MAC address base */
	u_int8_t	na_len;
	u_int8_t	na_data[12];
	u_int8_t	cclk_name[2];		/* core clock */
	u_int8_t	cclk_len;
	u_int8_t	cclk_data[6];
	u_int8_t	mclk_name[2];		/* mem clock */
	u_int8_t	mclk_len;
	u_int8_t	mclk_data[6];
	u_int8_t	uclk_name[2];		/* uP clock */
	u_int8_t	uclk_len;
	u_int8_t	uclk_data[6];
	u_int8_t	mdc_name[2];		/* MDIO clock */
	u_int8_t	mdc_len;
	u_int8_t	mdc_data[6];
	u_int8_t	mt_name[2];		/* mem timing */
	u_int8_t	mt_len;
	u_int8_t	mt_data[2];
	u_int8_t	xaui0cfg_name[2];	/* XAUI0 config */
	u_int8_t	xaui0cfg_len;
	u_int8_t	xaui0cfg_data[6];
	u_int8_t	xaui1cfg_name[2];	/* XAUI1 config */
	u_int8_t	xaui1cfg_len;
	u_int8_t	xaui1cfg_data[6];
	u_int8_t	port0_name[2];		/* PHY0 */
	u_int8_t	port0_len;
	u_int8_t	port0_data[2];
	u_int8_t	port1_name[2];		/* PHY1 */
	u_int8_t	port1_len;
	u_int8_t	port1_data[2];
	u_int8_t	port2_name[2];		/* PHY2 */
	u_int8_t	port2_len;
	u_int8_t	port2_data[2];
	u_int8_t	port3_name[2];		/* PHY3 */
	u_int8_t	port3_len;
	u_int8_t	port3_data[2];
	u_int8_t	rv_name[2];		/* csum */
	u_int8_t	rv_len;
	u_int8_t	rv_data[1];
	u_int8_t	pad[4];			/* for multiple-of-4 sizing */
} __packed;


#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)

/* the pci controller */

struct cheg_softc {
	struct device		sc_dev;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;

	u_int32_t		sc_rev;		/* card revision */
	u_int32_t		sc_cclk;	/* core clock */
	u_int32_t		sc_mdc;		/* mdio clock */

	pci_vendor_id_t		sc_product;
};

int		cheg_match(struct device *, void *, void *);
void		cheg_attach(struct device *, struct device *, void *);
int		cheg_print(void *, const char *);

struct cfattach cheg_ca = {
	sizeof(struct cheg_softc),
	cheg_match,
	cheg_attach
};

struct cfdriver cheg_cd = {
	NULL, "cheg", DV_DULL
};

/* glue between the controller and the port */

struct che_attach_args {
	struct pci_attach_args	*caa_pa;
	pci_intr_handle_t	caa_ih;
	int			caa_port;
	u_int8_t		caa_lladdr[ETHER_ADDR_LEN];
};

/* che itself */

struct che_softc {
	struct device		sc_dev;
	struct arpcom		sc_ac;
	struct mii_data		sc_mii;

	struct cheg_softc	*sc_cheg;
	void			*sc_ih;
	int			sc_port;
};

int		che_match(struct device *, void *, void *);
void		che_attach(struct device *, struct device *, void *);

struct cfattach che_ca = {
	sizeof(struct che_softc),
	che_match,
	che_attach
};

struct cfdriver che_cd = {
	NULL, "che", DV_IFNET
};

int		che_write_flash_reg(struct cheg_softc *, size_t, int,
		    u_int32_t);
int		che_read_flash_reg(struct cheg_softc *, size_t, int,
		    u_int32_t *);
int		che_read_flash_multi4(struct cheg_softc *, u_int, u_int32_t *,
		    size_t);
int		che_read_eeprom(struct cheg_softc *, struct pci_attach_args *,
		    pcireg_t, pcireg_t *);
int		che_get_vpd(struct cheg_softc *, struct pci_attach_args *,
		    void *, size_t);
void		che_conv_lladdr(char *, u_int8_t *);
u_int32_t	che_conv_num(char *, size_t);
void		che_reset(struct cheg_softc *);
int		che_ioctl(struct ifnet *, u_long, caddr_t);
void		che_watchdog(struct ifnet *);
void		che_start(struct ifnet *);

/* ifmedia & mii helper functions */
int	che_ifmedia_upd(struct ifnet *);
void	che_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int	che_miibus_readreg(struct device *, int, int);
void	che_miibus_writereg(struct device *, int, int, int);
int	che_miibus_ind_readreg(struct device *, int, int);
void	che_miibus_ind_writereg(struct device *, int, int, int);
void	che_miibus_statchg(struct device *);

/* bus_space wrappers */
u_int32_t 	che_read(struct cheg_softc *, bus_size_t);
void		che_write(struct cheg_softc *, bus_size_t, u_int32_t);
int		che_waitfor(struct cheg_softc *, bus_size_t, u_int32_t, int);

/* HW low-level functions */
void	che_hw_init(struct cheg_softc *);

/* cheg */
struct cheg_device {
	pci_vendor_id_t	cd_vendor;
	pci_vendor_id_t	cd_product;
	u_int		cd_nports;
};

const struct cheg_device *cheg_lookup(struct pci_attach_args *);

const struct cheg_device che_devices[] = {
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_PE9000, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T302E, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T302X, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T310E, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T310X, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T320E, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T320X, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B02, 2 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B10, 1 },
        { PCI_VENDOR_CHELSIO, PCI_PRODUCT_CHELSIO_T3B20, 2 }
};

const struct cheg_device *
cheg_lookup(struct pci_attach_args *pa)
{
	int i;
	const struct cheg_device *cd;

	for (i = 0; i < nitems(che_devices); i++) {
		cd = &che_devices[i];
		if (cd->cd_vendor == PCI_VENDOR(pa->pa_id) &&
		    cd->cd_product == PCI_PRODUCT(pa->pa_id))
			return (cd);
	}

	return (NULL);
}

int
cheg_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (cheg_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
cheg_attach(struct device *parent, struct device *self, void *aux)
{
	struct cheg_softc *sc = (struct cheg_softc *)self;
	struct pci_attach_args *pa = aux;
	const struct cheg_device *cd;
	struct che_attach_args caa;
	struct che_vpd vpd;
	pcireg_t memtype;
	u_int32_t vers;
	u_int i;

	bzero(&caa, sizeof(caa));
	cd = cheg_lookup(pa);

	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, CHE_PCI_BAR);
	if (pci_mapreg_map(pa, CHE_PCI_BAR, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map host registers\n");
		return;
	}

	if (pci_intr_map(pa, &caa.caa_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}

	sc->sc_rev = che_read(sc, CHE_REG_PL_REV);

	/* reset the beast */
	che_reset(sc);

	if (che_read_flash_multi4(sc, FW_VERS_ADDR, &vers, 1) != 0) {
		printf(": unable to read flash version\n");
		goto unmap;
	}

	if (che_get_vpd(sc, pa, &vpd, sizeof(vpd)/sizeof(u_int32_t)) != 0) {
		printf(": unable to get vital product data\n");
		goto unmap;
	}

	printf(": %s revision %d firmware %s-%d.%d.%d\n",
	    pci_intr_string(pa->pa_pc, caa.caa_ih), sc->sc_rev,
	    FW_VERS_TYPE(vers) ? "T" : "N",
	    FW_VERS_MAJOR(vers), FW_VERS_MINOR(vers), FW_VERS_MICRO(vers));

	sc->sc_product = PCI_PRODUCT(pa->pa_id);
	sc->sc_cclk = che_conv_num(vpd.cclk_data, sizeof(vpd.cclk_data));
	sc->sc_mdc = che_conv_num(vpd.mdc_data, sizeof(vpd.mdc_data));

	che_hw_init(sc);

	caa.caa_pa = pa;
	che_conv_lladdr(vpd.na_data, caa.caa_lladdr);

	for (i = 0; i < cd->cd_nports; i++) {
		caa.caa_port = i;

		config_found(self, &caa, cheg_print);

		/*
		 * The VPD EEPROM stores only the base Ethernet address for the
		 * card. The last octet is increased by one for every additional
		 * port.
		 */
		caa.caa_lladdr[5] += 1;
	}

	return;

unmap:   
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
cheg_print(void *aux, const char *pnp)
{
	struct che_attach_args *caa = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", che_cd.cd_name, pnp);

	printf(" port %d", caa->caa_port);

	return (UNCONF);
}

int
che_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
che_attach(struct device *parent, struct device *self, void *aux)
{
	struct cheg_softc *gsc = (struct cheg_softc *)parent;
	struct che_softc *sc = (struct che_softc *)self;
	struct che_attach_args *caa = aux;
	struct ifnet *ifp;

	sc->sc_cheg = gsc;

	sc->sc_port = caa->caa_port;
	bcopy(caa->caa_lladdr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	printf(": address %s\n", ether_sprintf(sc->sc_ac.ac_enaddr));

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = che_ioctl;
	ifp->if_start = che_start;
	ifp->if_watchdog = che_watchdog;
	ifp->if_hardmtu = MCLBYTES - ETHER_HDR_LEN - ETHER_CRC_LEN; /* XXX */
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 400);
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    che_ifmedia_upd, che_ifmedia_sts);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = che_miibus_ind_readreg;
	sc->sc_mii.mii_writereg = che_miibus_ind_writereg;
	sc->sc_mii.mii_statchg = che_miibus_statchg;

	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_DOPAUSE | MIIF_HAVEFIBER);

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	return;
}

int
che_write_flash_reg(struct cheg_softc *sc, size_t bcnt, int cont, u_int32_t v)
{
	if (che_read(sc, CHE_REG_SF_OP) & CHE_SF_F_BUSY)
		return (EBUSY);

	che_write(sc, CHE_REG_SF_DATA, v);
	che_write(sc, CHE_REG_SF_OP, CHE_SF_CONT(cont) |
	    CHE_SF_BYTECNT(bcnt - 1) | CHE_SF_F_OP);

	return (che_waitfor(sc, CHE_REG_SF_OP, CHE_SF_F_BUSY, 5));
}

int
che_read_flash_reg(struct cheg_softc *sc, size_t bcnt, int cont, u_int32_t *vp)
{
	if (che_read(sc, CHE_REG_SF_OP) & CHE_SF_F_BUSY)
		return (EBUSY);

	che_write(sc, CHE_REG_SF_OP, CHE_SF_CONT(cont) |
	    CHE_SF_BYTECNT(bcnt - 1));

	if (che_waitfor(sc, CHE_REG_SF_OP, CHE_SF_F_BUSY, 5))
		return (EAGAIN);

	*vp = che_read(sc, CHE_REG_SF_DATA);
	return (0);
}

int
che_read_flash_multi4(struct cheg_softc *sc, u_int addr, u_int32_t *datap,
	size_t count)
{
	int rv;

	if (addr + count * sizeof(u_int32_t) > CHE_SF_SIZE || (addr & 3))
		panic("%s: che_read_flash_multi4 bad params", DEVNAME(sc));

	addr = swap32(addr) | CHE_SF_RD_DATA;

	if ((rv = che_write_flash_reg(sc, 4, 1, addr)))
		return (rv);
	if ((rv = che_read_flash_reg(sc, 1, 1, datap)))
		return (rv);

	while (count) {
		if ((rv = che_read_flash_reg(sc, 4, count > 1, datap)))
			return (rv);
		count--;
		datap++;
	}
	return (0);
}

int
che_read_eeprom(struct cheg_softc *sc, struct pci_attach_args *pa,
    pcireg_t addr, pcireg_t *dp)
{
	pcireg_t rv, base; 
	int i = 4;

	if (!pci_get_capability(pa->pa_pc, pa->pa_tag, CHE_PCI_CAP_ID_VPD,
	    &base, NULL)) {
		printf("%s: VPD EEPROM not found\n", 
		    DEVNAME(sc), addr);
		return EIO;
	}

	addr <<= 16;
	pci_conf_write(pa->pa_pc, pa->pa_tag, base, addr);

	while(i--) {
		delay(10);	
		rv = pci_conf_read(pa->pa_pc, pa->pa_tag, base);
		if (rv & CHE_PCI_F_VPD_ADDR)
			break;
	}
	if (!(rv & CHE_PCI_F_VPD_ADDR)) {
		printf("%s: reading EEPROM address 0x%x failed\n", 
		    DEVNAME(sc), addr);
		return EIO;
	}

	*dp = pci_conf_read(pa->pa_pc, pa->pa_tag, base + CHE_PCI_VPD_DATA);
	return (0);
}

int
che_get_vpd(struct cheg_softc *sc, struct pci_attach_args *pa,
    void *vpd, size_t dwords)
{
	pcireg_t dw0, *dw = vpd;
	int i;
	u_int16_t addr;

	/*
	 * Card information is normally at CHE_PCI_VPD_BASE but some early
	 * cards had it at 0.
	 */
	if (che_read_eeprom(sc, pa, CHE_PCI_VPD_BASE, &dw0))
		return (1);

	/* we compare the id_tag which is least significant byte */
	addr = ((dw0 & 0xff) == 0x82) ? CHE_PCI_VPD_BASE : 0;

	for (i = 0; i < dwords; i++) {
		if (che_read_eeprom(sc, pa, addr + i * 4, &dw[i]))
			return (1);
	}

	return (0);
}

/*
 * VPD mac addr is stored as ASCII string so we need to convert it to a
 * sane representation form.
 */
void
che_conv_lladdr(char *mac, u_int8_t *lladdr)
{
	int i;
	u_int8_t digit;

	bzero(lladdr, ETHER_ADDR_LEN);

	for (i = 0; i < ETHER_ADDR_LEN * 2; i++) {
		if (mac[i] >= '0' && mac[i] <= '9')
			digit = mac[i] - '0';
		else if (mac[i] >= 'A' && mac[i] <= 'F')
			digit = mac[i] - 'A' + 10;
		else if (mac[i] >= 'a' && mac[i] <= 'f')
			digit = mac[i] - 'a' + 10;

		if ((i & 1) == 0)
			digit <<= 4;

		lladdr[i/2] |= digit;
	}
}

u_int32_t
che_conv_num(char *num, size_t len)
{
	size_t i;
	u_int32_t n = 0;

	for (i = 0; i < len; i++) {
		if (num[i] >= '0' && num[i] <= '9')
			n = 10 * n + (num[i] - '0');
		else
			break;
	}
	return (n);
}

void
che_reset(struct cheg_softc *sc)
{
	che_write(sc, CHE_REG_PL_RST, CHE_RST_F_CRSTWRM |
	    CHE_RST_F_CRSTWRMMODE);

	/* Give the card some time to boot */
	delay(500);
}

int
che_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	return (EIO);
}

void
che_watchdog(struct ifnet *ifp)
{
	/* XXX */
}

void
che_start(struct ifnet *ifp)
{
	/* XXX */
}

int
che_ifmedia_upd(struct ifnet *ifp)
{
	struct che_softc *sc = ifp->if_softc;

	mii_mediachg(&sc->sc_mii);
	return (0);
}

void
che_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct che_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

int
che_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct che_softc *sc = (struct che_softc *)dev; 
	u_int32_t addr = CHE_MI1_PHYADDR(phy) | reg;

	che_write(sc->sc_cheg, CHE_REG_MI1_ADDR, addr);
	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(2));

	if (che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20))
		return (0);

	return ((int)che_read(sc->sc_cheg, CHE_REG_MI1_DATA));
}

void
che_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct che_softc *sc = (struct che_softc *)dev; 
	u_int32_t addr = CHE_MI1_PHYADDR(phy) | reg;

	che_write(sc->sc_cheg, CHE_REG_MI1_ADDR, addr);
	che_write(sc->sc_cheg, CHE_REG_MI1_DATA, val);
	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(1));
	che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20);
}

int
che_miibus_ind_readreg(struct device *dev, int phy, int reg)
{
	struct che_softc *sc = (struct che_softc *)dev; 

	che_write(sc->sc_cheg, CHE_REG_MI1_ADDR, CHE_MI1_PHYADDR(phy));
	che_write(sc->sc_cheg, CHE_REG_MI1_DATA, reg);
	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(0));

	if (che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20))
		return (0);

	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(3));

	if (che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20))
		return (0);

	return ((int)che_read(sc->sc_cheg, CHE_REG_MI1_DATA));
}

void
che_miibus_ind_writereg(struct device *dev, int phy, int reg, int val)
{
	struct che_softc *sc = (struct che_softc *)dev; 

	che_write(sc->sc_cheg, CHE_REG_MI1_ADDR, CHE_MI1_PHYADDR(phy));
	che_write(sc->sc_cheg, CHE_REG_MI1_DATA, reg);
	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(0));

	if (che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20))
		return;

	che_write(sc->sc_cheg, CHE_REG_MI1_DATA, val);
	che_write(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_OP(1));

	che_waitfor(sc->sc_cheg, CHE_REG_MI1_OP, CHE_MI1_F_BUSY, 20);
}

void 
che_miibus_statchg(struct device *dev)   
{
	struct che_softc *sc = (struct che_softc *)dev;
	//struct mii_data *mii = &sc->sc_mii;

	printf("%s: che_miibus_statchg\n", DEVNAME(sc));
}

u_int32_t
che_read(struct cheg_softc *sc, bus_size_t r)
{
        bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, r));
}

void
che_write(struct cheg_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_memt, sc->sc_memh, r, v);
        bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
che_waitfor(struct cheg_softc *sc, bus_size_t r, u_int32_t mask, int tries)
{
	u_int32_t v;
	int i;

	for (i = 0; i < tries; i++) {
		v = che_read(sc, r);
		if ((v & mask) == 0)
			return (0);
		delay(10);
	}
	return (EAGAIN);
}

void
che_hw_init(struct cheg_softc *sc)
{
	u_int32_t	mi1_reg;
	u_int32_t	i2c_reg;
	u_int32_t	gpio_reg;
	u_int32_t	port_reg;
	
	mi1_reg = CHE_MI1_F_PREEN |
	    CHE_MI1_CLKDIV(sc->sc_cclk / (2 * sc->sc_mdc) - 1);

	i2c_reg = CHE_I2C_CLKDIV(sc->sc_cclk / 80 - 1);	/* 80KHz */

	gpio_reg =  CHE_T3DBG_F_GPIO0_OEN | CHE_T3DBG_F_GPIO0_OUT_VAL;

	switch (sc->sc_product) {
	case PCI_PRODUCT_CHELSIO_PE9000:
		gpio_reg |= CHE_T3DBG_F_GPIO2_OEN | CHE_T3DBG_F_GPIO2_OUT_VAL |
		    CHE_T3DBG_F_GPIO4_OEN | CHE_T3DBG_F_GPIO4_OUT_VAL;
		port_reg = CHE_XGM_PORTSPEED(2);
		break;
	case PCI_PRODUCT_CHELSIO_T302E:
	case PCI_PRODUCT_CHELSIO_T302X:
	case PCI_PRODUCT_CHELSIO_T3B02:
		gpio_reg |= CHE_T3DBG_F_GPIO2_OEN | CHE_T3DBG_F_GPIO2_OUT_VAL |
		    CHE_T3DBG_F_GPIO4_OEN | CHE_T3DBG_F_GPIO4_OUT_VAL;
		port_reg = CHE_XGM_PORTSPEED(2);
		break;
	case PCI_PRODUCT_CHELSIO_T310E:
	case PCI_PRODUCT_CHELSIO_T310X:
	case PCI_PRODUCT_CHELSIO_T3B10:
		mi1_reg |= CHE_MI1_F_ST;
		gpio_reg |= CHE_T3DBG_F_GPIO1_OEN | CHE_T3DBG_F_GPIO1_OUT_VAL |
		    CHE_T3DBG_F_GPIO6_OEN | CHE_T3DBG_F_GPIO6_OUT_VAL |
		    CHE_T3DBG_F_GPIO7_OEN |
		    CHE_T3DBG_F_GPIO10_OEN | CHE_T3DBG_F_GPIO10_OUT_VAL;
		port_reg = CHE_XGM_PORTSPEED(3);
		port_reg |= CHE_XGM_F_ENRGMII;
		break;
	case PCI_PRODUCT_CHELSIO_T320X:
	case PCI_PRODUCT_CHELSIO_T320E:
	case PCI_PRODUCT_CHELSIO_T3B20:
		mi1_reg |= CHE_MI1_F_ST;
		gpio_reg |= CHE_T3DBG_F_GPIO1_OEN | CHE_T3DBG_F_GPIO1_OUT_VAL |
		    CHE_T3DBG_F_GPIO2_OEN |
		    CHE_T3DBG_F_GPIO4_OEN |
		    CHE_T3DBG_F_GPIO5_OEN | CHE_T3DBG_F_GPIO5_OUT_VAL |
		    CHE_T3DBG_F_GPIO6_OEN | CHE_T3DBG_F_GPIO6_OUT_VAL |
		    CHE_T3DBG_F_GPIO7_OEN |
		    CHE_T3DBG_F_GPIO10_OEN | CHE_T3DBG_F_GPIO10_OUT_VAL |
		    CHE_T3DBG_F_GPIO11_OEN;
		port_reg = CHE_XGM_PORTSPEED(3);
		port_reg |= CHE_XGM_F_ENRGMII;
		break;
	}

	if (sc->sc_rev == 0)
		port_reg |= CHE_XGM_F_ENRGMII;

	/* write all registers */
	che_write(sc, CHE_REG_MI1_CFG, mi1_reg);
	che_write(sc, CHE_REG_I2C_CFG, i2c_reg);
	che_write(sc, CHE_REG_T3DBG_GPIO_EN, gpio_reg);

	che_write(sc, CHE_REG_XGM_PORT_CFG, port_reg);
	(void)che_read(sc, CHE_REG_XGM_PORT_CFG);

	port_reg |= CHE_XGM_F_CLKDIVRESET;

	che_write(sc, CHE_REG_XGM_PORT_CFG, port_reg);
	(void)che_read(sc, CHE_REG_XGM_PORT_CFG);
	che_write(sc, CHE_XGM_REG(CHE_REG_XGM_PORT_CFG, 1), port_reg);
	(void)che_read(sc, CHE_REG_XGM_PORT_CFG);
}
