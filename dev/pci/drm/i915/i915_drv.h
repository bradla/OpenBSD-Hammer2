/* $OpenBSD: i915_drv.h,v 1.51 2014/03/25 17:44:39 mpi Exp $ */
/* i915_drv.h -- Private header for the I915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _I915_DRV_H_
#define _I915_DRV_H_

#include "i915_reg.h"
#include <dev/pci/drm/i915_drm.h>
#include "intel_bios.h"
#include "intel_ringbuffer.h"

#include <sys/task.h>
#include <dev/pci/vga_pcivar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include "acpi.h"

/* General customization:
 */

#define DRIVER_AUTHOR		"Tungsten Graphics, Inc."

#define DRIVER_NAME		"i915"
#define DRIVER_DESC		"Intel Graphics"
#define DRIVER_DATE		"20080730"

enum pipe {
	PIPE_A = 0,
	PIPE_B,
	PIPE_C,
	I915_MAX_PIPES
};
#define pipe_name(p) ((p) + 'A')

enum transcoder {
	TRANSCODER_A = 0,
	TRANSCODER_B,
	TRANSCODER_C,
	TRANSCODER_EDP = 0xF,
};
#define transcoder_name(t) ((t) + 'A')

enum plane {
	PLANE_A = 0,
	PLANE_B,
	PLANE_C,
};
#define plane_name(p) ((p) + 'A')

enum port {
	PORT_A = 0,
	PORT_B,
	PORT_C,
	PORT_D,
	PORT_E,
	I915_MAX_PORTS
};
#define port_name(p) ((p) + 'A')

#define for_each_pipe(p) for ((p) = 0; (p) < INTEL_INFO(dev)->num_pipes; (p)++)

#define for_each_encoder_on_crtc(dev, __crtc, intel_encoder) \
	list_for_each_entry((intel_encoder), &(dev)->mode_config.encoder_list, base.head) \
		if ((intel_encoder)->base.crtc == (__crtc))

struct intel_pch_pll {
	int refcount; /* count of number of CRTCs sharing this PLL */
	int active; /* count of number of active CRTCs (i.e. DPMS on) */
	bool on; /* is the PLL actually active? Disabled during modeset */
	int pll_reg;
	int fp0_reg;
	int fp1_reg;
};
#define I915_NUM_PLLS 2

struct intel_ddi_plls {
	int spll_refcount;
	int wrpll1_refcount;
	int wrpll2_refcount;
};

/* Interface history:
 *
 * 1.1: Original.
 * 1.2: Add Power Management
 * 1.3: Add vblank support
 * 1.4: Fix cmdbuffer path, add heap destroy
 * 1.5: Add vblank pipe configuration
 * 1.6: - New ioctl for scheduling buffer swaps on vertical blank
 *      - Support vertical blank on secondary display pipe
 */
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		6
#define DRIVER_PATCHLEVEL	0

#define I915_GEM_PHYS_CURSOR_0 1
#define I915_GEM_PHYS_CURSOR_1 2
#define I915_GEM_PHYS_OVERLAY_REGS 3
#define I915_MAX_PHYS_OBJECT (I915_GEM_PHYS_OVERLAY_REGS)

struct drm_i915_gem_phys_object {
	int id;
	struct drm_dmamem *handle;
	struct drm_i915_gem_object *cur_obj;
};

struct inteldrm_softc;

struct drm_i915_display_funcs {
	bool (*fbc_enabled)(struct drm_device *dev);
	void (*enable_fbc)(struct drm_crtc *crtc, unsigned long interval);
	void (*disable_fbc)(struct drm_device *dev);
	int (*get_display_clock_speed)(struct drm_device *dev);
	int (*get_fifo_size)(struct drm_device *dev, int plane);
	void (*update_wm)(struct drm_device *dev);
	void (*update_sprite_wm)(struct drm_device *dev, int pipe,
				 uint32_t sprite_width, int pixel_size);
	void (*update_linetime_wm)(struct drm_device *dev, int pipe,
				 struct drm_display_mode *mode);
	void (*modeset_global_resources)(struct drm_device *dev);
	int (*crtc_mode_set)(struct drm_crtc *crtc,
			     struct drm_display_mode *mode,
			     struct drm_display_mode *adjusted_mode,
			     int x, int y,
			     struct drm_framebuffer *old_fb);
	void (*crtc_enable)(struct drm_crtc *crtc);
	void (*crtc_disable)(struct drm_crtc *crtc);
	void (*off)(struct drm_crtc *crtc);
	void (*write_eld)(struct drm_connector *connector,
			  struct drm_crtc *crtc);
	void (*fdi_link_train)(struct drm_crtc *crtc);
	void (*init_clock_gating)(struct drm_device *dev);
	int (*queue_flip)(struct drm_device *dev, struct drm_crtc *crtc,
			  struct drm_framebuffer *fb,
			  struct drm_i915_gem_object *obj);
	int (*update_plane)(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			    int x, int y);
	/* clock updates for mode set */
	/* cursor updates */
	/* render clock increase/decrease */
	/* display clock increase/decrease */
	/* pll clock increase/decrease */
};

struct drm_i915_gt_funcs {
	void (*force_wake_get)(struct inteldrm_softc *dev_priv);
	void (*force_wake_put)(struct inteldrm_softc *dev_priv);
};

struct intel_device_info {
	u8 num_pipes:3;
	u8 gen;
	u8 is_mobile:1;
	u8 is_i85x:1;
	u8 is_i915g:1;
	u8 is_i945gm:1;
	u8 is_g33:1;
	u8 need_gfx_hws:1;
	u8 is_g4x:1;
	u8 is_pineview:1;
	u8 is_broadwater:1;
	u8 is_crestline:1;
	u8 is_ivybridge:1;
	u8 is_valleyview:1;
	u8 has_force_wake:1;
	u8 is_haswell:1;
	u8 has_fbc:1;
	u8 has_pipe_cxsr:1;
	u8 has_hotplug:1;
	u8 cursor_needs_physical:1;
	u8 has_overlay:1;
	u8 overlay_needs_physical:1;
	u8 supports_tv:1;
	u8 has_bsd_ring:1;
	u8 has_blt_ring:1;
	u8 has_llc:1;
};

/* This must match up with the value previously used for execbuf2.rsvd1. */
#define DEFAULT_CONTEXT_ID 0
struct i915_hw_context {
	int id;
	bool is_initialized;
	struct drm_i915_file_private *file_priv;
	struct intel_ring_buffer *ring;
	struct drm_i915_gem_object *obj;
};

struct i915_ctx_handle {
	SPLAY_ENTRY(i915_ctx_handle)	 entry;
	struct i915_hw_context		*ctx;
	uint32_t			 handle;
};

enum no_fbc_reason {
	FBC_NO_OUTPUT, /* no outputs enabled to compress */
	FBC_STOLEN_TOO_SMALL, /* not enough space to hold compressed buffers */
	FBC_UNSUPPORTED_MODE, /* interlace or doublescanned mode */
	FBC_MODE_TOO_LARGE, /* mode too large for compression */
	FBC_BAD_PLANE, /* fbc not supported on plane */
	FBC_NOT_TILED, /* buffer not tiled */
	FBC_MULTIPLE_PIPES, /* more than one pipe active */
	FBC_MODULE_PARAM,
};

struct opregion_header;
struct opregion_acpi;
struct opregion_swsci;
struct opregion_asle;

struct intel_opregion {
	struct opregion_header *header;
	struct opregion_acpi *acpi;
	struct opregion_swsci *swsci;
	struct opregion_asle *asle;
	void *vbt;
	u32 *lid_state;
};
#define OPREGION_SIZE            (8*1024)


