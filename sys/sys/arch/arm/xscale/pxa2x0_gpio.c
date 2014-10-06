/*	$OpenBSD: pxa2x0_gpio.c,v 1.22 2010/09/20 06:33:47 matthew Exp $ */
/*	$NetBSD: pxa2x0_gpio.c,v 1.2 2003/07/15 00:24:55 lukem Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/cpufunc.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

struct gpio_irq_handler {
	struct gpio_irq_handler *gh_next;
	int (*gh_func)(void *);
	void *gh_arg;
	int gh_spl;
	u_int gh_gpio;
	int gh_level;
	int gh_irq;
	struct evcount gh_count;
};

struct pxagpio_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_space_handle_t sc_bush;
	void *sc_irqcookie[4];
	u_int32_t sc_mask[3];
#ifdef PXAGPIO_HAS_GPION_INTRS
	struct gpio_irq_handler *sc_handlers[GPIO_NPINS];
	int sc_minipl;
	int sc_maxipl;
#else
	struct gpio_irq_handler *sc_handlers[2];
#endif
	int npins;
	int pxa27x_pins;
};

int	pxagpio_match(struct device *, void *, void *);
void	pxagpio_attach(struct device *, struct device *, void *);

#ifdef __NetBSD__
CFATTACH_DECL(pxagpio, sizeof(struct pxagpio_softc),
    pxagpio_match, pxagpio_attach, NULL, NULL);
#else
struct cfattach pxagpio_ca = {
        sizeof (struct pxagpio_softc), pxagpio_match, pxagpio_attach
};
	 
struct cfdriver pxagpio_cd = {
	NULL, "pxagpio", DV_DULL
};

#endif

static struct pxagpio_softc *pxagpio_softc;
static vaddr_t pxagpio_regs;
#define GPIO_BOOTSTRAP_REG(reg)	\
	(*((volatile u_int32_t *)(pxagpio_regs + (reg))))

void pxa2x0_gpio_set_intr_level(u_int, int);
int pxagpio_intr0(void *);
int pxagpio_intr1(void *);
#ifdef PXAGPIO_HAS_GPION_INTRS
int pxagpio_dispatch(struct pxagpio_softc *, int);
int pxagpio_intrN(void *);
int pxagpio_intrlow(void *);
void pxa2x0_gpio_intr_fixup(int minipl, int maxipl);
#endif
u_int32_t pxagpio_reg_read(struct pxagpio_softc *sc, int reg);
void pxagpio_reg_write(struct pxagpio_softc *sc, int reg, u_int32_t val);

u_int32_t
pxagpio_reg_read(struct pxagpio_softc *sc, int reg)
{
	if (__predict_true(sc != NULL))
		return (bus_space_read_4(sc->sc_bust, sc->sc_bush, reg));
	else
	if (pxagpio_regs)
		return (GPIO_BOOTSTRAP_REG(reg));
	panic("pxagpio_reg_read: not bootstrapped");
}

void
pxagpio_reg_write(struct pxagpio_softc *sc, int reg, u_int32_t val)
{
	if (__predict_true(sc != NULL))
		bus_space_write_4(sc->sc_bust, sc->sc_bush, reg, val);
	else
	if (pxagpio_regs)
		GPIO_BOOTSTRAP_REG(reg) = val;
	else
		panic("pxagpio_reg_write: not bootstrapped");
	return;
}

int
pxagpio_match(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;

	if (pxagpio_softc != NULL || pxa->pxa_addr != PXA2X0_GPIO_BASE)
		return (0);

	pxa->pxa_size = PXA2X0_GPIO_SIZE;

	return (1);
}

void
pxagpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxagpio_softc *sc = (struct pxagpio_softc *)self;
	struct pxaip_attach_args *pxa = aux;

	sc->sc_bust = pxa->pxa_iot;

	printf(": GPIO Controller\n");

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		sc->npins = GPIO_NPINS;
		sc->pxa27x_pins = 1;
	} else  {
		sc->npins = GPIO_NPINS_25x;
		sc->pxa27x_pins = 0;
	}

	if (bus_space_map(sc->sc_bust, pxa->pxa_addr, pxa->pxa_size, 0,
	    &sc->sc_bush)) {
		printf("%s: Can't map registers!\n", sc->sc_dev.dv_xname);
		return;
	}

	memset(sc->sc_handlers, 0, sizeof(sc->sc_handlers));

	/*
	 * Disable all GPIO interrupts
	 */
	pxagpio_reg_write(sc, GPIO_GRER0, 0);
	pxagpio_reg_write(sc, GPIO_GRER1, 0);
	pxagpio_reg_write(sc, GPIO_GRER2, 0);
	pxagpio_reg_write(sc, GPIO_GRER3, 0);
	pxagpio_reg_write(sc, GPIO_GFER0, 0);
	pxagpio_reg_write(sc, GPIO_GFER1, 0);
	pxagpio_reg_write(sc, GPIO_GFER2, 0);
	pxagpio_reg_write(sc, GPIO_GFER3, 0);
	pxagpio_reg_write(sc, GPIO_GEDR0, ~0);
	pxagpio_reg_write(sc, GPIO_GEDR1, ~0);
	pxagpio_reg_write(sc, GPIO_GEDR2, ~0);
	pxagpio_reg_write(sc, GPIO_GEDR3, ~0);

