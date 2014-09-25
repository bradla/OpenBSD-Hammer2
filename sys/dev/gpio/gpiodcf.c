/*	$OpenBSD: gpiodcf.c,v 1.4 2011/07/03 15:47:16 matthew Exp $ */

/*
 * Copyright (c) 2008 Marc Balmer <mbalmer@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/sensors.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiovar.h>

#ifdef GPIODCF_DEBUG
#define DPRINTFN(n, x)	do { if (gpiodcfdebug > (n)) printf x; } while (0)
int gpiodcfdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x)	DPRINTFN(0, x)

#define DPERIOD1	((long) 5 * 60)		/* degrade OK -> WARN */
#define DPERIOD2	((long) 15 * 60)	/* degrade WARN -> CRIT */

/* max. skew of received time diff vs. measured time diff in percent. */
#define MAX_SKEW	5

#define CLOCK_DCF77	0
#define CLOCK_HBG	1

#define GPIODCF_NPINS		1
#define	GPIODCF_PIN_DATA	0

static const char	*clockname[2] = {
	"DCF77",
	"HBG" };

struct gpiodcf_softc {
	struct device		sc_dev;		/* base device */
	void			*sc_gpio; 
	struct gpio_pinmap	sc_map;
	int			__map[GPIODCF_NPINS];
	u_char			sc_dying;	/* disconnecting */
	int			sc_data;

	struct timeout		sc_to;

	struct timeout		sc_bv_to;	/* bit-value detect */
	struct timeout		sc_db_to;	/* debounce */
	struct timeout		sc_mg_to;	/* minute-gap detect */
	struct timeout		sc_sl_to;	/* signal-loss detect */
	struct timeout		sc_it_to;	/* invalidate time */
	struct timeout		sc_ct_to;	/* detect clock type */

	int			sc_detect_ct;	/* != 0: autodetect type */
	int			sc_clocktype;	/* DCF77 or HBG */
	int			sc_sync;	/* 1 during sync */
	u_int64_t		sc_mask;	/* 64 bit mask */
	u_int64_t		sc_tbits;	/* Time bits */
	int			sc_minute;
	int			sc_level;
	time_t			sc_last_mg;
	time_t			sc_current;	/* current time */
	time_t			sc_next;	/* time to become valid next */
	time_t			sc_last;
	int			sc_nrecv;	/* consecutive valid times */
	struct timeval		sc_last_tv;	/* uptime of last valid time */
	struct ksensor		sc_sensor;
#ifdef GPIODCF_DEBUG
	struct ksensor		sc_skew;	/* recv vs local skew */
#endif
	struct ksensordev	sc_sensordev;
};

/*
 * timeouts being used in hz:
 * t_bv		bit value detection (150ms)
 * t_ct		detect clocktype (250ms)
 * t_sync	sync (950ms)
 * t_mg		minute gap detection (1500ms)
 * t_mgsync	resync after a minute gap (450ms)
 * t_sl		detect signal loss (3sec)
 * t_wait	wait (5sec)
 * t_warn	degrade sensor status to warning (5min)
 * t_crit	degrade sensor status to critical (15min)
 */
static int t_bv, t_ct, t_sync, t_mg, t_sl, t_mgsync, t_wait, t_warn, t_crit;

void	gpiodcf_intr(void *);
void	gpiodcf_probe(void *);
void	gpiodcf_bv_probe(void *);
void	gpiodcf_mg_probe(void *);
void	gpiodcf_sl_probe(void *);
void	gpiodcf_ct_probe(void *);
void	gpiodcf_invalidate(void *);

int gpiodcf_match(struct device *, void *, void *); 
void gpiodcf_attach(struct device *, struct device *, void *); 
int gpiodcf_detach(struct device *, int); 
int gpiodcf_activate(struct device *, int); 

int gpiodcf_signal(struct gpiodcf_softc *);

struct cfdriver gpiodcf_cd = {
	NULL, "gpiodcf", DV_DULL
};