#define I915_FENCE_REG_NONE -1
#define I915_MAX_NUM_FENCES 16
/* 16 fences + sign bit for FENCE_REG_NONE */
#define I915_MAX_NUM_FENCE_BITS 5

struct drm_i915_fence_reg {
	struct list_head lru_list;
	struct drm_i915_gem_object *obj;
	int pin_count;
};

struct sdvo_device_mapping {
	u8 initialized;
	u8 dvo_port;
	u8 slave_addr;
	u8 dvo_wiring;
	u8 i2c_pin;
	u8 ddc_pin;
};

enum intel_pch {
	PCH_NONE = 0,	/* No PCH present */
	PCH_IBX,	/* Ibexpeak PCH */
	PCH_CPT,	/* Cougarpoint PCH */
	PCH_LPT,	/* Lynxpoint PCH */
};

enum intel_sbi_destination {
	SBI_ICLK,
	SBI_MPHY,
};

#define QUIRK_PIPEA_FORCE	(1<<0)
#define QUIRK_LVDS_SSC_DISABLE	(1<<1)
#define QUIRK_INVERT_BRIGHTNESS	(1<<2)
#define QUIRK_NO_PCH_PWM_ENABLE	(1<<3)

struct intel_fbdev;

struct intel_gmbus {
	struct i2c_controller controller;
	u32 port;
	u32 speed;
	u32 force_bit;
	u32 reg0;
	u32 gpio_reg;
	struct inteldrm_softc *dev_priv;
};

struct i915_suspend_saved_registers {
	u8 saveLBB;
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 saveDSPARB;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveBCLRPAT_A;
	u32 saveTRANSACONF;
	u32 saveTRANS_HTOTAL_A;
	u32 saveTRANS_HBLANK_A;
	u32 saveTRANS_HSYNC_A;
	u32 saveTRANS_VTOTAL_A;
	u32 saveTRANS_VBLANK_A;
	u32 saveTRANS_VSYNC_A;
	u32 savePIPEASTAT;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPAADDR;
	u32 saveDSPASURF;
	u32 saveDSPATILEOFF;
	u32 savePFIT_PGM_RATIOS;
	u32 saveBLC_HIST_CTL;
	u32 saveBLC_PWM_CTL;
	u32 saveBLC_PWM_CTL2;
	u32 saveBLC_CPU_PWM_CTL;
	u32 saveBLC_CPU_PWM_CTL2;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveBCLRPAT_B;
	u32 saveTRANSBCONF;
	u32 saveTRANS_HTOTAL_B;
	u32 saveTRANS_HBLANK_B;
	u32 saveTRANS_HSYNC_B;
	u32 saveTRANS_VTOTAL_B;
	u32 saveTRANS_VBLANK_B;
	u32 saveTRANS_VSYNC_B;
	u32 savePIPEBSTAT;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBADDR;
	u32 saveDSPBSURF;
	u32 saveDSPBTILEOFF;
	u32 saveVGA0;
	u32 saveVGA1;
	u32 saveVGA_PD;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 savePP_ON_DELAYS;
	u32 savePP_OFF_DELAYS;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_DIVISOR;
	u32 savePFIT_CONTROL;
	u32 save_palette_a[256];
	u32 save_palette_b[256];
	u32 saveDPFC_CB_BASE;
	u32 saveFBC_CFB_BASE;
	u32 saveFBC_LL_BASE;
	u32 saveFBC_CONTROL;
	u32 saveFBC_CONTROL2;
	u32 saveIER;
	u32 saveIIR;
	u32 saveIMR;
	u32 saveDEIER;
	u32 saveDEIMR;
	u32 saveGTIER;
	u32 saveGTIMR;
	u32 saveFDI_RXA_IMR;
	u32 saveFDI_RXB_IMR;
	u32 saveCACHE_MODE_0;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF2[3];
	u8 saveMSR;
	u8 saveSR[8];
	u8 saveGR[25];
	u8 saveAR_INDEX;
	u8 saveAR[21];
	u8 saveDACMASK;
	u8 saveCR[37];
	uint64_t saveFENCE[I915_MAX_NUM_FENCES];
	u32 saveCURACNTR;
	u32 saveCURAPOS;
	u32 saveCURABASE;
	u32 saveCURBCNTR;
	u32 saveCURBPOS;
	u32 saveCURBBASE;
	u32 saveCURSIZE;
	u32 saveDP_B;
	u32 saveDP_C;
	u32 saveDP_D;
	u32 savePIPEA_GMCH_DATA_M;
	u32 savePIPEB_GMCH_DATA_M;
	u32 savePIPEA_GMCH_DATA_N;
	u32 savePIPEB_GMCH_DATA_N;
	u32 savePIPEA_DP_LINK_M;
	u32 savePIPEB_DP_LINK_M;
	u32 savePIPEA_DP_LINK_N;
	u32 savePIPEB_DP_LINK_N;
	u32 saveFDI_RXA_CTL;
	u32 saveFDI_TXA_CTL;
	u32 saveFDI_RXB_CTL;
	u32 saveFDI_TXB_CTL;
	u32 savePFA_CTL_1;
	u32 savePFB_CTL_1;
	u32 savePFA_WIN_SZ;
	u32 savePFB_WIN_SZ;
	u32 savePFA_WIN_POS;
	u32 savePFB_WIN_POS;
	u32 savePCH_DREF_CONTROL;
	u32 saveDISP_ARB_CTL;
	u32 savePIPEA_DATA_M1;
	u32 savePIPEA_DATA_N1;
	u32 savePIPEA_LINK_M1;
	u32 savePIPEA_LINK_N1;
	u32 savePIPEB_DATA_M1;
	u32 savePIPEB_DATA_N1;
	u32 savePIPEB_LINK_M1;
	u32 savePIPEB_LINK_N1;
	u32 saveMCHBAR_RENDER_STANDBY;
	u32 savePCH_PORT_HOTPLUG;
};

struct intel_gen6_power_mgmt {
	struct task task;
	u32 pm_iir;
	/* lock - irqsave spinlock that protectects the work_struct and
	 * pm_iir. */
	struct mutex lock;

	/* The below variables an all the rps hw state are protected by
	 * dev->struct mutext. */
	u8 cur_delay;
	u8 min_delay;
	u8 max_delay;

	struct task delayed_resume_task;
	struct timeout delayed_resume_to;

	/*
	 * Protects RPS/RC6 register access and PCU communication.
	 * Must be taken after struct_mutex if nested.
	 */
	struct rwlock hw_lock;
};

struct intel_ilk_power_mgmt {
	u8 cur_delay;
	u8 min_delay;
	u8 max_delay;
	u8 fmax;
	u8 fstart;

	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
	struct timespec last_time2;
	unsigned long gfx_power;
	u8 corr;

	int c_m;
	int r_t;

	struct drm_i915_gem_object *pwrctx;
	struct drm_i915_gem_object *renderctx;
};

struct intel_l3_parity {
	u32 *remap_info;
	struct task error_task;
};

/*
 * lock ordering:
 * exec lock,
 * request lock
 * list lock.
 *
 * XXX fence lock ,object lock
 */
struct inteldrm_softc {
	struct device		 dev;
	struct device		*drmdev;
	bus_dma_tag_t		 dmat;
	bus_space_tag_t		 bst;
	struct agp_map		*agph;
	bus_space_handle_t	 opregion_ioh;

	u_long			 flags;

	struct intel_gmbus	 gmbus[GMBUS_NUM_PORTS];

	pci_chipset_tag_t	 pc;
	pcitag_t		 tag;
	pci_intr_handle_t	 ih;
	void			*irqh;

	struct vga_pci_bar	*regs;

	int			 nscreens;
	void			(*switchcb)(void *, int, int);
	void			*switchcbarg;
	struct workq_task	 switchwqt;
	struct rasops_info	 ro;