#ifdef PXAGPIO_HAS_GPION_INTRS
	sc->sc_minipl = IPL_NONE;
	sc->sc_maxipl = IPL_NONE;
#endif

	sc->sc_irqcookie[0] = sc->sc_irqcookie[1] = NULL;

	pxagpio_softc = sc;
}

void
pxa2x0_gpio_bootstrap(vaddr_t gpio_regs)
{

	pxagpio_regs = gpio_regs;
}

void *
pxa2x0_gpio_intr_establish(u_int gpio, int level, int spl, int (*func)(void *),
    void *arg, const char *name)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	struct gpio_irq_handler *gh;
	u_int32_t bit;

#ifdef DEBUG
#ifdef PXAGPIO_HAS_GPION_INTRS
	if (gpio >= sc->npins)
		panic("pxa2x0_gpio_intr_establish: bad pin number: %d", gpio);
#else
	if (gpio > 1)
		panic("pxa2x0_gpio_intr_establish: bad pin number: %d", gpio);
#endif
#endif

	if (GPIO_FN_IS_OUT(pxa2x0_gpio_get_function(gpio)) != GPIO_IN)
		panic("pxa2x0_gpio_intr_establish: Pin %d not GPIO_IN", gpio);

	gh = (struct gpio_irq_handler *)malloc(sizeof(struct gpio_irq_handler),
	    M_DEVBUF, M_NOWAIT);

	gh->gh_func = func;
	gh->gh_arg = arg;
	gh->gh_spl = spl;
	gh->gh_gpio = gpio;
	gh->gh_irq = gpio+32;
	gh->gh_level = level;
	evcount_attach(&gh->gh_count, name, &gh->gh_irq);

	gh->gh_next = sc->sc_handlers[gpio];
	sc->sc_handlers[gpio] = gh;

	if (gpio == 0) {
		KDASSERT(sc->sc_irqcookie[0] == NULL);
		sc->sc_irqcookie[0] = pxa2x0_intr_establish(PXA2X0_INT_GPIO0,
		    spl, pxagpio_intr0, sc, NULL);
		KDASSERT(sc->sc_irqcookie[0]);
	} else if (gpio == 1) {
		KDASSERT(sc->sc_irqcookie[1] == NULL);
		sc->sc_irqcookie[1] = pxa2x0_intr_establish(PXA2X0_INT_GPIO1,
		    spl, pxagpio_intr1, sc, NULL);
		KDASSERT(sc->sc_irqcookie[1]);
	} else {
#ifdef PXAGPIO_HAS_GPION_INTRS
		int minipl, maxipl;
		
		if (sc->sc_maxipl == IPL_NONE || spl > sc->sc_maxipl) {
			maxipl = spl;
		} else {
			maxipl = sc->sc_maxipl;
		}

		
		if (sc->sc_minipl == IPL_NONE || spl < sc->sc_minipl) {
			minipl = spl;
		} else {
			minipl = sc->sc_minipl;
		}
		pxa2x0_gpio_intr_fixup(minipl, maxipl);
#endif
	}

	bit = GPIO_BIT(gpio);
	sc->sc_mask[GPIO_BANK(gpio)] |= bit;

	pxa2x0_gpio_set_intr_level(gpio, gh->gh_level);

	return (gh);
}