const struct cfattach gpiodcf_ca = {
	sizeof(struct gpiodcf_softc),
	gpiodcf_match,
	gpiodcf_attach,
	gpiodcf_detach,
	gpiodcf_activate
};

int
gpiodcf_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct gpio_attach_args *ga = aux;

	if (ga->ga_offset == -1)
		return 0;

	return (strcmp(cf->cf_driver->cd_name, "gpiodcf") == 0);
}

void
gpiodcf_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpiodcf_softc		*sc = (struct gpiodcf_softc *)self;
	struct gpio_attach_args		*ga = aux;
	struct timeval			 t;
	int				 caps;

	if (gpio_npins(ga->ga_mask) != GPIODCF_NPINS) {
		printf(": invalid pin mask\n");
		return;
	}
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->__map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		printf(": can't map pins\n");
		return;
	}

	caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, GPIODCF_PIN_DATA);
	if (!(caps & GPIO_PIN_INPUT)) {
		printf(": data pin is unable to receive input\n");
		goto fishy;
	}
	printf(": DATA[%d]", sc->sc_map.pm_map[GPIODCF_PIN_DATA]);
	sc->sc_data = GPIO_PIN_INPUT;
	gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIODCF_PIN_DATA, sc->sc_data);
	printf("\n");

	sc->sc_detect_ct = 1;
	strlcpy(sc->sc_sensor.desc, "Unknown",
	    sizeof(sc->sc_sensor.desc));

	timeout_set(&sc->sc_to, gpiodcf_probe, sc);
	timeout_set(&sc->sc_bv_to, gpiodcf_bv_probe, sc);
	timeout_set(&sc->sc_mg_to, gpiodcf_mg_probe, sc);
	timeout_set(&sc->sc_sl_to, gpiodcf_sl_probe, sc);
	timeout_set(&sc->sc_it_to, gpiodcf_invalidate, sc);
	timeout_set(&sc->sc_ct_to, gpiodcf_ct_probe, sc);

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	sc->sc_sensor.type = SENSOR_TIMEDELTA;
	sc->sc_sensor.status = SENSOR_S_UNKNOWN;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);

#ifdef GPIODCF_DEBUG
	sc->sc_skew.type = SENSOR_TIMEDELTA;
	sc->sc_skew.status = SENSOR_S_UNKNOWN;
	strlcpy(sc->sc_skew.desc, "local clock skew",
	    sizeof(sc->sc_skew.desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_skew);
#endif
	sensordev_install(&sc->sc_sensordev);

	sc->sc_clocktype = -1;
	sc->sc_level = 0;
	sc->sc_minute = 0;
	sc->sc_last_mg = 0L;

	sc->sc_sync = 1;

	sc->sc_current = 0L;
	sc->sc_next = 0L;
	sc->sc_nrecv = 0;
	sc->sc_last = 0L;
	sc->sc_last_tv.tv_sec = 0L;

	/* convert timevals to hz */
	t.tv_sec = 0L;
	t.tv_usec = 150000L;
	t_bv = tvtohz(&t);

	t.tv_usec = 450000L;
	t_mgsync = tvtohz(&t);

	t.tv_usec = 950000L;
	t_sync = tvtohz(&t);

	t.tv_sec = 1L;
	t.tv_usec = 500000L;
	t_mg = tvtohz(&t);

	t.tv_sec = 3L;
	t.tv_usec = 0L;
	t_sl = tvtohz(&t);
	
	t.tv_sec = 5L;
	t_wait = tvtohz(&t);

	t.tv_sec = DPERIOD1;
	t_warn = tvtohz(&t);

	t.tv_sec = DPERIOD2;
	t_crit = tvtohz(&t);

	if (sc->sc_detect_ct) {
		t.tv_sec = 0L;
		t.tv_usec = 250000L;
		t_ct = tvtohz(&t);
	}

	/* Give the receiver some slack to stabilize */
	timeout_add(&sc->sc_to, t_wait);

	/* Detect signal loss */
	timeout_add(&sc->sc_sl_to, t_wait + t_sl);

	DPRINTF(("synchronizing\n"));
	return;

fishy:
	DPRINTF(("gpiodcf_attach failed\n"));
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
	sc->sc_dying = 1;
}