	int	sc_offset;
	int	(*sc_copyrows)(void *, int, int, int);

	uint32_t		 gpio_mmio_base;

	struct drm_i915_gt_funcs gt;
	/** gt_fifo_count and the subsequent register write are synchronized
	 * with dev->struct_mutex. */
	unsigned gt_fifo_count;
	/** forcewake_count is protected by gt_lock */
	unsigned forcewake_count;
	/** gt_lock is also taken in irq contexts. */
	struct mutex gt_lock;

	drm_i915_sarea_t *sarea_priv;
	struct intel_ring_buffer ring[I915_NUM_RINGS];
	uint32_t next_seqno;

	struct drm_dmamem *status_page_dmah;

	union flush {
		struct {
			bus_space_tag_t		bst;
			bus_space_handle_t	bsh;
		} i9xx;
		struct {
			bus_dma_segment_t	seg;
			caddr_t			kva;
		} i8xx;
	}			 ifp;
	struct vm_page		*pgs;

	/* Protects user_irq_refcount and irq_mask reg */
	struct mutex		 irq_lock;
	/* Refcount for user irq, only enabled when needed */
	int			 user_irq_refcount;
	u_int32_t		 irq_mask;

	/* DPIO indirect register protection */
	struct mutex		 dpio_lock;
	/* Cached value of IMR to avoid reads in updating the bitfield */
	u_int32_t		 pipestat[2];
	/* these two  ironlake only, we should union this with pipestat XXX */
	u_int32_t		 gt_irq_mask;
	u_int32_t		 pch_irq_mask;

	u_int32_t		 hotplug_supported_mask;
	struct task	 	 hotplug_task;

	int			 num_pch_pll;

	struct intel_opregion	 opregion;

	int crt_ddc_pin;

	/* overlay */
        struct intel_overlay *overlay;
	bool sprite_scaling_enabled;

	/* LVDS info */
	int backlight_level;  /* restore backlight to this value */
	bool backlight_enabled;
	struct drm_display_mode *lfp_lvds_vbt_mode; /* if any */
	struct drm_display_mode *sdvo_lvds_vbt_mode; /* if any */

	/* Feature bits from the VBIOS */
	unsigned int int_tv_support:1;
	unsigned int lvds_dither:1;
	unsigned int lvds_vbt:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	unsigned int display_clock_mode:1;
	unsigned int fdi_rx_polarity_inverted:1;
	int lvds_ssc_freq;
	unsigned int bios_lvds_val; /* initial [PCH_]LVDS reg val in VBIOS */
	unsigned int lvds_val; /* used for checking LVDS channel mode */
	struct {
		int rate;
		int lanes;
		int preemphasis;
		int vswing;

		bool initialized;
		bool support;
		int bpp;
		struct edp_power_seq pps;
	} edp;
	bool no_aux_handshake;

	struct intel_pch_pll	 pch_plls[I915_NUM_PLLS];
	struct intel_ddi_plls	 ddi_plls;

	/* Reclocking support */
	bool			 render_reclock_avail;
	bool			 lvds_downclock_avail;
	int			 lvds_downclock;
	bool			 busy;
	int			 child_dev_num;
	struct child_device_config *child_dev;

	struct drm_i915_fence_reg fence_regs[16]; /* 965 */
	int			 fence_reg_start; /* 4 by default */
	int			 num_fence_regs; /* 8 pre-965, 16 post */

	/* number of ioctls + faults in flight */
	int			 entries;

	struct task error_task;

	enum intel_pch pch_type;
	unsigned short pch_id;

	struct drm_i915_display_funcs display;

	struct timeout idle_timeout;

	unsigned long quirks;

	/* Register state */
	bool modeset_on_lid;

	struct {
		/** Bridge to intel-gtt-ko */
		struct intel_gtt *gtt;
		/** Memory allocator for GTT */
		struct drm_mm gtt_space;
		/** List of all objects in gtt_space. Used to restore gtt
		 * mappings on resume */
		struct list_head bound_list;
		/**
		 * List of objects which are not bound to the GTT (thus
		 * are idle and not used by the GPU) but still have
		 * (presumably uncached) pages still attached.
		 */
		struct list_head unbound_list;

		/** Usable portion of the GTT for GEM */
		unsigned long gtt_start;
		unsigned long gtt_mappable_end;
		unsigned long gtt_end;

		/**
		 * List of objects currently involved in rendering from the
		 * ringbuffer.
		 *
		 * Includes buffers having the contents of their GPU caches
		 * flushed, not necessarily primitives. last_rendering_seqno
		 * represents when the rendering involved will be completed.
		 *
		 * A reference is held on the buffer while on this list.
		 */
		struct list_head active_list;

		/**
		 * LRU list of objects which are not in the ringbuffer and
		 * are ready to unbind, but are still in the GTT.
		 *
		 * last_rendering_seqno is 0 while an object is in this list
		 *
		 * A reference is not held on the buffer while on this list,
		 * as merely being GTT-bound shouldn't prevent its being
		 * freed, and we'll pull it off the list in the free path.
		 */
		struct list_head inactive_list;

		/* Fence LRU */
		struct list_head fence_list;

		/**
		 * We leave the user IRQ off as much as possible,
		 * but this means that requests will finish and never
		 * be retired once the system goes idle. Set a timer to
		 * fire periodically while the ring is running. When it
		 * fires, go retire requests in a workq.
		 */
		struct timeout retire_timer;

		/**
		 * Are we in a non-interruptible section of code like
		 * modesetting?
		 */
		bool interruptible;

		/**
		 * Flag if the X Server, and thus DRM, is not currently in
		 * control of the device.
		 *
		 * This is set between LeaveVT and EnterVT.  It needs to be
		 * replaced with a semaphore.  It also needs to be
		 * transitioned away from for kernel modesetting.
		 */
		int suspended;

		/**
		 * Flag if the hardware appears to be wedged.
		 *
		 * This is set when attempts to idle the device timeout.
		 * It prevents command submission from occuring and makes
		 * every pending request fail
		 */
		int wedged;

		/** Bit 6 swizzling required for X tiling */
		uint32_t bit_6_swizzle_x;
		/** Bit 6 swizzling required for Y tiling */
		uint32_t bit_6_swizzle_y;

		/* storage for physical objects */
		struct drm_i915_gem_phys_object *phys_objs[I915_MAX_PHYS_OBJECT];

		/* accounting, useful for userland debugging */
		size_t gtt_total;
		size_t mappable_gtt_total;
		size_t object_memory;
		u32 object_count;

		/* for gem retire handler */
		struct taskq *retire_taskq;
		struct task retire_task;
	} mm;

	/* for hangcheck */
#define DRM_I915_HANGCHECK_PERIOD 1500 /* in ms */
	struct timeout hangcheck_timer;
	int hangcheck_count;
	uint32_t last_acthd[I915_NUM_RINGS];
	uint32_t prev_instdone[I915_NUM_INSTDONE_REG];

	const struct intel_device_info *info;

	int relative_constants_mode;

	struct sdvo_device_mapping sdvo_mappings[2];
	/* indicate whether the LVDS_BORDER should be enabled or not */
	unsigned int lvds_border_bits;
	/* Panel fitter placement and size for Ironlake+ */
	u32 pch_pf_pos, pch_pf_size;

	struct drm_crtc *plane_to_crtc_mapping[3];
	struct drm_crtc *pipe_to_crtc_mapping[3];
	int pending_flip_queue;

	bool flip_pending_is_done;

	struct drm_connector *int_lvds_connector;
	struct drm_connector *int_edp_connector;

	u8 cur_delay;
	u8 min_delay;
	u8 max_delay;
	u8 fmax;
	u8 fstart;

	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
//	struct timespec last_time2;
	unsigned long gfx_power;
	int c_m;
	int r_t;
	u8 corr;