void
pxa2x0_gpio_intr_disestablish(void *cookie)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	struct gpio_irq_handler *gh = cookie;
	u_int32_t bit, reg;

	evcount_detach(&gh->gh_count);

	bit = GPIO_BIT(gh->gh_gpio);

	reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GFER0, gh->gh_gpio));
	reg &= ~bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GFER0, gh->gh_gpio), reg);
	reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GRER0, gh->gh_gpio));
	reg &= ~bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GRER0, gh->gh_gpio), reg);

	pxagpio_reg_write(sc, GPIO_REG(GPIO_GEDR0, gh->gh_gpio), bit);

	sc->sc_mask[GPIO_BANK(gh->gh_gpio)] &= ~bit;
	sc->sc_handlers[gh->gh_gpio] = NULL;

	if (gh->gh_gpio == 0) {
		pxa2x0_intr_disestablish(sc->sc_irqcookie[0]);
		sc->sc_irqcookie[0] = NULL;
	} else if (gh->gh_gpio == 1) {
		pxa2x0_intr_disestablish(sc->sc_irqcookie[1]);
		sc->sc_irqcookie[1] = NULL;
	}  else { 
#ifdef PXAGPIO_HAS_GPION_INTRS
		int i, minipl, maxipl, ipl;
		minipl = IPL_HIGH;
		maxipl = IPL_NONE;
		for (i = 2; i < sc->npins; i++) {
			if (sc->sc_handlers[i] != NULL) {
				ipl = sc->sc_handlers[i]->gh_spl;
				if (minipl > ipl)
					minipl = ipl;

				if (maxipl < ipl)
					maxipl = ipl;
			}
		}
		pxa2x0_gpio_intr_fixup(minipl, maxipl);
#endif /* PXAGPIO_HAS_GPION_INTRS */
	}

	free(gh, M_DEVBUF); 
}

#ifdef PXAGPIO_HAS_GPION_INTRS
void
pxa2x0_gpio_intr_fixup(int minipl, int maxipl)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int save = disable_interrupts(I32_bit);

	if (maxipl == IPL_NONE  && minipl == IPL_HIGH) {
		/* no remaining interrupts */
		if (sc->sc_irqcookie[2])
			pxa2x0_intr_disestablish(sc->sc_irqcookie[2]);
		sc->sc_irqcookie[2] = NULL;
		if (sc->sc_irqcookie[3])
			pxa2x0_intr_disestablish(sc->sc_irqcookie[3]);
		sc->sc_irqcookie[3] = NULL;
		sc->sc_minipl = IPL_NONE;
		sc->sc_maxipl = IPL_NONE;
		restore_interrupts(save);
		return;
	}
		
	if (sc->sc_maxipl == IPL_NONE || maxipl > sc->sc_maxipl) {
		if (sc->sc_irqcookie[2])
			pxa2x0_intr_disestablish(sc->sc_irqcookie[2]);

		sc->sc_maxipl = maxipl;
		sc->sc_irqcookie[2] =
		    pxa2x0_intr_establish(PXA2X0_INT_GPION,
		    maxipl, pxagpio_intrN, sc, NULL);

		if (sc->sc_irqcookie[2] == NULL) {
			printf("%s: failed to hook main "
			    "GPIO interrupt\n",
			    sc->sc_dev.dv_xname);
			/* XXX - panic? */
		}
	}
	if (sc->sc_minipl == IPL_NONE || minipl < sc->sc_minipl) {
		if (sc->sc_irqcookie[3])
			pxa2x0_intr_disestablish(sc->sc_irqcookie[3]);

		sc->sc_minipl = minipl;
		sc->sc_irqcookie[3] =
		    pxa2x0_intr_establish(PXA2X0_INT_GPION,
		    sc->sc_minipl, pxagpio_intrlow, sc, NULL);

		if (sc->sc_irqcookie[3] == NULL) {
			printf("%s: failed to hook main "
			    "GPIO interrupt\n",
			    sc->sc_dev.dv_xname);
			/* XXX - panic? */
		}
	}
	restore_interrupts(save);
}
#endif /* PXAGPIO_HAS_GPION_INTRS */