int
gpiodcf_detach(struct device *self, int flags)
{
	struct gpiodcf_softc	*sc = (struct gpiodcf_softc *)self;

	sc->sc_dying = 1;

	timeout_del(&sc->sc_to);
	timeout_del(&sc->sc_bv_to);
	timeout_del(&sc->sc_mg_to);
	timeout_del(&sc->sc_sl_to);
	timeout_del(&sc->sc_it_to);
	timeout_del(&sc->sc_ct_to);

	/* Unregister the clock with the kernel */
	sensordev_deinstall(&sc->sc_sensordev);

	/* Finally unmap the GPIO pin */
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);

	return 0;
}

/*
 * return 1 during high-power-, 0 during low-power-emission
 * If bit 0 is set, the transmitter emits at full power.
 * During the low-power emission we decode a zero bit.
 */
int
gpiodcf_signal(struct gpiodcf_softc *sc)
{
	return (gpio_pin_read(sc->sc_gpio, &sc->sc_map, GPIODCF_PIN_DATA) ==
	    GPIO_PIN_HIGH ? 1 : 0);
}

/* gpiodcf_probe runs in a process context. */
void
gpiodcf_probe(void *xsc)
{
	struct gpiodcf_softc	*sc = xsc;
	struct timespec		 now;
	int			 data;

	if (sc->sc_dying)
		return;

	data = gpiodcf_signal(sc);
	if (data == -1)
		return;

	if (data) {
		sc->sc_level = 1;
		timeout_add(&sc->sc_to, 1);
		return;
	}

	if (sc->sc_level == 0)
		return;

	/* the beginning of a second */
	sc->sc_level = 0;
	if (sc->sc_minute == 1) {
		if (sc->sc_sync) {
			DPRINTF(("start collecting bits\n"));
			sc->sc_sync = 0;
			if (sc->sc_sensor.status == SENSOR_S_UNKNOWN &&
			    sc->sc_detect_ct)
				sc->sc_clocktype = -1;
		} else {
			/* provide the timedelta */
			microtime(&sc->sc_sensor.tv);
			nanotime(&now);
			sc->sc_current = sc->sc_next;
			sc->sc_sensor.value = (int64_t)(now.tv_sec -
			    sc->sc_current) * 1000000000LL + now.tv_nsec;

			/* set the clocktype and make sensor valid */
			if (sc->sc_sensor.status == SENSOR_S_UNKNOWN &&
			    sc->sc_detect_ct) {
				strlcpy(sc->sc_sensor.desc, sc->sc_clocktype ?
				    clockname[CLOCK_HBG] :
				    clockname[CLOCK_DCF77],
				    sizeof(sc->sc_sensor.desc));
			}
			sc->sc_sensor.status = SENSOR_S_OK;

			/*
			 * if no valid time information is received
			 * during the next 5 minutes, the sensor state
			 * will be degraded to SENSOR_S_WARN
			 */
			timeout_add(&sc->sc_it_to, t_warn);
		}
		sc->sc_minute = 0;
	}

	timeout_add(&sc->sc_to, t_sync);	/* resync in 950 ms */

	/* no clock and bit detection during sync */
	if (!sc->sc_sync) {
		/* detect bit value */
		timeout_add(&sc->sc_bv_to, t_bv);

		/* detect clocktype */
		if (sc->sc_detect_ct && sc->sc_clocktype == -1)
			timeout_add(&sc->sc_ct_to, t_ct);
	}
	timeout_add(&sc->sc_mg_to, t_mg);	/* detect minute gap */
	timeout_add(&sc->sc_sl_to, t_sl);	/* detect signal loss */
}

/* detect the bit value */
void
gpiodcf_bv_probe(void *xsc)
{
	struct gpiodcf_softc	*sc = xsc;
	int			 data;

	if (sc->sc_dying)
		return;

	data = gpiodcf_signal(sc);
	if (data == -1) {
		DPRINTF(("bit detection failed\n"));
		return;
	}	

	DPRINTFN(1, (data ? "0" : "1"));
	if (!(data))
		sc->sc_tbits |= sc->sc_mask;
	sc->sc_mask <<= 1;
}