	struct intel_l3_parity l3_parity;

	/* gen6+ rps state */
	struct intel_gen6_power_mgmt rps;

	/* ilk-only ips/rps state. Everything in here is protected by the global
	 * mchdev_lock in intel_pm.c */
	struct intel_ilk_power_mgmt ips;

	enum no_fbc_reason no_fbc_reason;

	unsigned int stop_rings;

	unsigned long cfb_size;
	unsigned int cfb_fb;
	int cfb_plane;
	int cfb_y;
	struct intel_fbc_work *fbc_work;

	unsigned int fsb_freq, mem_freq, is_ddr3;

	int error_completion;
	struct mutex error_completion_lock;

	time_t last_gpu_reset;

	struct intel_fbdev *fbdev;

	struct drm_property *broadcast_rgb_property;
	struct drm_property *force_audio_property;

	bool hw_contexts_disabled;
	uint32_t hw_context_size;

	u32 fdi_rx_config;

	struct i915_suspend_saved_registers regfile;
};
#define drm_i915_private inteldrm_softc
typedef struct drm_i915_private drm_i915_private_t;

/* Iterate over initialised rings */
#define for_each_ring(ring__, dev_priv__, i__) \
	for ((i__) = 0; (i__) < I915_NUM_RINGS; (i__)++) \
		if (((ring__) = &(dev_priv__)->ring[(i__)]), intel_ring_initialized((ring__)))

enum hdmi_force_audio {
	HDMI_AUDIO_OFF_DVI = -2,	/* no aux data for HDMI-DVI converter */
	HDMI_AUDIO_OFF,			/* force turn off HDMI audio */
	HDMI_AUDIO_AUTO,		/* trust EDID */
	HDMI_AUDIO_ON,			/* force turn on HDMI audio */
};

enum i915_cache_level {
	I915_CACHE_NONE,
	I915_CACHE_LLC,
	I915_CACHE_LLC_MLC, /* gen6+ */
};

struct drm_i915_gem_object_ops {
	/* Interface between the GEM object and its backing storage.
	 * get_pages() is called once prior to the use of the associated set
	 * of pages before to binding them into the GTT, and put_pages() is
	 * called after we no longer need them. As we expect there to be
	 * associated cost with migrating pages between the backing storage
	 * and making them available for the GPU (e.g. clflush), we may hold
	 * onto the pages after they are no longer referenced by the GPU
	 * in case they may be used again shortly (for example migrating the
	 * pages to a different memory domain within the GTT). put_pages()
	 * will therefore most likely be called when the object itself is
	 * being released or under memory pressure (where we attempt to
	 * reap pages for the shrinker).
	 */
	int (*get_pages)(struct drm_i915_gem_object *);
	void (*put_pages)(struct drm_i915_gem_object *);
};

struct inteldrm_file {
	struct drm_file	file_priv;
	struct {
	} mm;
};

/* chip type flags */
#define CHIP_I830	0x00001
#define CHIP_I845G	0x00002
#define CHIP_I85X	0x00004
#define CHIP_I865G	0x00008
#define CHIP_I9XX	0x00010
#define CHIP_I915G	0x00020
#define CHIP_I915GM	0x00040
#define CHIP_I945G	0x00080
#define CHIP_I945GM	0x00100
#define CHIP_I965	0x00200
#define CHIP_I965GM	0x00400
#define CHIP_G33	0x00800
#define CHIP_GM45	0x01000
#define CHIP_G4X	0x02000
#define CHIP_M		0x04000
#define CHIP_HWS	0x08000
#define CHIP_GEN2	0x10000
#define CHIP_GEN3	0x20000
#define CHIP_GEN4	0x40000
#define CHIP_GEN6	0x80000
#define	CHIP_PINEVIEW	0x100000
#define	CHIP_IRONLAKE	0x200000
#define CHIP_IRONLAKE_D	0x400000
#define CHIP_IRONLAKE_M	0x800000
#define CHIP_SANDYBRIDGE	0x1000000
#define CHIP_IVYBRIDGE	0x2000000
#define CHIP_GEN7	0x4000000

/** driver private structure attached to each drm_gem_object */
struct drm_i915_gem_object {
	struct drm_gem_object base;

	const struct drm_i915_gem_object_ops *ops;

	/** Current space allocated to this object in the GTT, if any. */
	struct drm_mm_node *gtt_space;
	struct list_head gtt_list;

	/** This object's place on the active/flushing/inactive lists */
	struct list_head ring_list;
	struct list_head mm_list;
	/** This object's place in the batchbuffer or on the eviction list */
	struct list_head exec_list;

	/**
	 * This is set if the object is on the active lists (has pending
	 * rendering and so a non-zero seqno), and is not set if it i s on
	 * inactive (ready to be unbound) list.
	 */
	unsigned int active:1;

	/**
	 * This is set if the object has been written to since last bound
	 * to the GTT
	 */
	unsigned int dirty:1;

	/**
	 * Fence register bits (if any) for this object.  Will be set
	 * as needed when mapped into the GTT.
	 * Protected by dev->struct_mutex.
	 */
	signed int fence_reg:I915_MAX_NUM_FENCE_BITS;

	/**
	 * Advice: are the backing pages purgeable?
	 */
	unsigned int madv:2;

	/**
	 * Current tiling mode for the object.
	 */
	unsigned int tiling_mode:2;
	/**
	 * Whether the tiling parameters for the currently associated fence
	 * register have changed. Note that for the purposes of tracking
	 * tiling changes we also treat the unfenced register, the register
	 * slot that the object occupies whilst it executes a fenced
	 * command (such as BLT on gen2/3), as a "fence".
	 */
	unsigned int fence_dirty:1;

	/** How many users have pinned this object in GTT space. The following
	 * users can each hold at most one reference: pwrite/pread, pin_ioctl
	 * (via user_pin_count), execbuffer (objects are not allowed multiple
	 * times for the same batchbuffer), and the framebuffer code. When
	 * switching/pageflipping, the framebuffer code has at most two buffers
	 * pinned per crtc.
	 *
	 * In the worst case this is 1 + 1 + 1 + 2*2 = 7. That would fit into 3
	 * bits with absolutely no headroom. So use 4 bits. */
	unsigned int pin_count:4;
#define DRM_I915_GEM_OBJECT_MAX_PIN_COUNT 0xf

	/**
	 * Is the object at the current location in the gtt mappable and
	 * fenceable? Used to avoid costly recalculations.
	 */
	unsigned int map_and_fenceable:1;

	/**
	 * Whether the current gtt mapping needs to be mappable (and isn't just
	 * mappable by accident). Track pin and fault separate for a more
	 * accurate mappable working set.
	 */
	unsigned int fault_mappable:1;
	unsigned int pin_mappable:1;

	/*
	 * Is the GPU currently using a fence to access this buffer,
	 */
	unsigned int pending_fenced_gpu_access:1;
	unsigned int fenced_gpu_access:1;

	unsigned int cache_level:2;

	unsigned int has_aliasing_ppgtt_mapping:1;
	unsigned int has_global_gtt_mapping:1;
	unsigned int has_dma_mapping:1;

	struct vm_page **pages;
	int pages_pin_count;

	/**
	 * Used for performing relocations during execbuffer insertion.
	 */
	LIST_ENTRY(drm_i915_gem_object) exec_node;
	unsigned long exec_handle;
	struct drm_i915_gem_exec_object2 *exec_entry;

	/**
	 * Current offset of the object in GTT space.
	 *
	 * This is the same as gtt_space->start
	 */
	uint32_t gtt_offset;

	struct intel_ring_buffer *ring;