const char *
pxa2x0_gpio_intr_string(void *cookie)
{
	static char irqstr[32];
	struct gpio_irq_handler *gh = cookie;

	if (gh == NULL)
		snprintf(irqstr, sizeof irqstr, "couldn't establish interrupt");
	else 
		snprintf(irqstr, sizeof irqstr, "irq %ld", gh->gh_irq);
	return(irqstr);
}


int
pxagpio_intr0(void *arg)
{
	struct pxagpio_softc *sc = arg;
	int ret;

#ifdef DIAGNOSTIC
	if (sc->sc_handlers[0] == NULL) {
		printf("%s: stray GPIO#0 edge interrupt\n",
		    sc->sc_dev.dv_xname);
		return (0);
	}
#endif

	bus_space_write_4(sc->sc_bust, sc->sc_bush, GPIO_REG(GPIO_GEDR0, 0),
	    GPIO_BIT(0));

	ret = (sc->sc_handlers[0]->gh_func)(sc->sc_handlers[0]->gh_arg);
	if (ret != 0)
		sc->sc_handlers[0]->gh_count.ec_count++;
	return ret;
}

int
pxagpio_intr1(void *arg)
{
	struct pxagpio_softc *sc = arg;
	int ret;

#ifdef DIAGNOSTIC
	if (sc->sc_handlers[1] == NULL) {
		printf("%s: stray GPIO#1 edge interrupt\n",
		    sc->sc_dev.dv_xname);
		return (0);
	}
#endif

	bus_space_write_4(sc->sc_bust, sc->sc_bush, GPIO_REG(GPIO_GEDR0, 1),
	    GPIO_BIT(1));

	ret =  (sc->sc_handlers[1]->gh_func)(sc->sc_handlers[1]->gh_arg);
	if (ret != 0)
		sc->sc_handlers[1]->gh_count.ec_count++;
	return ret;
}

#ifdef PXAGPIO_HAS_GPION_INTRS
int
pxagpio_dispatch(struct pxagpio_softc *sc, int gpio_base)
{
	struct gpio_irq_handler **ghp, *gh;
	int i, s, nhandled, handled, pins;
	u_int32_t gedr, mask;
	int bank;

	/* Fetch bitmap of pending interrupts on this GPIO bank */
	gedr = pxagpio_reg_read(sc, GPIO_REG(GPIO_GEDR0, gpio_base));

	/* Don't handle GPIO 0/1 here */
	if (gpio_base == 0)
		gedr &= ~(GPIO_BIT(0) | GPIO_BIT(1));

	/* Bail early if there are no pending interrupts in this bank */
	if (gedr == 0)
		return (0);

	/* Acknowledge pending interrupts. */
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GEDR0, gpio_base), gedr);

	bank = GPIO_BANK(gpio_base);

	/*
	 * We're only interested in those for which we have a handler
	 * registered
	 */