/* detect the minute gap */
void
gpiodcf_mg_probe(void *xsc)
{
	struct gpiodcf_softc	*sc = xsc;
	struct clock_ymdhms	 ymdhm;
	struct timeval		 monotime;
	int			 tdiff_recv, tdiff_local;
	int			 skew;
	int			 minute_bits, hour_bits, day_bits;
	int			 month_bits, year_bits, wday;
	int			 p1, p2, p3;
	int			 p1_bit, p2_bit, p3_bit;
	int			 r_bit, a1_bit, a2_bit, z1_bit, z2_bit;
	int			 s_bit, m_bit;
	u_int32_t		 parity = 0x6996;

	if (sc->sc_sync) {
		sc->sc_minute = 1;
		goto cleanbits;
	}

	if (time_second - sc->sc_last_mg < 57) {
		DPRINTF(("\nunexpected gap, resync\n"));
		sc->sc_sync = sc->sc_minute = 1;
		goto cleanbits;	
	}

	/* extract bits w/o parity */
	m_bit = sc->sc_tbits & 1;
	r_bit = sc->sc_tbits >> 15 & 1;
	a1_bit = sc->sc_tbits >> 16 & 1;
	z1_bit = sc->sc_tbits >> 17 & 1;
	z2_bit = sc->sc_tbits >> 18 & 1;
	a2_bit = sc->sc_tbits >> 19 & 1;
	s_bit = sc->sc_tbits >> 20 & 1;
	p1_bit = sc->sc_tbits >> 28 & 1;
	p2_bit = sc->sc_tbits >> 35 & 1;
	p3_bit = sc->sc_tbits >> 58 & 1;

	minute_bits = sc->sc_tbits >> 21 & 0x7f;	
	hour_bits = sc->sc_tbits >> 29 & 0x3f;
	day_bits = sc->sc_tbits >> 36 & 0x3f;
	wday = (sc->sc_tbits >> 42) & 0x07;
	month_bits = sc->sc_tbits >> 45 & 0x1f;
	year_bits = sc->sc_tbits >> 50 & 0xff;

	/* validate time information */
	p1 = (parity >> (minute_bits & 0x0f) & 1) ^
	    (parity >> (minute_bits >> 4) & 1);

	p2 = (parity >> (hour_bits & 0x0f) & 1) ^
	    (parity >> (hour_bits >> 4) & 1);

	p3 = (parity >> (day_bits & 0x0f) & 1) ^
	    (parity >> (day_bits >> 4) & 1) ^
	    ((parity >> wday) & 1) ^ (parity >> (month_bits & 0x0f) & 1) ^
	    (parity >> (month_bits >> 4) & 1) ^
	    (parity >> (year_bits & 0x0f) & 1) ^
	    (parity >> (year_bits >> 4) & 1);

	if (m_bit == 0 && s_bit == 1 && p1 == p1_bit && p2 == p2_bit &&
	    p3 == p3_bit && (z1_bit ^ z2_bit)) {

		/* Decode time */
		if ((ymdhm.dt_year = 2000 + FROMBCD(year_bits)) > 2037) {
			DPRINTF(("year out of range, resync\n"));
			sc->sc_sync = 1;
			goto cleanbits;
		}
		ymdhm.dt_min = FROMBCD(minute_bits);
		ymdhm.dt_hour = FROMBCD(hour_bits);
		ymdhm.dt_day = FROMBCD(day_bits);
		ymdhm.dt_mon = FROMBCD(month_bits);
		ymdhm.dt_sec = 0;

		sc->sc_next = clock_ymdhms_to_secs(&ymdhm);
		getmicrouptime(&monotime);

		/* convert to coordinated universal time */
		sc->sc_next -= z1_bit ? 7200 : 3600;

		DPRINTF(("\n%02d.%02d.%04d %02d:%02d:00 %s",
		    ymdhm.dt_day, ymdhm.dt_mon, ymdhm.dt_year,
		    ymdhm.dt_hour, ymdhm.dt_min, z1_bit ? "CEST" : "CET"));
		DPRINTF((r_bit ? ", call bit" : ""));
		DPRINTF((a1_bit ? ", dst chg ann." : ""));
		DPRINTF((a2_bit ? ", leap sec ann." : ""));
		DPRINTF(("\n"));

		if (sc->sc_last) {
			tdiff_recv = sc->sc_next - sc->sc_last;
			tdiff_local = monotime.tv_sec - sc->sc_last_tv.tv_sec;
			skew = abs(tdiff_local - tdiff_recv);
#ifdef GPIODCF_DEBUG
			if (sc->sc_skew.status == SENSOR_S_UNKNOWN)
				sc->sc_skew.status = SENSOR_S_CRIT;
			sc->sc_skew.value = skew * 1000000000LL;
			getmicrotime(&sc->sc_skew.tv);
#endif
			DPRINTF(("local = %d, recv = %d, skew = %d\n",
			    tdiff_local, tdiff_recv, skew));

			if (skew && skew * 100LL / tdiff_local > MAX_SKEW) {
				DPRINTF(("skew out of tolerated range\n"));
				goto cleanbits;
			} else {
				if (sc->sc_nrecv < 2) {
					sc->sc_nrecv++;
					DPRINTF(("got frame %d\n",
					    sc->sc_nrecv));
				} else {
					DPRINTF(("data is valid\n"));
					sc->sc_minute = 1;
				}
			}
		} else {
			DPRINTF(("received the first frame\n"));
			sc->sc_nrecv = 1;
		}

		/* record the time received and when it was received */
		sc->sc_last = sc->sc_next;
		sc->sc_last_tv.tv_sec = monotime.tv_sec;
	} else {
		DPRINTF(("\nparity error, resync\n"));
		sc->sc_sync = sc->sc_minute = 1;
	}

cleanbits:
	timeout_add(&sc->sc_to, t_mgsync);	/* re-sync in 450 ms */
	sc->sc_last_mg = time_second;
	sc->sc_tbits = 0LL;
	sc->sc_mask = 1LL;
}