	/** Breadcrumb of last rendering to the buffer. */
	uint32_t last_read_seqno;
	uint32_t last_write_seqno;
	/** Breadcrumb of last fenced GPU access to the buffer. */
	uint32_t last_fenced_seqno;

	/** Current tiling stride for the object, if it's tiled. */
	uint32_t stride;

	/** Record of address bit 17 of each page at last unbind. */
	unsigned long *bit_17;

	/** User space pin count and filp owning the pin */
	uint32_t user_pin_count;
	struct drm_file *pin_filp;

	/** for phy allocated objects */
	struct drm_i915_gem_phys_object *phys_obj;

	/**
	 * Number of crtcs where this object is currently the fb, but
	 * will be page flipped away on the next vblank.  When it
	 * reaches 0, dev_priv->pending_flip_queue will be woken up.
	 */
	int pending_flip;
};
#define to_gem_object(obj) (&((struct drm_i915_gem_object *)(obj))->base)

#define to_intel_bo(x) container_of(x, struct drm_i915_gem_object, base)

struct drm_i915_file_private {
	struct {
		struct mutex lock;
		struct list_head request_list;
	} mm;
	SPLAY_HEAD(i915_ctx_tree, i915_ctx_handle) ctx_tree;
	uint32_t ctx_id;
};

/**
 * Request queue structure.
 *
 * The request queue allows us to note sequence numbers that have been emitted
 * and may be associated with active buffers to be retired.
 *
 * By keeping this list, we can avoid having to do questionable
 * sequence-number comparisons on buffer last_rendering_seqnos, and associate
 * an emission time with seqnos for tracking how far ahead of the GPU we are.
 */
struct drm_i915_gem_request {
	struct list_head			list;
	/** On Which ring this request was generated */
	struct intel_ring_buffer	*ring;
	/** GEM sequence number associated with this request. */
	uint32_t			seqno;
	/** Postion in the ringbuffer of the end of the request */
	uint32_t			tail;
	/** Time at which this request was emitted, in ticks. */
	unsigned long			emitted_ticks;
	struct drm_i915_file_private	*file_priv;
	/** file_priv list entry for this request */
	struct list_head		client_list;
};

u_int32_t	inteldrm_read_hws(struct inteldrm_softc *, int);
int		intel_ring_begin(struct intel_ring_buffer *, int);
void		intel_ring_emit(struct intel_ring_buffer *, u_int32_t);
void		intel_ring_advance(struct intel_ring_buffer *);
void		inteldrm_update_ring(struct intel_ring_buffer *);
int		inteldrm_pipe_enabled(struct inteldrm_softc *, int);
int		i915_init_phys_hws(struct inteldrm_softc *, bus_dma_tag_t);

unsigned long	i915_chipset_val(struct inteldrm_softc *dev_priv);
unsigned long	i915_mch_val(struct inteldrm_softc *dev_priv);
unsigned long	i915_gfx_val(struct inteldrm_softc *dev_priv);
void		i915_update_gfx_val(struct inteldrm_softc *);

int		intel_init_render_ring_buffer(struct drm_device *);
void		intel_cleanup_ring_buffer(struct intel_ring_buffer *);

/* i915_irq.c */

extern int i915_driver_irq_install(struct drm_device * dev);
extern void i915_driver_irq_uninstall(struct drm_device * dev);
extern void i915_user_irq_get(struct inteldrm_softc *);
extern void i915_user_irq_put(struct inteldrm_softc *);
void	i915_enable_pipestat(struct inteldrm_softc *, int, u_int32_t);
void	i915_disable_pipestat(struct inteldrm_softc *, int, u_int32_t);
void	intel_irq_init(struct drm_device *dev);
void	i915_hangcheck_elapsed(void *);
void	i915_handle_error(struct drm_device *dev, bool wedged);
void	intel_enable_asle(struct drm_device *);

extern void intel_pm_init(struct drm_device *dev);
extern void intel_gt_init(struct drm_device *dev);
extern void intel_gt_sanitize(struct drm_device *dev);

/* gem */
/* Ioctls */
int	i915_gem_init_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_create_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pread_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pwrite_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_set_domain_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_execbuffer2(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_unpin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_busy_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_entervt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_leavevt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_aperture_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_set_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_mmap_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_mmap_gtt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_madvise_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_sw_finish_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_caching_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_set_caching_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_wait_ioctl(struct drm_device *, void *, struct drm_file *);

/* GEM memory manager functions */
int	i915_gem_init_object(struct drm_gem_object *);
void	i915_gem_free_object(struct drm_gem_object *);
int	i915_gem_object_pin(struct drm_i915_gem_object *, uint32_t, bool, bool);
void	i915_gem_object_unpin(struct drm_i915_gem_object *);
void	i915_gem_retire_requests(struct drm_device *);
void	i915_gem_retire_requests_ring(struct intel_ring_buffer *);
int	i915_gem_check_wedge(struct inteldrm_softc *,
			     bool interruptible);

void	i915_gem_retire_work_handler(void *, void*);
int	i915_gem_idle(struct drm_device *);
void	i915_gem_object_move_to_active(struct drm_i915_gem_object *,
	    struct intel_ring_buffer *);
int	i915_add_request(struct intel_ring_buffer *, struct drm_file *, u32 *);
void	i915_gem_init_swizzling(struct drm_device *);
void	i915_gem_cleanup_ringbuffer(struct drm_device *);
int	i915_gem_ring_throttle(struct drm_device *, struct drm_file *);
void	i915_dispatch_gem_execbuffer(struct intel_ring_buffer *,
	    struct drm_i915_gem_execbuffer2 *, uint64_t);

extern int i915_gem_get_seqno(struct drm_device *, u32 *);

int	i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *,
	    bool);
int	i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *,
	    u32, struct intel_ring_buffer *);
int	i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *,
	    bool);

int	i915_gem_init(struct drm_device *);
int	i915_gem_mmap_gtt(struct drm_file *, struct drm_device *,
	    uint32_t, uint64_t *);
int	i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
	    enum i915_cache_level cache_level);

int	i915_gem_object_finish_gpu(struct drm_i915_gem_object *);
int	i915_gem_init_hw(struct drm_device *);

/* Debug functions, mostly called from ddb */
void	i915_gem_seqno_info(int);
void	i915_interrupt_info(int);
void	i915_gem_fence_regs_info(int);
void	i915_hws_info(int);
void	i915_batchbuffer_info(int);
void	i915_ringbuffer_data(int);
void	i915_ringbuffer_info(int);

int i915_gem_object_unbind(struct drm_i915_gem_object *);
int i915_wait_seqno(struct intel_ring_buffer *, uint32_t);
#define I915_GEM_GPU_DOMAINS	(~(I915_GEM_DOMAIN_CPU | I915_GEM_DOMAIN_GTT))

void i915_gem_detach_phys_object(struct drm_device *,
    struct drm_i915_gem_object *);
int i915_gem_attach_phys_object(struct drm_device *dev,
    struct drm_i915_gem_object *, int, int);

int i915_gem_dumb_create(struct drm_file *, struct drm_device *,
    struct drm_mode_create_dumb *);
int i915_gem_mmap_gtt(struct drm_file *, struct drm_device *,
    uint32_t, uint64_t *);
int i915_gem_dumb_destroy(struct drm_file *, struct drm_device *,
    uint32_t);

/* i915_dma.c */
void	i915_driver_lastclose(struct drm_device *);
int	intel_setup_mchbar(struct inteldrm_softc *,
	    struct pci_attach_args *);
void	intel_teardown_mchbar(struct inteldrm_softc *,
	    struct pci_attach_args *, int);
int	i915_getparam(struct drm_device *, void *, struct drm_file *);
int	i915_setparam(struct drm_device *, void *, struct drm_file *);
void	i915_kernel_lost_context(struct drm_device *);
int	i915_driver_open(struct drm_device *, struct drm_file *);
void	i915_driver_close(struct drm_device *, struct drm_file *);