#ifdef DEBUG
	if ((gedr & sc->sc_mask[bank]) == 0) {
		printf("%s: stray GPIO interrupt. Bank %d, GEDR 0x%08x, mask 0x%08x\n",
		    sc->sc_dev.dv_xname, bank, gedr, sc->sc_mask[bank]);
		return (1);	/* XXX: Pretend we dealt with it */
	}
#endif

	gedr &= sc->sc_mask[bank];
	ghp = &sc->sc_handlers[gpio_base];
	if (sc->pxa27x_pins == 1)
		pins = (gpio_base < 96) ? 32 : 25;
	else 
		pins = (gpio_base < 64) ? 32 : 17;
	handled = 0;

	for (i = 0, mask = 1; i < pins && gedr; i++, ghp++, mask <<= 1) {
		if ((gedr & mask) == 0)
			continue;
		gedr &= ~mask;

		if ((gh = *ghp) == NULL) {
			printf("%s: unhandled GPIO interrupt. GPIO#%d\n",
			    sc->sc_dev.dv_xname, gpio_base + i);
			continue;
		}

		s = _splraise(gh->gh_spl);
		do {
			nhandled = (gh->gh_func)(gh->gh_arg);
			if (nhandled != 0)
				gh->gh_count.ec_count++;
			handled |= nhandled;
			gh = gh->gh_next;
		} while (gh != NULL);
		splx(s);
	}

	return (handled);
}

int
pxagpio_intrN(void *arg)
{
	struct pxagpio_softc *sc = arg;
	int handled;

	handled = pxagpio_dispatch(sc, 0);
	handled |= pxagpio_dispatch(sc, 32);
	handled |= pxagpio_dispatch(sc, 64);
	handled |= pxagpio_dispatch(sc, 96);

	return (handled);
}

int
pxagpio_intrlow(void *arg)
{
	/* dummy */
	return 0;
}
#endif	/* PXAGPIO_HAS_GPION_INTRS */

u_int
pxa2x0_gpio_get_function(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	u_int32_t rv, io;

	if (__predict_true(sc != NULL))
		KDASSERT(gpio < sc->npins);

	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) >> GPIO_FN_SHIFT(gpio);
	rv = GPIO_FN(rv);

	io = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPDR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_OUT;

	io = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPLR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_SET;

	return (rv);
}

u_int
pxa2x0_gpio_set_function(u_int gpio, u_int fn)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	u_int32_t rv, bit;
	u_int oldfn;

	if (__predict_true(sc != NULL))
		KDASSERT(gpio < sc->npins);

	oldfn = pxa2x0_gpio_get_function(gpio);

	if (GPIO_FN(fn) == GPIO_FN(oldfn) &&
	    GPIO_FN_IS_OUT(fn) == GPIO_FN_IS_OUT(oldfn)) {
		/*
		 * The pin's function is not changing.
		 * For Alternate Functions and GPIO input, we can just
		 * return now.
		 * For GPIO output pins, check the initial state is
		 * the same.
		 *
		 * Return 'fn' instead of 'oldfn' so the caller can
		 * reliably detect that we didn't change anything.
		 * (The initial state might be different for non-
		 * GPIO output pins).
		 */
		if (!GPIO_IS_GPIO_OUT(fn) ||
		    GPIO_FN_IS_SET(fn) == GPIO_FN_IS_SET(oldfn))
			return (fn);
	}

	/*
	 * See section 4.1.3.7 of the PXA2x0 Developer's Manual for
	 * the correct procedure for changing GPIO pin functions.
	 */

	bit = GPIO_BIT(gpio);

	/*
	 * 1. Configure the correct set/clear state of the pin
	 */
	if (GPIO_FN_IS_SET(fn))
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GPSR0, gpio), bit);
	else
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GPCR0, gpio), bit);

	/*
	 * 2. Configure the pin as an input or output as appropriate
	 */
	rv = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPDR0, gpio)) & ~bit;
	if (GPIO_FN_IS_OUT(fn))
		rv |= bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GPDR0, gpio), rv);

	/*
	 * 3. Configure the pin's function
	 */
	bit = GPIO_FN_MASK << GPIO_FN_SHIFT(gpio);
	fn = GPIO_FN(fn) << GPIO_FN_SHIFT(gpio);
	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) & ~bit;
	pxagpio_reg_write(sc, GPIO_FN_REG(gpio), rv | fn);

	return (oldfn);
}