/* detect signal loss */
void
gpiodcf_sl_probe(void *xsc)
{
	struct gpiodcf_softc *sc = xsc;

	if (sc->sc_dying)
		return;

	DPRINTF(("no signal\n"));
	sc->sc_sync = 1;
	timeout_add(&sc->sc_to, t_wait);
	timeout_add(&sc->sc_sl_to, t_wait + t_sl);
}

/* invalidate timedelta (called in an interrupt context) */
void
gpiodcf_invalidate(void *xsc)
{
	struct gpiodcf_softc *sc = xsc;

	if (sc->sc_dying)
		return;

	if (sc->sc_sensor.status == SENSOR_S_OK) {
		sc->sc_sensor.status = SENSOR_S_WARN;
		/*
		 * further degrade in 15 minutes if we dont receive any new
		 * time information
		 */
		timeout_add(&sc->sc_it_to, t_crit);
	} else {
		sc->sc_sensor.status = SENSOR_S_CRIT;
		sc->sc_nrecv = 0;
	}
}

/* detect clock type.  used for older devices only. */
void
gpiodcf_ct_probe(void *xsc)
{
	struct gpiodcf_softc	*sc = xsc;
	int			 data;

	if (sc->sc_dying)
		return;

	data = gpiodcf_signal(sc);
	if (data == -1) {
		DPRINTF(("clocktype detection failed\n"));
		return;
	}

	sc->sc_clocktype = data ? 0 : 1;
	DPRINTF(("\nclocktype is %s\n", sc->sc_clocktype ?
		clockname[CLOCK_HBG] : clockname[CLOCK_DCF77]));
}

int
gpiodcf_activate(struct device *self, int act)
{
	struct gpiodcf_softc *sc = (struct gpiodcf_softc *)self;

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return 0;
}