/* i915_drv.c */
void	i915_gem_chipset_flush(struct drm_device *);
int	intel_gpu_reset(struct drm_device *);
int	i915_reset(struct drm_device *);
void	inteldrm_timeout(void *);
bool	i915_semaphore_is_enabled(struct drm_device *);

/* i915_gem_context.c */
void i915_gem_context_init(struct drm_device *dev);
void i915_gem_context_fini(struct drm_device *dev);
void i915_gem_context_close(struct drm_device *dev, struct drm_file *file);
int i915_switch_context(struct intel_ring_buffer *ring,
			struct drm_file *file, int to_id);
int i915_gem_context_create_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
int i915_gem_context_destroy_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file);

/* i915_gem_evict.c */
int __must_check i915_gem_evict_something(struct drm_device *dev, int min_size,
					  unsigned alignment,
					  unsigned cache_level,
					  bool mappable,
					  bool nonblock);
int i915_gem_evict_everything(struct drm_device *dev);

/* i915_gem_tiling.c */
void	i915_gem_detect_bit_6_swizzle(struct drm_device *);
void	i915_gem_object_do_bit_17_swizzle(struct drm_i915_gem_object *);
void	i915_gem_object_save_bit_17_swizzle(struct drm_i915_gem_object *);

/* i915_gem_debug.c */
#define i915_verify_lists(dev) 0

/* i915_suspend.c */
extern int i915_save_state(struct drm_device *);
extern int i915_restore_state(struct drm_device *);

/* intel_i2c.c */
extern int intel_setup_gmbus(struct inteldrm_softc *);
static inline bool intel_gmbus_is_port_valid(unsigned port)
{
	return (port >= GMBUS_PORT_SSC && port <= GMBUS_PORT_DPD);
}

struct i2c_controller *intel_gmbus_get_adapter(drm_i915_private_t *, unsigned);
extern void intel_gmbus_force_bit(struct i2c_controller *, bool);
static inline bool intel_gmbus_is_forced_bit(struct i2c_controller *i2c)
{
	return container_of(i2c, struct intel_gmbus, controller)->force_bit;
}

/* i915_gem.c */
int i915_gem_fault(struct drm_gem_object *, struct uvm_faultinfo *, off_t,
    vaddr_t, vm_page_t *, int, int, vm_prot_t, int );
void i915_gem_object_init(struct drm_i915_gem_object *obj,
			 const struct drm_i915_gem_object_ops *ops);
struct drm_i915_gem_object *i915_gem_alloc_object(struct drm_device *dev,
						  size_t size);
int i915_gpu_idle(struct drm_device *);
int i915_gem_object_get_fence(struct drm_i915_gem_object *);
int i915_gem_object_put_fence(struct drm_i915_gem_object *);
void i915_gem_reset(struct drm_device *);
void i915_gem_clflush_object(struct drm_i915_gem_object *);
void i915_gem_release(struct drm_device *, struct drm_file *);
void i915_gem_release_mmap(struct drm_i915_gem_object *);
void i915_gem_load(struct drm_device *dev);

uint32_t
i915_gem_get_unfenced_gtt_alignment(struct drm_device *dev,
				    uint32_t size,
				    int tiling_mode);

int i915_gem_object_sync(struct drm_i915_gem_object *,
    struct intel_ring_buffer *);

int i915_gem_object_get_pages(struct drm_i915_gem_object *obj);

static inline void i915_gem_object_pin_pages(struct drm_i915_gem_object *obj)
{
	BUG_ON(obj->pages == NULL);
	obj->pages_pin_count++;
}
static inline void i915_gem_object_unpin_pages(struct drm_i915_gem_object *obj)
{
	BUG_ON(obj->pages_pin_count == 0);
	obj->pages_pin_count--;
}

int i915_mutex_lock_interruptible(struct drm_device *dev);

/* intel_opregion.c */
int intel_opregion_setup(struct drm_device *dev);
#if NACPI > 0
extern void intel_opregion_init(struct drm_device *dev);
extern void intel_opregion_fini(struct drm_device *dev);
extern void intel_opregion_asle_intr(struct drm_device *dev);
extern void intel_opregion_gse_intr(struct drm_device *dev);
extern void intel_opregion_enable_asle(struct drm_device *dev);
#else
static inline void intel_opregion_init(struct drm_device *dev) { return; }
static inline void intel_opregion_fini(struct drm_device *dev) { return; }
static inline void intel_opregion_asle_intr(struct drm_device *dev) { return; }
static inline void intel_opregion_gse_intr(struct drm_device *dev) { return; }
static inline void intel_opregion_enable_asle(struct drm_device *dev) { return; }
#endif

/* i915_gem_gtt.c */
void i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev);
void i915_gem_restore_gtt_mappings(struct drm_device *dev);
int __must_check i915_gem_gtt_prepare_object(struct drm_i915_gem_object *obj);
void i915_gem_gtt_bind_object(struct drm_i915_gem_object *obj,
				enum i915_cache_level cache_level);
void i915_gem_gtt_unbind_object(struct drm_i915_gem_object *obj);
void i915_gem_gtt_finish_object(struct drm_i915_gem_object *obj);
void i915_gem_init_global_gtt(struct drm_device *dev,
			      unsigned long start,
			      unsigned long mappable_end,
			      unsigned long end);

/* modesetting */
extern void intel_modeset_init_hw(struct drm_device *dev);
extern void intel_modeset_init(struct drm_device *dev);
extern void intel_modeset_gem_init(struct drm_device *dev);
extern void intel_modeset_cleanup(struct drm_device *dev);
extern int intel_modeset_vga_set_state(struct drm_device *dev, bool state);
extern void intel_modeset_setup_hw_state(struct drm_device *dev,
					 bool force_restore);
extern bool intel_fbc_enabled(struct drm_device *dev);
extern void intel_disable_fbc(struct drm_device *dev);
extern bool ironlake_set_drps(struct drm_device *dev, u8 val);
extern void intel_init_pch_refclk(struct drm_device *dev);
extern void gen6_set_rps(struct drm_device *dev, u8 val);
extern void intel_detect_pch(struct inteldrm_softc *dev_priv);
extern int intel_trans_dp_port_sel(struct drm_crtc *crtc);
int i915_load_modeset_init(struct drm_device *dev);
extern int intel_enable_rc6(const struct drm_device *dev);

extern struct intel_overlay_error_state *intel_overlay_capture_error_state(
    struct drm_device *dev);
#ifdef notyet
extern void intel_overlay_print_error_state(struct sbuf *m,
    struct intel_overlay_error_state *error);
#endif
extern struct intel_display_error_state *intel_display_capture_error_state(
    struct drm_device *dev);
#ifdef notyet
extern void intel_display_print_error_state(struct sbuf *m,
    struct drm_device *dev, struct intel_display_error_state *error);
#endif

/* On SNB platform, before reading ring registers forcewake bit
 * must be set to prevent GT core from power down and stale values being
 * returned.
 */
void gen6_gt_force_wake_get(struct inteldrm_softc *dev_priv);
void gen6_gt_force_wake_put(struct inteldrm_softc *dev_priv);
int __gen6_gt_wait_for_fifo(struct inteldrm_softc *dev_priv);

int sandybridge_pcode_read(struct inteldrm_softc *dev_priv, u8 mbox, u32 *val);
int sandybridge_pcode_write(struct inteldrm_softc *dev_priv, u8 mbox, u32 val);

static __inline void
write8(struct inteldrm_softc *dev_priv, bus_size_t reg, uint8_t val)
{
	bus_space_write_1(dev_priv->regs->bst,dev_priv->regs->bsh, (reg),
	    (val));
}