/* 
 * Quick function to read pin value
 */
int
pxa2x0_gpio_get_bit(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int bit;

	bit = GPIO_BIT(gpio);
	if (pxagpio_reg_read(sc, GPIO_REG(GPIO_GPLR0, gpio)) & bit)
		return 1;
	else 
		return 0;
}

/* 
 * Quick function to set pin to 1
 */
void
pxa2x0_gpio_set_bit(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int bit;

	bit = GPIO_BIT(gpio);
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GPSR0, gpio), bit);
}

/* 
 * Quick function to set pin to 0
 */
void
pxa2x0_gpio_clear_bit(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int bit;

	bit = GPIO_BIT(gpio);
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GPCR0, gpio), bit);
}

/* 
 * Quick function to change pin direction
 */
void
pxa2x0_gpio_set_dir(u_int gpio, int dir)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int bit;
	u_int32_t reg;

	bit = GPIO_BIT(gpio);

	reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPDR0, gpio)) & ~bit;
	if (GPIO_FN_IS_OUT(dir))
		reg |= bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GPDR0, gpio), reg);
}

/* 
 * Quick function to clear interrupt status on a pin
 * GPIO pins may be toggle in an interrupt and we dont want
 * extra spurious interrupts to occur.
 * Suppose this causes a slight race if a key is pressed while
 * the interrupt handler is running. (yes this is for the keyboard driver)
 */
void
pxa2x0_gpio_clear_intr(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	int bit;

	bit = GPIO_BIT(gpio);
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GEDR0, gpio), bit);
}

/*
 * Quick function to mask (disable) a GPIO interrupt
 */
void
pxa2x0_gpio_intr_mask(void *v)
{
	struct gpio_irq_handler *gh = v;

	pxa2x0_gpio_set_intr_level(gh->gh_gpio, IPL_NONE);
}

/*
 * Quick function to unmask (enable) a GPIO interrupt
 */
void
pxa2x0_gpio_intr_unmask(void *v)
{
	struct gpio_irq_handler *gh = v;

	pxa2x0_gpio_set_intr_level(gh->gh_gpio, gh->gh_level);
}

/*
 * Configure the edge sensitivity of interrupt pins
 */
void
pxa2x0_gpio_set_intr_level(u_int gpio, int level)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	u_int32_t bit;
	u_int32_t gfer;
	u_int32_t grer;
	int s;

	s = splhigh();

	bit = GPIO_BIT(gpio);
	gfer = pxagpio_reg_read(sc, GPIO_REG(GPIO_GFER0, gpio));
	grer = pxagpio_reg_read(sc, GPIO_REG(GPIO_GRER0, gpio));

	switch (level) {
	case IST_NONE:
		gfer &= ~bit;
		grer &= ~bit;
		break;
	case IST_EDGE_FALLING:
		gfer |= bit;
		grer &= ~bit;
		break;
	case IST_EDGE_RISING:
		gfer &= ~bit;
		grer |= bit;
		break;
	case IST_EDGE_BOTH:
		gfer |= bit;
		grer |= bit;
		break;
	default:
		panic("pxa2x0_gpio_set_intr_level: bad level: %d", level);
		break;
	}

	pxagpio_reg_write(sc, GPIO_REG(GPIO_GFER0, gpio), gfer);
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GRER0, gpio), grer);

	splx(s);
}