static __inline void
write16(struct inteldrm_softc *dev_priv, bus_size_t reg, uint16_t val)
{
	bus_space_write_2(dev_priv->regs->bst,dev_priv->regs->bsh, (reg),
	    (val));
}

static __inline void
write32(struct inteldrm_softc *dev_priv, bus_size_t reg, uint32_t val)
{
	bus_space_write_4(dev_priv->regs->bst,dev_priv->regs->bsh, (reg),
	    (val));
}

/* XXX need bus_space_write_8, this evaluated arguments twice */
static __inline void
write64(struct inteldrm_softc *dev_priv, bus_size_t reg, uint64_t val)
{
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    reg, (u_int32_t)val);
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    reg + 4, upper_32_bits(val));
}

static __inline uint8_t
read8(struct inteldrm_softc *dev_priv, bus_size_t reg)
{
	return (bus_space_read_1(dev_priv->regs->bst, dev_priv->regs->bsh,
	    (reg)));
}

static __inline uint16_t
read16(struct inteldrm_softc *dev_priv, bus_size_t reg)
{
	return (bus_space_read_2(dev_priv->regs->bst, dev_priv->regs->bsh,
	    (reg)));
}

static __inline uint32_t
read32(struct inteldrm_softc *dev_priv, bus_size_t reg)
{
	return (bus_space_read_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    (reg)));
}

static __inline uint64_t
read64(struct inteldrm_softc *dev_priv, bus_size_t reg)
{
	u_int32_t low, high;

	low = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, reg);
	high = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, reg + 4);

	return ((u_int64_t)low | ((u_int64_t)high << 32));
}


#define __i915_read(x, y) \
	u##x i915_read##x(struct drm_i915_private *dev_priv, u32 reg);

__i915_read(8, b)
__i915_read(16, w)
__i915_read(32, l)
__i915_read(64, q)
#undef __i915_read

#define __i915_write(x, y) \
	void i915_write##x(struct drm_i915_private *dev_priv, u32 reg, u##x val);

__i915_write(8, b)
__i915_write(16, w)
__i915_write(32, l)
__i915_write(64, q)
#undef __i915_write

#define I915_READ8(reg)		i915_read8(dev_priv, (reg))
#define I915_WRITE8(reg, val)	i915_write8(dev_priv, (reg), (val))

#define I915_READ16(reg)	i915_read16(dev_priv, (reg))
#define I915_WRITE16(reg, val)	i915_write16(dev_priv, (reg), (val))
#define I915_READ16_NOTRACE(reg)	bus_space_read_2(dev_priv->regs->bst, \
					    dev_priv->regs->bsh, (reg))
#define I915_WRITE16_NOTRACE(reg,val)	bus_space_write_2(dev_priv->regs->bst, \
					    dev_priv->regs->bsh, (reg), (val))

#define I915_READ(reg)		i915_read32(dev_priv, (reg))
#define I915_WRITE(reg, val)	i915_write32(dev_priv, (reg), (val))
#define I915_READ_NOTRACE(reg)		bus_space_read_4(dev_priv->regs->bst, \
					    dev_priv->regs->bsh, (reg))
#define I915_WRITE_NOTRACE(reg,val)	bus_space_write_4(dev_priv->regs->bst, \
					    dev_priv->regs->bsh, (reg), (val))

#define I915_WRITE64(reg, val)	i915_write64(dev_priv, (reg), (val))
#define I915_READ64(reg)	i915_read64(dev_priv, (reg))

#define POSTING_READ(reg)	(void)I915_READ_NOTRACE(reg)
#define POSTING_READ16(reg)	(void)I915_READ16_NOTRACE(reg)

#define INTELDRM_VERBOSE 0
#if INTELDRM_VERBOSE > 0
#define	INTELDRM_VPRINTF(fmt, args...) DRM_INFO(fmt, ##args)
#else
#define	INTELDRM_VPRINTF(fmt, args...)
#endif

#define LP_RING(d) (&((struct inteldrm_softc *)(d))->ring[RCS])

#define BEGIN_LP_RING(n) \
	intel_ring_begin(LP_RING(dev_priv), (n))

#define OUT_RING(x) \
	intel_ring_emit(LP_RING(dev_priv), (x))

#define ADVANCE_LP_RING() \
	intel_ring_advance(LP_RING(dev_priv))

/* MCH IFP BARs */
#define	I915_IFPADDR	0x60
#define I965_IFPADDR	0x70

/**
 * Reads a dword out of the status page, which is written to from the command
 * queue by automatic updates, MI_REPORT_HEAD, MI_STORE_DATA_INDEX, or
 * MI_STORE_DATA_IMM.
 *
 * The following dwords have a reserved meaning:
 * 0x00: ISR copy, updated when an ISR bit not set in the HWSTAM changes.
 * 0x04: ring 0 head pointer
 * 0x05: ring 1 head pointer (915-class)
 * 0x06: ring 2 head pointer (915-class)
 * 0x10-0x1b: Context status DWords (GM45)
 * 0x1f: Last written status offset. (GM45)
 *
 * The area from dword 0x20 to 0x3ff is available for driver usage.
 */
#define READ_HWSP(dev_priv, reg)  inteldrm_read_hws(dev_priv, reg)
#define I915_GEM_HWS_INDEX		0x20

#define INTEL_INFO(dev) (((struct inteldrm_softc *) (dev)->dev_private)->info)

/* Chipset type macros */

#define IS_I830(dev)		((dev)->pci_device == 0x3577)
#define IS_845G(dev)		((dev)->pci_device == 0x2562)
#define IS_I85X(dev)		(INTEL_INFO(dev)->is_i85x)
#define IS_I865G(dev)		((dev)->pci_device == 0x2572)
#define IS_I915G(dev)		(INTEL_INFO(dev)->is_i915g)
#define IS_I915GM(dev)		((dev)->pci_device == 0x2592)
#define IS_I945G(dev)		((dev)->pci_device == 0x2772)
#define IS_I945GM(dev)		(INTEL_INFO(dev)->is_i945gm)
#define IS_BROADWATER(dev)	(INTEL_INFO(dev)->is_broadwater)
#define IS_CRESTLINE(dev)	(INTEL_INFO(dev)->is_crestline)
#define IS_GM45(dev)		((dev)->pci_device == 0x2A42)
#define IS_G4X(dev)		(INTEL_INFO(dev)->is_g4x)
#define IS_PINEVIEW_G(dev)	((dev)->pci_device == 0xa001)
#define IS_PINEVIEW_M(dev)	((dev)->pci_device == 0xa011)
#define IS_PINEVIEW(dev)	(INTEL_INFO(dev)->is_pineview)
#define IS_G33(dev)		(INTEL_INFO(dev)->is_g33)
#define IS_IRONLAKE_D(dev)	((dev)->pci_device == 0x0042)
#define IS_IRONLAKE_M(dev)	((dev)->pci_device == 0x0046)
#define IS_IVYBRIDGE(dev)	(INTEL_INFO(dev)->is_ivybridge)
#define IS_IVB_GT1(dev)		((dev)->pci_device == 0x0156 || \
				 (dev)->pci_device == 0x0152 || \
				 (dev)->pci_device == 0x015a)
#define IS_SNB_GT1(dev)		((dev)->pci_device == 0x0102 || \
				 (dev)->pci_device == 0x0106 || \
				 (dev)->pci_device == 0x010A)
#define IS_MOBILE(dev)		(INTEL_INFO(dev)->is_mobile)

#define IS_I9XX(dev)		(INTEL_INFO(dev)->gen >= 3)
#define IS_IRONLAKE(dev)	(INTEL_INFO(dev)->gen == 5)

#define IS_SANDYBRIDGE(dev)	(INTEL_INFO(dev)->gen == 6)
#define IS_SANDYBRIDGE_D(dev)	(IS_SANDYBRIDGE(dev) && \
 (INTEL_INFO(dev)->is_mobile == 0))
#define IS_SANDYBRIDGE_M(dev)	(IS_SANDYBRIDGE(dev) && \
 (INTEL_INFO(dev)->is_mobile == 1))
#define IS_VALLEYVIEW(dev)	(INTEL_INFO(dev)->is_valleyview)
#define IS_HASWELL(dev)	(INTEL_INFO(dev)->is_haswell)
#define IS_ULT(dev)	(IS_HASWELL(dev) && \
			 ((dev)->pci_device & 0xFF00) == 0x0A00)

/*
 * The genX designation typically refers to the render engine, so render
 * capability related checks should use IS_GEN, while display and other checks
 * have their own (e.g. HAS_PCH_SPLIT for ILK+ display, IS_foo for particular
 * chips, etc.).
 */
#define IS_GEN2(dev)		(INTEL_INFO(dev)->gen == 2)
#define IS_GEN3(dev)		(INTEL_INFO(dev)->gen == 3)
#define IS_GEN4(dev)		(INTEL_INFO(dev)->gen == 4)
#define IS_GEN5(dev)		(INTEL_INFO(dev)->gen == 5)
#define IS_GEN6(dev)		(INTEL_INFO(dev)->gen == 6)
#define IS_GEN7(dev)		(INTEL_INFO(dev)->gen == 7)

#define HAS_BSD(dev)		(INTEL_INFO(dev)->has_bsd_ring)
#define HAS_BLT(dev)		(INTEL_INFO(dev)->has_blt_ring)
#define HAS_LLC(dev)		(INTEL_INFO(dev)->has_llc)
#define I915_NEED_GFX_HWS(dev)	(INTEL_INFO(dev)->need_gfx_hws)

#define HAS_HW_CONTEXTS(dev)	(INTEL_INFO(dev)->gen >= 6)
#define HAS_ALIASING_PPGTT(dev)	(INTEL_INFO(dev)->gen >=6 && !IS_VALLEYVIEW(dev))

#define HAS_OVERLAY(dev)	(INTEL_INFO(dev)->has_overlay)
#define OVERLAY_NEEDS_PHYSICAL(dev) \
    (INTEL_INFO(dev)->overlay_needs_physical)

/* Early gen2 have a totally busted CS tlb and require pinned batches. */
#define HAS_BROKEN_CS_TLB(dev)	(IS_I830(dev) || IS_845G(dev))

/*
 * With the 945 and later, Y tiling got adjusted so that it was 32 128-byte
 * rows, which changes the alignment requirements and fence programming.
 */
#define HAS_128_BYTE_Y_TILING(dev) (IS_I9XX(dev) &&	\
	!(IS_I915G(dev) || IS_I915GM(dev)))

#define HAS_RESET(dev)	(INTEL_INFO(dev)->gen >= 4 && \
    (!IS_GEN6(dev)) && (!IS_GEN7(dev)))

#define SUPPORTS_DIGITAL_OUTPUTS(dev)	(!IS_GEN2(dev) && !IS_PINEVIEW(dev))
#define SUPPORTS_INTEGRATED_HDMI(dev)	(IS_G4X(dev) || IS_GEN5(dev))
#define SUPPORTS_INTEGRATED_DP(dev)	(IS_G4X(dev) || IS_GEN5(dev))
#define SUPPORTS_EDP(dev)		(IS_IRONLAKE_M(dev))
#define SUPPORTS_TV(dev)		(INTEL_INFO(dev)->supports_tv)
#define I915_HAS_HOTPLUG(dev)		(INTEL_INFO(dev)->has_hotplug)

#define HAS_FW_BLC(dev)		(INTEL_INFO(dev)->gen > 2)
#define HAS_PIPE_CXSR(dev)	(INTEL_INFO(dev)->has_pipe_cxsr)
#define I915_HAS_FBC(dev)	(INTEL_INFO(dev)->has_fbc)

#define HAS_PIPE_CONTROL(dev)	(INTEL_INFO(dev)->gen >= 5)

#define HAS_DDI(dev)		(IS_HASWELL(dev))

#define INTEL_PCH_DEVICE_ID_MASK		0xff00
#define INTEL_PCH_IBX_DEVICE_ID_TYPE		0x3b00
#define INTEL_PCH_CPT_DEVICE_ID_TYPE		0x1c00
#define INTEL_PCH_PPT_DEVICE_ID_TYPE		0x1e00
#define INTEL_PCH_LPT_DEVICE_ID_TYPE		0x8c00
#define INTEL_PCH_LPT_LP_DEVICE_ID_TYPE		0x9c00

#define INTEL_PCH_TYPE(dev)	(((struct inteldrm_softc *) (dev)->dev_private)->pch_type)
#define HAS_PCH_LPT(dev)	(INTEL_PCH_TYPE(dev) == PCH_LPT)
#define HAS_PCH_CPT(dev)	(INTEL_PCH_TYPE(dev) == PCH_CPT)
#define HAS_PCH_IBX(dev)	(INTEL_PCH_TYPE(dev) == PCH_IBX)
#define HAS_PCH_SPLIT(dev)	(INTEL_PCH_TYPE(dev) != PCH_NONE)

#define HAS_FORCE_WAKE(dev) (INTEL_INFO(dev)->has_force_wake)

#define HAS_L3_GPU_CACHE(dev) (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))

#define PRIMARY_RINGBUFFER_SIZE         (128*1024)

#define  __EXEC_OBJECT_HAS_FENCE (1<<30)

/**
 * RC6 is a special power stage which allows the GPU to enter an very
 * low-voltage mode when idle, using down to 0V while at this stage.  This
 * stage is entered automatically when the GPU is idle when RC6 support is
 * enabled, and as soon as new workload arises GPU wakes up automatically as well.
 * 
 * There are different RC6 modes available in Intel GPU, which differentiate
 * among each other with the latency required to enter and leave RC6 and
 * voltage consumed by the GPU in different states.
 * 
 * The combination of the following flags define which states GPU is allowed
 * to enter, while RC6 is the normal RC6 state, RC6p is the deep RC6, and
 * RC6pp is deepest RC6. Their support by hardware varies according to the
 * GPU, BIOS, chipset and platform. RC6 is usually the safest one and the one
 * which brings the most power savings; deeper states save more power, but
 * require higher latency to switch to and wake up.
 */
#define INTEL_RC6_ENABLE			(1<<0)
#define INTEL_RC6p_ENABLE			(1<<1)
#define INTEL_RC6pp_ENABLE			(1<<2)

extern unsigned int i915_lvds_downclock;
extern int i915_lvds_channel_mode;
extern int i915_panel_use_ssc;
extern int i915_panel_ignore_lid;
extern unsigned int i915_powersave;
extern int i915_semaphores;
extern int i915_vbt_sdvo_panel_type;
extern int i915_enable_rc6;
extern int i915_enable_fbc;
extern bool i915_enable_hangcheck;

/* Inlines */

/**
 * Returns true if seq1 is later than seq2.
 */
static __inline int
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return ((int32_t)(seq1 - seq2) >= 0);
}

static inline bool
i915_gem_object_pin_fence(struct drm_i915_gem_object *obj)
{
	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
		dev_priv->fence_regs[obj->fence_reg].pin_count++;
		return true;
	} else
		return false;
}

static inline void
i915_gem_object_unpin_fence(struct drm_i915_gem_object *obj)
{
	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
		dev_priv->fence_regs[obj->fence_reg].pin_count--;
	}
}

static inline int
i915_gem_object_is_purgeable(struct drm_i915_gem_object *obj)
{
	return obj->madv == I915_MADV_DONTNEED;
}

#endif
