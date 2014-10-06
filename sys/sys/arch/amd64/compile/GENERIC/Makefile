IDENT=-DDDB -DDIAGNOSTIC -DKTRACE -DACCOUNTING -DKMEMSTATS -DPTRACE -DPOOL_DEBUG -DCRYPTO -DSYSVMSG -DSYSVSEM -DSYSVSHM -DUVM_SWAP_ENCRYPT -DCOMPAT_43 -DLKM -DFFS -DFFS2 -DFFS_SOFTUPDATES -DUFS_DIRHASH -DQUOTA -DEXT2FS -DMFS -DNFSCLIENT -DNFSSERVER -DCD9660 -DUDF -DMSDOSFS -DFIFO -DTMPFS -DFUSE -DSOCKET_SPLICE -DTCP_SACK -DTCP_ECN -DTCP_SIGNATURE -DINET -DINET6 -DIPSEC -DPPP_BSDCOMP -DPPP_DEFLATE -DPIPEX -DMROUTING -DMPLS -DBOOT_CONFIG -DUSER_PCICONF -DAPERTURE -DMTRR -DNTFS -DHAMMER2 -DHIBERNATE -DPCIVERBOSE -DUSBVERBOSE -DWSDISPLAY_COMPAT_USL -DWSDISPLAY_COMPAT_RAWKBD -DWSDISPLAY_DEFAULTSCREENS="6" -DWSDISPLAY_COMPAT_PCVT -DX86EMU -DONEWIREVERBOSE
PARAM=-DMAXUSERS=80
S=	../../../..
_mach=amd64
_arch=amd64
#	$OpenBSD: Makefile.amd64,v 1.58 2013/10/16 17:34:02 florian Exp $

# For instructions on building kernels consult the config(8) and options(4)
# manual pages.
#
# N.B.: NO DEPENDENCIES ON FOLLOWING FLAGS ARE VISIBLE TO MAKEFILE
#	IF YOU CHANGE THE DEFINITION OF ANY OF THESE RECOMPILE EVERYTHING
# DEBUG is set to -g by config if debugging is requested (config -g).
# PROF is set to -pg by config if profiling is requested (config -p).

.include <bsd.own.mk>

SIZE?=	size
STRIP?=	strip

# source tree is located via $S relative to the compilation directory
.ifndef S
S!=	cd ../../../..; pwd
.endif

_machdir?=	$S/arch/${_mach}
_archdir?=	$S/arch/${_arch}

INCLUDES=	-nostdinc -I$S -I. -I$S/arch
CPPFLAGS=	${INCLUDES} ${IDENT} ${PARAM} -D_KERNEL -MD -MP
CWARNFLAGS=	-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wno-main -Wno-uninitialized -Wno-format \
		-Wstack-larger-than-2047

CMACHFLAGS=	-mcmodel=kernel -mno-red-zone -mno-sse2 -mno-sse -mno-3dnow \
		-mno-mmx -msoft-float -fno-omit-frame-pointer
CMACHFLAGS+=	-fno-builtin-printf -fno-builtin-snprintf \
		-fno-builtin-vsnprintf -fno-builtin-log \
		-fno-builtin-log2 -fno-builtin-malloc ${NOPIE_FLAGS}
.if ${IDENT:M-DNO_PROPOLICE}
CMACHFLAGS+=	-fno-stack-protector
.endif
.if ${IDENT:M-DSMALL_KERNEL}
CMACHFLAGS+=	-Wa,-n
.endif

COPTS?=		-O2
CFLAGS=		${DEBUG} ${CWARNFLAGS} ${CMACHFLAGS} ${COPTS} ${PIPE}
AFLAGS=		-D_LOCORE -x assembler-with-cpp ${CWARNFLAGS} ${CMACHFLAGS}
LINKFLAGS=	-Ttext 0xffffffff810001e0 -e start --warn-common -nopie

.if ${IDENT:M-DDDB_STRUCT}
DB_STRUCTINFO=	db_structinfo.h
.else
DB_STRUCTINFO=
.endif

HOSTCC?=	${CC}
HOSTED_CPPFLAGS=${CPPFLAGS:S/^-nostdinc$//}
HOSTED_CFLAGS=	${CFLAGS}
HOSTED_C=	${HOSTCC} ${HOSTED_CFLAGS} ${HOSTED_CPPFLAGS} -c $<

NORMAL_C_NOP=	${CC} ${CFLAGS} ${CPPFLAGS} -c $<
NORMAL_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<

OBJS=	smc93cx6.o pcdisplay_subr.o pcdisplay_chars.o vga.o \
	vga_subr.o edid.o vesagtf.o videomode.o mii_bitbang.o wdc.o \
	aic7xxx.o aic7xxx_openbsd.o aic7xxx_seeprom.o aic79xx.o \
	aic79xx_openbsd.o aic6360.o adw.o gdt_common.o twe.o ciss.o \
	ami.o mfi.o qlw.o qla.o isp_fw2100.o isp_fw2200.o \
	isp_fw2300.o ahci.o mpi.o sili.o ncr53c9x.o siop_common.o \
	siop.o elink3.o if_wi.o if_wi_hostap.o an.o xl.o fxp.o \
	rtl81x9.o re.o dc.o smc91cxx.o smc83c170.o ne2000.o \
	dl10019.o ax88190.o gem.o ti.o com.o pckbc.o ac97.o lpt.o \
	iha.o lm78.o ar5xxx.o ar5210.o ar5211.o ar5212.o ath.o \
	athn.o ar5008.o ar5416.o ar9280.o ar9285.o ar9287.o \
	ar9003.o ar9380.o atw.o rtw.o rt2560.o rt2661.o rt2860.o \
	acx.o acx111.o acx100.o pgt.o aic6915.o malo.o bwi.o \
	uhci.o ohci.o ehci.o sdhc.o rtsx.o radio.o ipmi.o vmt.o \
	vscsi.o mpath.o softraid.o softraid_concat.o \
	softraid_crypto.o softraid_raid0.o softraid_raid1.o \
	softraid_raid5.o softraid_raid6.o spdmem.o ksyms.o \
	fuse_device.o fuse_file.o fuse_lookup.o fuse_vfsops.o \
	fuse_vnops.o fusebuf.o pf.o pf_norm.o pf_ruleset.o \
	pf_ioctl.o pf_table.o pf_osfp.o pf_if.o pf_lb.o hfsc.o \
	if_pflog.o if_pfsync.o if_pflow.o bio.o hotplug.o \
	if_pppoe.o db_access.o db_break.o db_command.o db_elf.o \
	db_examine.o db_expr.o db_hangman.o db_input.o db_lex.o \
	db_output.o db_print.o db_run.o db_sym.o db_trap.o \
	db_variables.o db_watch.o db_write_cmd.o db_usrreq.o \
	auconv.o audio.o cons.o diskmap.o firmload.o dp8390.o \
	rtl80x9.o midi.o mulaw.o systrace.o vnd.o rnd.o video.o \
	cd9660_bmap.o cd9660_lookup.o cd9660_node.o cd9660_rrip.o \
	cd9660_util.o cd9660_vfsops.o cd9660_vnops.o udf_subr.o \
	udf_vfsops.o udf_vnops.o clock_subr.o exec_conf.o \
	exec_elf32.o exec_elf64.o exec_script.o exec_subr.o \
	init_main.o init_sysent.o kern_acct.o kern_bufq.o \
	kern_clock.o kern_descrip.o kern_event.o kern_exec.o \
	kern_exit.o kern_fork.o kern_kthread.o kern_ktrace.o \
	kern_lock.o kern_lkm.o kern_malloc.o kern_rwlock.o \
	kern_physio.o kern_proc.o kern_prot.o kern_resource.o \
	kern_sched.o kern_sensors.o kern_sig.o kern_subr.o \
	kern_sysctl.o kern_synch.o kern_tc.o kern_time.o \
	kern_timeout.o kern_watchdog.o kern_workq.o kern_task.o \
	kern_xxx.o kern_uuid.o kern_objcache.o sched_bsd.o \
	subr_autoconf.o subr_disk.o subr_evcount.o subr_extent.o \
	subr_hibernate.o subr_log.o subr_poison.o subr_pool.o \
	dma_alloc.o subr_prf.o subr_prof.o subr_userconf.o \
	subr_xxx.o sys_generic.o sys_pipe.o sys_process.o \
	sys_socket.o sysv_ipc.o sysv_msg.o sysv_sem.o sysv_shm.o \
	tty.o tty_conf.o tty_pty.o tty_nmea.o tty_msts.o \
	tty_endrun.o tty_subr.o tty_tty.o uipc_domain.o uipc_mbuf.o \
	uipc_mbuf2.o uipc_proto.o uipc_socket.o uipc_socket2.o \
	uipc_syscalls.o uipc_usrreq.o vfs_bio.o vfs_biomem.o \
	vfs_cache.o vfs_cluster.o vfs_default.o vfs_init.o \
	vfs_lockf.o vfs_lookup.o vfs_subr.o vfs_sync.o \
	vfs_syscalls.o vfs_vops.o vfs_vnops.o vfs_getcwd.o \
	spec_vnops.o dead_vnops.o fifo_vnops.o msdosfs_conv.o \
	msdosfs_denode.o msdosfs_fat.o msdosfs_lookup.o \
	msdosfs_vfsops.o msdosfs_vnops.o hammer2_ccms.o \
	hammer2_chain.o hammer2_cluster.o hammer2_flush.o \
	hammer2_freemap.o hammer2_inode.o hammer2_io.o \
	hammer2_ioctl.o hammer2_lz4.o hammer2_msgops.o hammer2_subr.o \
	hammer2_vfsops.o hammer2_vnops.o ntfs_compr.o ntfs_conv.o \
	ntfs_ihash.o ntfs_subr.o ntfs_vfsops.o ntfs_vnops.o \
	tmpfs_mem.o tmpfs_subr.o tmpfs_vfsops.o tmpfs_vnops.o \
	tmpfs_specops.o tmpfs_fifoops.o bpf.o bpf_filter.o if.o \
	if_ethersubr.o if_spppsubr.o if_loop.o if_media.o if_sl.o \
	if_ppp.o netisr.o ppp_tty.o bsd-comp.o ppp-deflate.o \
	if_tun.o if_bridge.o bridgestp.o if_vlan.o pipex.o radix.o \
	radix_mpath.o raw_cb.o raw_usrreq.o route.o rtsock.o \
	slcompress.o if_enc.o if_gre.o if_trunk.o trunklacp.o \
	if_mpe.o if_vether.o if_pppx.o if_vxlan.o ieee80211.o \
	ieee80211_amrr.o ieee80211_crypto.o ieee80211_crypto_bip.o \
	ieee80211_crypto_ccmp.o ieee80211_crypto_tkip.o \
	ieee80211_crypto_wep.o ieee80211_input.o ieee80211_ioctl.o \
	ieee80211_node.o ieee80211_output.o ieee80211_pae_input.o \
	ieee80211_pae_output.o ieee80211_proto.o ieee80211_rssadapt.o \
	ieee80211_regdomain.o if_ether.o igmp.o in.o in_pcb.o \
	in_proto.o inet_nat64.o inet_ntop.o ip_divert.o ip_icmp.o \
	ip_id.o ip_input.o ip_mroute.o ip_output.o raw_ip.o \
	tcp_debug.o tcp_input.o tcp_output.o tcp_subr.o tcp_timer.o \
	tcp_usrreq.o udp_usrreq.o ip_gre.o ip_ipsp.o ip_spd.o \
	ip_ipip.o ip_ether.o ipsec_input.o ipsec_output.o ip_esp.o \
	ip_ah.o ip_carp.o ip_ipcomp.o rijndael.o md5.o rmd160.o \
	sha1.o sha2.o blf.o cast.o ecb_enc.o set_key.o ecb3_enc.o \
	crypto.o cryptodev.o criov.o cryptosoft.o xform.o \
	xform_ipcomp.o arc4.o michael.o cmac.o hmac.o gmac.o \
	key_wrap.o idgen.o mpls_input.o mpls_output.o mpls_proto.o \
	mpls_raw.o mpls_shim.o krpc_subr.o nfs_bio.o nfs_boot.o \
	nfs_debug.o nfs_node.o nfs_kq.o nfs_serv.o nfs_socket.o \
	nfs_srvcache.o nfs_subs.o nfs_syscalls.o nfs_vfsops.o \
	nfs_vnops.o ffs_alloc.o ffs_balloc.o ffs_inode.o ffs_subr.o \
	ffs_softdep_stub.o ffs_tables.o ffs_vfsops.o ffs_vnops.o \
	ffs_softdep.o mfs_vfsops.o mfs_vnops.o ufs_bmap.o \
	ufs_dirhash.o ufs_ihash.o ufs_inode.o ufs_lookup.o \
	ufs_quota.o ufs_quota_stub.o ufs_vfsops.o ufs_vnops.o \
	ext2fs_alloc.o ext2fs_balloc.o ext2fs_bmap.o ext2fs_bswap.o \
	ext2fs_inode.o ext2fs_lookup.o ext2fs_readwrite.o \
	ext2fs_subr.o ext2fs_vfsops.o ext2fs_vnops.o uvm_addr.o \
	uvm_amap.o uvm_anon.o uvm_aobj.o uvm_device.o uvm_fault.o \
	uvm_glue.o uvm_init.o uvm_io.o uvm_km.o uvm_map.o \
	uvm_meter.o uvm_mmap.o uvm_object.o uvm_page.o uvm_pager.o \
	uvm_pdaemon.o uvm_pmemrange.o uvm_stat.o uvm_swap.o \
	uvm_swap_encrypt.o uvm_unix.o uvm_user.o uvm_vnode.o \
	if_gif.o ip_ecn.o in_gif.o in6_gif.o in6_pcb.o in6.o \
	ip6_divert.o in6_ifattach.o in6_cksum.o in6_src.o \
	in6_proto.o dest6.o frag6.o icmp6.o ip6_id.o ip6_input.o \
	ip6_forward.o ip6_mroute.o ip6_output.o route6.o mld6.o \
	nd6.o nd6_nbr.o nd6_rtr.o raw_ip6.o udp6_output.o pfkey.o \
	pfkeyv2.o pfkeyv2_parsemessage.o pfkeyv2_convert.o \
	compat_util.o x86emu.o x86emu_util.o getsn.o random.o \
	explicit_bzero.o timingsafe_bcmp.o strchr.o strrchr.o \
	__main.o imax.o imin.o lmax.o lmin.o max.o min.o ulmax.o \
	ulmin.o memchr.o memcmp.o bcmp.o bzero.o bcopy.o memcpy.o \
	memmove.o ffs.o fls.o flsl.o memset.o strcmp.o strlcat.o \
	strlcpy.o strlen.o strncmp.o strncpy.o strnlen.o scanc.o \
	skpc.o htonl.o htons.o strncasecmp.o adler32.o crc32.o \
	infback.o inffast.o inflate.o inftrees.o deflate.o zutil.o \
	zopenbsd.o trees.o compress.o autoconf.o conf.o disksubr.o \
	machdep.o hibernate_machdep.o identcpu.o via.o aes_intel.o \
	aesni.o amd64errata.o mem.o amd64_mem.o mtrr.o pmap.o \
	process_machdep.o sys_machdep.o trap.o vm_machdep.o fpu.o \
	softintr.o i8259.o cacheinfo.o mutex.o vector.o copy.o \
	spl.o intr.o bus_space.o bus_dma.o apic.o consinit.o \
	cninit.o dkcsum.o db_disasm.o db_interface.o db_memrw.o \
	db_trace.o in_cksum.o in4_cksum.o clock.o powernow-k8.o \
	est.o k1x-pstate.o rasops.o rasops8.o rasops15.o rasops32.o \
	wsfont.o mii.o mii_physubr.o ukphy_subr.o nsphy.o \
	nsphyter.o gentbi.o qsphy.o inphy.o iophy.o eephy.o \
	exphy.o rlphy.o lxtphy.o luphy.o mtdphy.o icsphy.o sqphy.o \
	tqphy.o ukphy.o dcphy.o bmtphy.o brgphy.o xmphy.o amphy.o \
	acphy.o nsgphy.o urlphy.o rgephy.o ciphy.o ipgphy.o \
	etphy.o jmphy.o atphy.o scsi_base.o scsi_ioctl.o scsiconf.o \
	cd.o ch.o sd.o st.o uk.o safte.o ses.o mpath_sym.o \
	mpath_rdac.o mpath_emc.o mpath_hds.o atapiscsi.o wd.o \
	ata_wdc.o ata.o atascsi.o mainbus.o bios.o mpbios.o \
	mpbios_intr_fixup.o cpu.o lapic.o ioapic.o pci.o pci_map.o \
	pci_quirks.o pci_subr.o vga_pci.o vga_pci_common.o \
	cy82c693.o ahc_pci.o ahd_pci.o adw_pci.o adwlib.o \
	adwmcode.o twe_pci.o arc.o jmb.o ahci_pci.o ami_pci.o \
	mfi_pci.o mfii.o ips.o eap.o auacer.o auich.o azalia.o \
	azalia_codec.o envy.o emuxki.o auixp.o cs4280.o yds.o \
	auvia.o gdt_pci.o ciss_pci.o qlw_pci.o qla_pci.o qle.o \
	mpi_pci.o mpii.o sili_pci.o if_de.o if_ep_pci.o if_pcn.o \
	siop_pci_common.o siop_pci.o pciide.o ppb.o if_lmc.o \
	if_lmc_common.o if_lmc_media.o if_lmc_obsd.o if_rl_pci.o \
	if_re_pci.o if_vr.o if_txp.o bktr_audio.o bktr_card.o \
	bktr_core.o bktr_os.o bktr_tuner.o if_xl_pci.o if_fxp_pci.o \
	if_em.o if_em_hw.o if_em_soc.o if_ixgb.o ixgb_ee.o \
	ixgb_hw.o if_ix.o ixgbe.o ixgbe_82598.o ixgbe_82599.o \
	ixgbe_x540.o ixgbe_phy.o if_xge.o if_tht.o if_myx.o \
	if_oce.o if_dc_pci.o if_epic_pci.o if_ti_pci.o if_ne_pci.o \
	if_gem_pci.o lofn.o hifn7751.o nofn.o ubsec.o safe.o \
	if_sf_pci.o if_sis.o if_se.o uhci_pci.o ohci_pci.o \
	ehci_pci.o pccbb.o if_sk.o if_msk.o puc.o pucdata.o \
	com_puc.o lpt_puc.o if_wi_pci.o if_an_pci.o if_iwi.o \
	if_wpi.o if_iwn.o cmpci.o iha_pci.o pcscp.o if_bge.o \
	if_bnx.o if_vge.o if_stge.o if_nfe.o if_et.o if_jme.o \
	if_age.o if_alc.o if_ale.o amdpm.o if_bce.o if_ath_pci.o \
	if_athn_pci.o if_atw_pci.o if_rtw_pci.o if_ral_pci.o \
	if_acx_pci.o if_pgt_pci.o if_malo_pci.o if_bwi_pci.o \
	if_sandrv.o if_san_common.o if_san_obsd.o if_san_te1.o \
	if_san_xilinx.o piixpm.o musycc.o musycc_obsd.o if_art.o \
	bt8370.o if_vic.o if_vmx.o vmwpvs.o if_lii.o ichiic.o \
	viapm.o amdiic.o nviic.o sdhc_pci.o kate.o km.o itherm.o \
	rtsx_pci.o virtio.o virtio_pci.o if_vio.o vioblk.o viomb.o \
	viornd.o vioscsi.o agp.o agp_i810.o drm_agpsupport.o \
	drm_drv.o drm_global.o drm_hashtab.o drm_irq.o drm_memory.o \
	drm_mm.o drm_crtc.o drm_modes.o drm_crtc_helper.o drm_edid.o \
	drm_dp_helper.o drm_fb_helper.o ttm_agp_backend.o ttm_bo.o \
	ttm_bo_manager.o ttm_bo_util.o ttm_bo_vm.o ttm_execbuf_util.o \
	ttm_lock.o ttm_memory.o ttm_object.o ttm_page_alloc.o \
	ttm_tt.o i915_dma.o i915_drv.o i915_gem.o i915_gem_context.o \
	i915_gem_evict.o i915_gem_execbuffer.o i915_gem_gtt.o \
	i915_gem_tiling.o i915_irq.o i915_suspend.o intel_bios.o \
	intel_crt.o intel_ddi.o intel_display.o intel_dp.o \
	intel_dvo.o intel_fb.o intel_hdmi.o intel_i2c.o intel_lvds.o \
	intel_modes.o intel_opregion.o intel_overlay.o intel_panel.o \
	intel_pm.o intel_ringbuffer.o intel_sdvo.o intel_sprite.o \
	intel_tv.o dvo_ch7017.o dvo_ch7xxx.o dvo_ivch.o dvo_ns2501.o \
	dvo_sil164.o dvo_tfp410.o atom.o atombios_crtc.o \
	atombios_dp.o atombios_encoders.o atombios_i2c.o \
	cayman_blit_shaders.o evergreen.o evergreen_blit_kms.o \
	evergreen_blit_shaders.o evergreen_cs.o evergreen_hdmi.o ni.o \
	r100.o r200.o r300.o r420.o r520.o r600.o r600_audio.o \
	r600_blit_kms.o r600_blit_shaders.o r600_cs.o r600_hdmi.o \
	radeon_agp.o radeon_asic.o radeon_atombios.o \
	radeon_benchmark.o radeon_bios.o radeon_clocks.o \
	radeon_combios.o radeon_connectors.o radeon_cs.o \
	radeon_cursor.o radeon_device.o radeon_display.o \
	radeon_encoders.o radeon_fb.o radeon_fence.o radeon_gart.o \
	radeon_gem.o radeon_i2c.o radeon_irq_kms.o radeon_kms.o \
	radeon_legacy_crtc.o radeon_legacy_encoders.o \
	radeon_legacy_tv.o radeon_object.o radeon_pm.o radeon_ring.o \
	radeon_sa.o radeon_semaphore.o radeon_test.o radeon_ttm.o \
	rs400.o rs600.o rs690.o rv515.o rv770.o si.o \
	si_blit_shaders.o pci_machdep.o pciide_machdep.o vga_post.o \
	pchb.o amas.o agp_machdep.o cardslot.o cardbus.o \
	cardbus_map.o cardbus_exrom.o rbus.o com_cardbus.o \
	if_xl_cardbus.o if_dc_cardbus.o if_fxp_cardbus.o \
	if_rl_cardbus.o if_re_cardbus.o if_ath_cardbus.o \
	if_athn_cardbus.o if_atw_cardbus.o if_rtw_cardbus.o \
	if_ral_cardbus.o if_acx_cardbus.o if_pgt_cardbus.o \
	ehci_cardbus.o ohci_cardbus.o uhci_cardbus.o \
	if_malo_cardbus.o if_bwi_cardbus.o rbus_machdep.o pcmcia.o \
	pcmcia_cis.o pcmcia_cis_quirks.o if_ep_pcmcia.o \
	if_ne_pcmcia.o aic_pcmcia.o com_pcmcia.o wdc_pcmcia.o \
	if_sm_pcmcia.o if_xe.o if_cnw.o if_wi_pcmcia.o if_malo.o \
	if_an_pcmcia.o pcib.o amdpcib.o tcpcib.o aapic.o hme.o \
	if_hme_pci.o isa.o isadma.o com_isa.o pckbc_isa.o vga_isa.o \
	wdc_isa.o mpu401.o mpu_isa.o pcppi.o spkr.o lpt_isa.o \
	wbsio.o sch311x.o lm78_isa.o it.o uguru.o aps.o \
	isa_machdep.o wsdisplay.o wsdisplay_compat_usl.o wsevent.o \
	wskbd.o wskbdutil.o wsmouse.o wsmux.o wsemulconf.o \
	wsemul_subr.o wsemul_vt100.o wsemul_vt100_subr.o \
	wsemul_vt100_chars.o wsemul_vt100_keys.o pckbd.o \
	wskbdmap_mfii.o pms.o wscons_machdep.o fdc.o fd.o pctr.o \
	nvram.o usb.o usbdi.o usbdi_util.o usb_mem.o usb_subr.o \
	usb_quirks.o uhub.o uaudio.o uvideo.o udl.o umidi.o \
	umidi_quirks.o ucom.o ugen.o hid.o uhidev.o uhid.o \
	hidkbd.o ukbdmap.o ukbd.o hidms.o ums.o uts.o ubcmtp.o \
	ucycom.o ulpt.o umass.o umass_quirks.o umass_scsi.o uthum.o \
	ugold.o utrh.o uoak_subr.o uoakrh.o uoaklux.o uoakv.o \
	udcf.o urio.o uvisor.o udsbr.o utwitch.o if_aue.o if_axe.o \
	if_axen.o if_smsc.o if_cue.o if_kue.o if_cdce.o if_urndis.o \
	if_mos.o if_udav.o if_upl.o if_ugl.o if_url.o umodem.o \
	uftdi.o uplcom.o umct.o uvscom.o ubsa.o uslcom.o uark.o \
	moscom.o uipaq.o umsm.o uchcom.o uticom.o if_wi_usb.o \
	if_atu.o if_ral.o if_rum.o if_run.o if_zyd.o if_upgt.o \
	if_urtw.o if_urtwn.o if_rsu.o if_otus.o if_uath.o \
	if_athn_usb.o uow.o uberry.o upd.o i2c.o i2c_exec.o \
	i2c_scan.o i2c_bitbang.o lm75.o lm93.o lm87.o maxim6690.o \
	ad741x.o adm1021.o adm1024.o adm1025.o adm1030.o adm1031.o \
	ds1631.o adt7460.o lm78_i2c.o adm1026.o w83793g.o w83795g.o \
	asc7621.o asc7611.o spdmem_i2c.o sdtemp.o lis331dl.o acpi.o \
	acpiutil.o dsdt.o acpidebug.o acpitimer.o acpiac.o \
	acpibat.o acpibtn.o acpicpu.o acpihpet.o acpiec.o acpitz.o \
	acpimadt.o acpimcfg.o acpiprt.o acpidock.o acpiasus.o \
	acpithinkpad.o acpitoshiba.o acpisony.o acpivideo.o \
	acpivout.o acpipwrres.o atk0110.o acpi_machdep.o \
	acpi_wakecode.o sdmmc.o sdmmc_cis.o sdmmc_io.o sdmmc_mem.o \
	sdmmc_scsi.o onewire.o onewire_subr.o owid.o owsbm.o \
	owtemp.o owctr.o

CFILES=	$S/dev/ic/smc93cx6.c $S/dev/ic/pcdisplay_subr.c \
	$S/dev/ic/pcdisplay_chars.c $S/dev/ic/vga.c $S/dev/ic/vga_subr.c \
	$S/dev/videomode/edid.c $S/dev/videomode/vesagtf.c \
	$S/dev/videomode/videomode.c $S/dev/mii/mii_bitbang.c \
	$S/dev/ic/wdc.c $S/dev/ic/aic7xxx.c $S/dev/ic/aic7xxx_openbsd.c \
	$S/dev/ic/aic7xxx_seeprom.c $S/dev/ic/aic79xx.c \
	$S/dev/ic/aic79xx_openbsd.c $S/dev/ic/aic6360.c $S/dev/ic/adw.c \
	$S/dev/ic/gdt_common.c $S/dev/ic/twe.c $S/dev/ic/ciss.c \
	$S/dev/ic/ami.c $S/dev/ic/mfi.c $S/dev/ic/qlw.c $S/dev/ic/qla.c \
	$S/dev/microcode/isp/isp_fw2100.c \
	$S/dev/microcode/isp/isp_fw2200.c \
	$S/dev/microcode/isp/isp_fw2300.c $S/dev/ic/ahci.c \
	$S/dev/ic/mpi.c $S/dev/ic/sili.c $S/dev/ic/ncr53c9x.c \
	$S/dev/ic/siop_common.c $S/dev/ic/siop.c $S/dev/ic/elink3.c \
	$S/dev/ic/if_wi.c $S/dev/ic/if_wi_hostap.c $S/dev/ic/an.c \
	$S/dev/ic/xl.c $S/dev/ic/fxp.c $S/dev/ic/rtl81x9.c $S/dev/ic/re.c \
	$S/dev/ic/dc.c $S/dev/ic/smc91cxx.c $S/dev/ic/smc83c170.c \
	$S/dev/ic/ne2000.c $S/dev/ic/dl10019.c $S/dev/ic/ax88190.c \
	$S/dev/ic/gem.c $S/dev/ic/ti.c $S/dev/ic/com.c $S/dev/ic/pckbc.c \
	$S/dev/ic/ac97.c $S/dev/ic/lpt.c $S/dev/ic/iha.c $S/dev/ic/lm78.c \
	$S/dev/ic/ar5xxx.c $S/dev/ic/ar5210.c $S/dev/ic/ar5211.c \
	$S/dev/ic/ar5212.c $S/dev/ic/ath.c $S/dev/ic/athn.c \
	$S/dev/ic/ar5008.c $S/dev/ic/ar5416.c $S/dev/ic/ar9280.c \
	$S/dev/ic/ar9285.c $S/dev/ic/ar9287.c $S/dev/ic/ar9003.c \
	$S/dev/ic/ar9380.c $S/dev/ic/atw.c $S/dev/ic/rtw.c \
	$S/dev/ic/rt2560.c $S/dev/ic/rt2661.c $S/dev/ic/rt2860.c \
	$S/dev/ic/acx.c $S/dev/ic/acx111.c $S/dev/ic/acx100.c \
	$S/dev/ic/pgt.c $S/dev/ic/aic6915.c $S/dev/ic/malo.c \
	$S/dev/ic/bwi.c $S/dev/usb/uhci.c $S/dev/usb/ohci.c \
	$S/dev/usb/ehci.c $S/dev/sdmmc/sdhc.c $S/dev/ic/rtsx.c \
	$S/dev/radio.c $S/dev/ipmi.c $S/dev/vmt.c $S/dev/vscsi.c \
	$S/scsi/mpath.c $S/dev/softraid.c $S/dev/softraid_concat.c \
	$S/dev/softraid_crypto.c $S/dev/softraid_raid0.c \
	$S/dev/softraid_raid1.c $S/dev/softraid_raid5.c \
	$S/dev/softraid_raid6.c $S/dev/spdmem.c $S/dev/ksyms.c \
	$S/miscfs/fuse/fuse_device.c $S/miscfs/fuse/fuse_file.c \
	$S/miscfs/fuse/fuse_lookup.c $S/miscfs/fuse/fuse_vfsops.c \
	$S/miscfs/fuse/fuse_vnops.c $S/miscfs/fuse/fusebuf.c $S/net/pf.c \
	$S/net/pf_norm.c $S/net/pf_ruleset.c $S/net/pf_ioctl.c \
	$S/net/pf_table.c $S/net/pf_osfp.c $S/net/pf_if.c $S/net/pf_lb.c \
	$S/net/hfsc.c $S/net/if_pflog.c $S/net/if_pfsync.c \
	$S/net/if_pflow.c $S/dev/bio.c $S/dev/hotplug.c $S/net/if_pppoe.c \
	$S/ddb/db_access.c $S/ddb/db_break.c $S/ddb/db_command.c \
	$S/ddb/db_elf.c $S/ddb/db_examine.c $S/ddb/db_expr.c \
	$S/ddb/db_hangman.c $S/ddb/db_input.c $S/ddb/db_lex.c \
	$S/ddb/db_output.c $S/ddb/db_print.c $S/ddb/db_run.c \
	$S/ddb/db_sym.c $S/ddb/db_trap.c $S/ddb/db_variables.c \
	$S/ddb/db_watch.c $S/ddb/db_write_cmd.c $S/ddb/db_usrreq.c \
	$S/dev/auconv.c $S/dev/audio.c $S/dev/cons.c $S/dev/diskmap.c \
	$S/dev/firmload.c $S/dev/ic/dp8390.c $S/dev/ic/rtl80x9.c \
	$S/dev/midi.c $S/dev/mulaw.c $S/dev/systrace.c $S/dev/vnd.c \
	$S/dev/rnd.c $S/dev/video.c $S/isofs/cd9660/cd9660_bmap.c \
	$S/isofs/cd9660/cd9660_lookup.c $S/isofs/cd9660/cd9660_node.c \
	$S/isofs/cd9660/cd9660_rrip.c $S/isofs/cd9660/cd9660_util.c \
	$S/isofs/cd9660/cd9660_vfsops.c $S/isofs/cd9660/cd9660_vnops.c \
	$S/isofs/udf/udf_subr.c $S/isofs/udf/udf_vfsops.c \
	$S/isofs/udf/udf_vnops.c $S/kern/clock_subr.c $S/kern/exec_conf.c \
	$S/kern/exec_elf32.c $S/kern/exec_elf64.c $S/kern/exec_script.c \
	$S/kern/exec_subr.c $S/kern/init_main.c $S/kern/init_sysent.c \
	$S/kern/kern_acct.c $S/kern/kern_bufq.c $S/kern/kern_clock.c \
	$S/kern/kern_descrip.c $S/kern/kern_event.c $S/kern/kern_exec.c \
	$S/kern/kern_exit.c $S/kern/kern_fork.c $S/kern/kern_kthread.c \
	$S/kern/kern_ktrace.c $S/kern/kern_lock.c $S/kern/kern_lkm.c \
	$S/kern/kern_malloc.c $S/kern/kern_rwlock.c $S/kern/kern_physio.c \
	$S/kern/kern_proc.c $S/kern/kern_prot.c $S/kern/kern_resource.c \
	$S/kern/kern_sched.c $S/kern/kern_sensors.c $S/kern/kern_sig.c \
	$S/kern/kern_subr.c $S/kern/kern_sysctl.c $S/kern/kern_synch.c \
	$S/kern/kern_tc.c $S/kern/kern_time.c $S/kern/kern_timeout.c \
	$S/kern/kern_watchdog.c $S/kern/kern_workq.c $S/kern/kern_task.c \
	$S/kern/kern_xxx.c $S/kern/kern_uuid.c $S/kern/kern_objcache.c \
	$S/kern/sched_bsd.c $S/kern/subr_autoconf.c $S/kern/subr_disk.c \
	$S/kern/subr_evcount.c $S/kern/subr_extent.c \
	$S/kern/subr_hibernate.c $S/kern/subr_log.c $S/kern/subr_poison.c \
	$S/kern/subr_pool.c $S/kern/dma_alloc.c $S/kern/subr_prf.c \
	$S/kern/subr_prof.c $S/kern/subr_userconf.c $S/kern/subr_xxx.c \
	$S/kern/sys_generic.c $S/kern/sys_pipe.c $S/kern/sys_process.c \
	$S/kern/sys_socket.c $S/kern/sysv_ipc.c $S/kern/sysv_msg.c \
	$S/kern/sysv_sem.c $S/kern/sysv_shm.c $S/kern/tty.c \
	$S/kern/tty_conf.c $S/kern/tty_pty.c $S/kern/tty_nmea.c \
	$S/kern/tty_msts.c $S/kern/tty_endrun.c $S/kern/tty_subr.c \
	$S/kern/tty_tty.c $S/kern/uipc_domain.c $S/kern/uipc_mbuf.c \
	$S/kern/uipc_mbuf2.c $S/kern/uipc_proto.c $S/kern/uipc_socket.c \
	$S/kern/uipc_socket2.c $S/kern/uipc_syscalls.c \
	$S/kern/uipc_usrreq.c $S/kern/vfs_bio.c $S/kern/vfs_biomem.c \
	$S/kern/vfs_cache.c $S/kern/vfs_cluster.c $S/kern/vfs_default.c \
	$S/kern/vfs_init.c $S/kern/vfs_lockf.c $S/kern/vfs_lookup.c \
	$S/kern/vfs_subr.c $S/kern/vfs_sync.c $S/kern/vfs_syscalls.c \
	$S/kern/vfs_vops.c $S/kern/vfs_vnops.c $S/kern/vfs_getcwd.c \
	$S/kern/spec_vnops.c $S/miscfs/deadfs/dead_vnops.c \
	$S/miscfs/fifofs/fifo_vnops.c $S/msdosfs/msdosfs_conv.c \
	$S/msdosfs/msdosfs_denode.c $S/msdosfs/msdosfs_fat.c \
	$S/msdosfs/msdosfs_lookup.c $S/msdosfs/msdosfs_vfsops.c \
	$S/msdosfs/msdosfs_vnops.c $S/hammer2/hammer2_ccms.c \
	$S/hammer2/hammer2_chain.c $S/hammer2/hammer2_cluster.c \
	$S/hammer2/hammer2_flush.c $S/hammer2/hammer2_freemap.c \
	$S/hammer2/hammer2_inode.c $S/hammer2/hammer2_io.c \
	$S/hammer2/hammer2_ioctl.c $S/hammer2/hammer2_lz4.c \
	$S/hammer2/hammer2_msgops.c $S/hammer2/hammer2_subr.c \
	$S/hammer2/hammer2_vfsops.c $S/hammer2/hammer2_vnops.c \
	$S/ntfs/ntfs_compr.c $S/ntfs/ntfs_conv.c $S/ntfs/ntfs_ihash.c \
	$S/ntfs/ntfs_subr.c $S/ntfs/ntfs_vfsops.c $S/ntfs/ntfs_vnops.c \
	$S/tmpfs/tmpfs_mem.c $S/tmpfs/tmpfs_subr.c \
	$S/tmpfs/tmpfs_vfsops.c $S/tmpfs/tmpfs_vnops.c \
	$S/tmpfs/tmpfs_specops.c $S/tmpfs/tmpfs_fifoops.c $S/net/bpf.c \
	$S/net/bpf_filter.c $S/net/if.c $S/net/if_ethersubr.c \
	$S/net/if_spppsubr.c $S/net/if_loop.c $S/net/if_media.c \
	$S/net/if_sl.c $S/net/if_ppp.c $S/net/netisr.c $S/net/ppp_tty.c \
	$S/net/bsd-comp.c $S/net/ppp-deflate.c $S/net/if_tun.c \
	$S/net/if_bridge.c $S/net/bridgestp.c $S/net/if_vlan.c \
	$S/net/pipex.c $S/net/radix.c $S/net/radix_mpath.c \
	$S/net/raw_cb.c $S/net/raw_usrreq.c $S/net/route.c \
	$S/net/rtsock.c $S/net/slcompress.c $S/net/if_enc.c \
	$S/net/if_gre.c $S/net/if_trunk.c $S/net/trunklacp.c \
	$S/net/if_mpe.c $S/net/if_vether.c $S/net/if_pppx.c \
	$S/net/if_vxlan.c $S/net80211/ieee80211.c \
	$S/net80211/ieee80211_amrr.c $S/net80211/ieee80211_crypto.c \
	$S/net80211/ieee80211_crypto_bip.c \
	$S/net80211/ieee80211_crypto_ccmp.c \
	$S/net80211/ieee80211_crypto_tkip.c \
	$S/net80211/ieee80211_crypto_wep.c $S/net80211/ieee80211_input.c \
	$S/net80211/ieee80211_ioctl.c $S/net80211/ieee80211_node.c \
	$S/net80211/ieee80211_output.c $S/net80211/ieee80211_pae_input.c \
	$S/net80211/ieee80211_pae_output.c $S/net80211/ieee80211_proto.c \
	$S/net80211/ieee80211_rssadapt.c \
	$S/net80211/ieee80211_regdomain.c $S/netinet/if_ether.c \
	$S/netinet/igmp.c $S/netinet/in.c $S/netinet/in_pcb.c \
	$S/netinet/in_proto.c $S/netinet/inet_nat64.c \
	$S/netinet/inet_ntop.c $S/netinet/ip_divert.c \
	$S/netinet/ip_icmp.c $S/netinet/ip_id.c $S/netinet/ip_input.c \
	$S/netinet/ip_mroute.c $S/netinet/ip_output.c $S/netinet/raw_ip.c \
	$S/netinet/tcp_debug.c $S/netinet/tcp_input.c \
	$S/netinet/tcp_output.c $S/netinet/tcp_subr.c \
	$S/netinet/tcp_timer.c $S/netinet/tcp_usrreq.c \
	$S/netinet/udp_usrreq.c $S/netinet/ip_gre.c $S/netinet/ip_ipsp.c \
	$S/netinet/ip_spd.c $S/netinet/ip_ipip.c $S/netinet/ip_ether.c \
	$S/netinet/ipsec_input.c $S/netinet/ipsec_output.c \
	$S/netinet/ip_esp.c $S/netinet/ip_ah.c $S/netinet/ip_carp.c \
	$S/netinet/ip_ipcomp.c $S/crypto/rijndael.c $S/crypto/md5.c \
	$S/crypto/rmd160.c $S/crypto/sha1.c $S/crypto/sha2.c \
	$S/crypto/blf.c $S/crypto/cast.c $S/crypto/ecb_enc.c \
	$S/crypto/set_key.c $S/crypto/ecb3_enc.c $S/crypto/crypto.c \
	$S/crypto/cryptodev.c $S/crypto/criov.c $S/crypto/cryptosoft.c \
	$S/crypto/xform.c $S/crypto/xform_ipcomp.c $S/crypto/arc4.c \
	$S/crypto/michael.c $S/crypto/cmac.c $S/crypto/hmac.c \
	$S/crypto/gmac.c $S/crypto/key_wrap.c $S/crypto/idgen.c \
	$S/netmpls/mpls_input.c $S/netmpls/mpls_output.c \
	$S/netmpls/mpls_proto.c $S/netmpls/mpls_raw.c \
	$S/netmpls/mpls_shim.c $S/nfs/krpc_subr.c $S/nfs/nfs_bio.c \
	$S/nfs/nfs_boot.c $S/nfs/nfs_debug.c $S/nfs/nfs_node.c \
	$S/nfs/nfs_kq.c $S/nfs/nfs_serv.c $S/nfs/nfs_socket.c \
	$S/nfs/nfs_srvcache.c $S/nfs/nfs_subs.c $S/nfs/nfs_syscalls.c \
	$S/nfs/nfs_vfsops.c $S/nfs/nfs_vnops.c $S/ufs/ffs/ffs_alloc.c \
	$S/ufs/ffs/ffs_balloc.c $S/ufs/ffs/ffs_inode.c \
	$S/ufs/ffs/ffs_subr.c $S/ufs/ffs/ffs_softdep_stub.c \
	$S/ufs/ffs/ffs_tables.c $S/ufs/ffs/ffs_vfsops.c \
	$S/ufs/ffs/ffs_vnops.c $S/ufs/ffs/ffs_softdep.c \
	$S/ufs/mfs/mfs_vfsops.c $S/ufs/mfs/mfs_vnops.c \
	$S/ufs/ufs/ufs_bmap.c $S/ufs/ufs/ufs_dirhash.c \
	$S/ufs/ufs/ufs_ihash.c $S/ufs/ufs/ufs_inode.c \
	$S/ufs/ufs/ufs_lookup.c $S/ufs/ufs/ufs_quota.c \
	$S/ufs/ufs/ufs_quota_stub.c $S/ufs/ufs/ufs_vfsops.c \
	$S/ufs/ufs/ufs_vnops.c $S/ufs/ext2fs/ext2fs_alloc.c \
	$S/ufs/ext2fs/ext2fs_balloc.c $S/ufs/ext2fs/ext2fs_bmap.c \
	$S/ufs/ext2fs/ext2fs_bswap.c $S/ufs/ext2fs/ext2fs_inode.c \
	$S/ufs/ext2fs/ext2fs_lookup.c $S/ufs/ext2fs/ext2fs_readwrite.c \
	$S/ufs/ext2fs/ext2fs_subr.c $S/ufs/ext2fs/ext2fs_vfsops.c \
	$S/ufs/ext2fs/ext2fs_vnops.c $S/uvm/uvm_addr.c $S/uvm/uvm_amap.c \
	$S/uvm/uvm_anon.c $S/uvm/uvm_aobj.c $S/uvm/uvm_device.c \
	$S/uvm/uvm_fault.c $S/uvm/uvm_glue.c $S/uvm/uvm_init.c \
	$S/uvm/uvm_io.c $S/uvm/uvm_km.c $S/uvm/uvm_map.c \
	$S/uvm/uvm_meter.c $S/uvm/uvm_mmap.c $S/uvm/uvm_object.c \
	$S/uvm/uvm_page.c $S/uvm/uvm_pager.c $S/uvm/uvm_pdaemon.c \
	$S/uvm/uvm_pmemrange.c $S/uvm/uvm_stat.c $S/uvm/uvm_swap.c \
	$S/uvm/uvm_swap_encrypt.c $S/uvm/uvm_unix.c $S/uvm/uvm_user.c \
	$S/uvm/uvm_vnode.c $S/net/if_gif.c $S/netinet/ip_ecn.c \
	$S/netinet/in_gif.c $S/netinet6/in6_gif.c $S/netinet6/in6_pcb.c \
	$S/netinet6/in6.c $S/netinet6/ip6_divert.c \
	$S/netinet6/in6_ifattach.c $S/netinet6/in6_cksum.c \
	$S/netinet6/in6_src.c $S/netinet6/in6_proto.c $S/netinet6/dest6.c \
	$S/netinet6/frag6.c $S/netinet6/icmp6.c $S/netinet6/ip6_id.c \
	$S/netinet6/ip6_input.c $S/netinet6/ip6_forward.c \
	$S/netinet6/ip6_mroute.c $S/netinet6/ip6_output.c \
	$S/netinet6/route6.c $S/netinet6/mld6.c $S/netinet6/nd6.c \
	$S/netinet6/nd6_nbr.c $S/netinet6/nd6_rtr.c $S/netinet6/raw_ip6.c \
	$S/netinet6/udp6_output.c $S/net/pfkey.c $S/net/pfkeyv2.c \
	$S/net/pfkeyv2_parsemessage.c $S/net/pfkeyv2_convert.c \
	$S/compat/common/compat_util.c $S/dev/x86emu/x86emu.c \
	$S/dev/x86emu/x86emu_util.c $S/lib/libkern/getsn.c \
	$S/lib/libkern/random.c $S/lib/libkern/explicit_bzero.c \
	$S/lib/libkern/timingsafe_bcmp.c $S/lib/libkern/__main.c \
	$S/lib/libkern/imax.c $S/lib/libkern/imin.c $S/lib/libkern/lmax.c \
	$S/lib/libkern/lmin.c $S/lib/libkern/max.c $S/lib/libkern/min.c \
	$S/lib/libkern/ulmax.c $S/lib/libkern/ulmin.c \
	$S/lib/libkern/fls.c $S/lib/libkern/flsl.c \
	$S/lib/libkern/strlcat.c $S/lib/libkern/strlcpy.c \
	$S/lib/libkern/strlen.c $S/lib/libkern/strncmp.c \
	$S/lib/libkern/strncpy.c $S/lib/libkern/strnlen.c \
	$S/lib/libkern/strncasecmp.c $S/lib/libz/adler32.c \
	$S/lib/libz/crc32.c $S/lib/libz/infback.c $S/lib/libz/inffast.c \
	$S/lib/libz/inflate.c $S/lib/libz/inftrees.c \
	$S/lib/libz/deflate.c $S/lib/libz/zutil.c $S/lib/libz/zopenbsd.c \
	$S/lib/libz/trees.c $S/lib/libz/compress.c \
	$S/arch/amd64/amd64/autoconf.c $S/arch/amd64/amd64/conf.c \
	$S/arch/amd64/amd64/disksubr.c $S/arch/amd64/amd64/machdep.c \
	$S/arch/amd64/amd64/hibernate_machdep.c \
	$S/arch/amd64/amd64/identcpu.c $S/arch/amd64/amd64/via.c \
	$S/arch/amd64/amd64/aesni.c $S/arch/amd64/amd64/amd64errata.c \
	$S/arch/amd64/amd64/mem.c $S/arch/amd64/amd64/amd64_mem.c \
	$S/arch/amd64/amd64/mtrr.c $S/arch/amd64/amd64/pmap.c \
	$S/arch/amd64/amd64/process_machdep.c \
	$S/arch/amd64/amd64/sys_machdep.c $S/arch/amd64/amd64/trap.c \
	$S/arch/amd64/amd64/vm_machdep.c $S/arch/amd64/amd64/fpu.c \
	$S/arch/amd64/amd64/softintr.c $S/arch/amd64/amd64/i8259.c \
	$S/arch/amd64/amd64/cacheinfo.c $S/arch/amd64/amd64/intr.c \
	$S/arch/amd64/amd64/bus_space.c $S/arch/amd64/amd64/bus_dma.c \
	$S/arch/amd64/amd64/apic.c $S/arch/amd64/amd64/consinit.c \
	$S/dev/cninit.c $S/arch/amd64/amd64/dkcsum.c \
	$S/arch/amd64/amd64/db_disasm.c \
	$S/arch/amd64/amd64/db_interface.c $S/arch/amd64/amd64/db_memrw.c \
	$S/arch/amd64/amd64/db_trace.c $S/netinet/in_cksum.c \
	$S/netinet/in4_cksum.c $S/arch/amd64/isa/clock.c \
	$S/arch/amd64/amd64/powernow-k8.c $S/arch/amd64/amd64/est.c \
	$S/arch/amd64/amd64/k1x-pstate.c $S/dev/rasops/rasops.c \
	$S/dev/rasops/rasops8.c $S/dev/rasops/rasops15.c \
	$S/dev/rasops/rasops32.c $S/dev/wsfont/wsfont.c $S/dev/mii/mii.c \
	$S/dev/mii/mii_physubr.c $S/dev/mii/ukphy_subr.c \
	$S/dev/mii/nsphy.c $S/dev/mii/nsphyter.c $S/dev/mii/gentbi.c \
	$S/dev/mii/qsphy.c $S/dev/mii/inphy.c $S/dev/mii/iophy.c \
	$S/dev/mii/eephy.c $S/dev/mii/exphy.c $S/dev/mii/rlphy.c \
	$S/dev/mii/lxtphy.c $S/dev/mii/luphy.c $S/dev/mii/mtdphy.c \
	$S/dev/mii/icsphy.c $S/dev/mii/sqphy.c $S/dev/mii/tqphy.c \
	$S/dev/mii/ukphy.c $S/dev/mii/dcphy.c $S/dev/mii/bmtphy.c \
	$S/dev/mii/brgphy.c $S/dev/mii/xmphy.c $S/dev/mii/amphy.c \
	$S/dev/mii/acphy.c $S/dev/mii/nsgphy.c $S/dev/mii/urlphy.c \
	$S/dev/mii/rgephy.c $S/dev/mii/ciphy.c $S/dev/mii/ipgphy.c \
	$S/dev/mii/etphy.c $S/dev/mii/jmphy.c $S/dev/mii/atphy.c \
	$S/scsi/scsi_base.c $S/scsi/scsi_ioctl.c $S/scsi/scsiconf.c \
	$S/scsi/cd.c $S/scsi/ch.c $S/scsi/sd.c $S/scsi/st.c $S/scsi/uk.c \
	$S/scsi/safte.c $S/scsi/ses.c $S/scsi/mpath_sym.c \
	$S/scsi/mpath_rdac.c $S/scsi/mpath_emc.c $S/scsi/mpath_hds.c \
	$S/dev/atapiscsi/atapiscsi.c $S/dev/ata/wd.c $S/dev/ata/ata_wdc.c \
	$S/dev/ata/ata.c $S/dev/ata/atascsi.c \
	$S/arch/amd64/amd64/mainbus.c $S/arch/amd64/amd64/bios.c \
	$S/arch/amd64/amd64/mpbios.c \
	$S/arch/amd64/amd64/mpbios_intr_fixup.c $S/arch/amd64/amd64/cpu.c \
	$S/arch/amd64/amd64/lapic.c $S/arch/amd64/amd64/ioapic.c \
	$S/dev/pci/pci.c $S/dev/pci/pci_map.c $S/dev/pci/pci_quirks.c \
	$S/dev/pci/pci_subr.c $S/dev/pci/vga_pci.c \
	$S/dev/pci/vga_pci_common.c $S/dev/pci/cy82c693.c \
	$S/dev/pci/ahc_pci.c $S/dev/pci/ahd_pci.c $S/dev/pci/adw_pci.c \
	$S/dev/ic/adwlib.c $S/dev/microcode/adw/adwmcode.c \
	$S/dev/pci/twe_pci.c $S/dev/pci/arc.c $S/dev/pci/jmb.c \
	$S/dev/pci/ahci_pci.c $S/dev/pci/ami_pci.c $S/dev/pci/mfi_pci.c \
	$S/dev/pci/mfii.c $S/dev/pci/ips.c $S/dev/pci/eap.c \
	$S/dev/pci/auacer.c $S/dev/pci/auich.c $S/dev/pci/azalia.c \
	$S/dev/pci/azalia_codec.c $S/dev/pci/envy.c $S/dev/pci/emuxki.c \
	$S/dev/pci/auixp.c $S/dev/pci/cs4280.c $S/dev/pci/yds.c \
	$S/dev/pci/auvia.c $S/dev/pci/gdt_pci.c $S/dev/pci/ciss_pci.c \
	$S/dev/pci/qlw_pci.c $S/dev/pci/qla_pci.c $S/dev/pci/qle.c \
	$S/dev/pci/mpi_pci.c $S/dev/pci/mpii.c $S/dev/pci/sili_pci.c \
	$S/dev/pci/if_de.c $S/dev/pci/if_ep_pci.c $S/dev/pci/if_pcn.c \
	$S/dev/pci/siop_pci_common.c $S/dev/pci/siop_pci.c \
	$S/dev/pci/pciide.c $S/dev/pci/ppb.c $S/dev/pci/if_lmc.c \
	$S/dev/pci/if_lmc_common.c $S/dev/pci/if_lmc_media.c \
	$S/dev/pci/if_lmc_obsd.c $S/dev/pci/if_rl_pci.c \
	$S/dev/pci/if_re_pci.c $S/dev/pci/if_vr.c $S/dev/pci/if_txp.c \
	$S/dev/pci/bktr/bktr_audio.c $S/dev/pci/bktr/bktr_card.c \
	$S/dev/pci/bktr/bktr_core.c $S/dev/pci/bktr/bktr_os.c \
	$S/dev/pci/bktr/bktr_tuner.c $S/dev/pci/if_xl_pci.c \
	$S/dev/pci/if_fxp_pci.c $S/dev/pci/if_em.c $S/dev/pci/if_em_hw.c \
	$S/dev/pci/if_em_soc.c $S/dev/pci/if_ixgb.c $S/dev/pci/ixgb_ee.c \
	$S/dev/pci/ixgb_hw.c $S/dev/pci/if_ix.c $S/dev/pci/ixgbe.c \
	$S/dev/pci/ixgbe_82598.c $S/dev/pci/ixgbe_82599.c \
	$S/dev/pci/ixgbe_x540.c $S/dev/pci/ixgbe_phy.c \
	$S/dev/pci/if_xge.c $S/dev/pci/if_tht.c $S/dev/pci/if_myx.c \
	$S/dev/pci/if_oce.c $S/dev/pci/if_dc_pci.c \
	$S/dev/pci/if_epic_pci.c $S/dev/pci/if_ti_pci.c \
	$S/dev/pci/if_ne_pci.c $S/dev/pci/if_gem_pci.c $S/dev/pci/lofn.c \
	$S/dev/pci/hifn7751.c $S/dev/pci/nofn.c $S/dev/pci/ubsec.c \
	$S/dev/pci/safe.c $S/dev/pci/if_sf_pci.c $S/dev/pci/if_sis.c \
	$S/dev/pci/if_se.c $S/dev/pci/uhci_pci.c $S/dev/pci/ohci_pci.c \
	$S/dev/pci/ehci_pci.c $S/dev/pci/pccbb.c $S/dev/pci/if_sk.c \
	$S/dev/pci/if_msk.c $S/dev/pci/puc.c $S/dev/pci/pucdata.c \
	$S/dev/puc/com_puc.c $S/dev/puc/lpt_puc.c $S/dev/pci/if_wi_pci.c \
	$S/dev/pci/if_an_pci.c $S/dev/pci/if_iwi.c $S/dev/pci/if_wpi.c \
	$S/dev/pci/if_iwn.c $S/dev/pci/cmpci.c $S/dev/pci/iha_pci.c \
	$S/dev/pci/pcscp.c $S/dev/pci/if_bge.c $S/dev/pci/if_bnx.c \
	$S/dev/pci/if_vge.c $S/dev/pci/if_stge.c $S/dev/pci/if_nfe.c \
	$S/dev/pci/if_et.c $S/dev/pci/if_jme.c $S/dev/pci/if_age.c \
	$S/dev/pci/if_alc.c $S/dev/pci/if_ale.c $S/dev/pci/amdpm.c \
	$S/dev/pci/if_bce.c $S/dev/pci/if_ath_pci.c \
	$S/dev/pci/if_athn_pci.c $S/dev/pci/if_atw_pci.c \
	$S/dev/pci/if_rtw_pci.c $S/dev/pci/if_ral_pci.c \
	$S/dev/pci/if_acx_pci.c $S/dev/pci/if_pgt_pci.c \
	$S/dev/pci/if_malo_pci.c $S/dev/pci/if_bwi_pci.c \
	$S/dev/pci/if_sandrv.c $S/dev/pci/if_san_common.c \
	$S/dev/pci/if_san_obsd.c $S/dev/pci/if_san_te1.c \
	$S/dev/pci/if_san_xilinx.c $S/dev/pci/piixpm.c \
	$S/dev/pci/musycc.c $S/dev/pci/musycc_obsd.c $S/dev/pci/if_art.c \
	$S/dev/pci/bt8370.c $S/dev/pci/if_vic.c $S/dev/pci/if_vmx.c \
	$S/dev/pci/vmwpvs.c $S/dev/pci/if_lii.c $S/dev/pci/ichiic.c \
	$S/dev/pci/viapm.c $S/dev/pci/amdiic.c $S/dev/pci/nviic.c \
	$S/dev/pci/sdhc_pci.c $S/dev/pci/kate.c $S/dev/pci/km.c \
	$S/dev/pci/itherm.c $S/dev/pci/rtsx_pci.c $S/dev/pci/virtio.c \
	$S/dev/pci/virtio_pci.c $S/dev/pci/if_vio.c $S/dev/pci/vioblk.c \
	$S/dev/pci/viomb.c $S/dev/pci/viornd.c $S/dev/pci/vioscsi.c \
	$S/dev/pci/agp.c $S/dev/pci/agp_i810.c \
	$S/dev/pci/drm/drm_agpsupport.c $S/dev/pci/drm/drm_drv.c \
	$S/dev/pci/drm/drm_global.c $S/dev/pci/drm/drm_hashtab.c \
	$S/dev/pci/drm/drm_irq.c $S/dev/pci/drm/drm_memory.c \
	$S/dev/pci/drm/drm_mm.c $S/dev/pci/drm/drm_crtc.c \
	$S/dev/pci/drm/drm_modes.c $S/dev/pci/drm/drm_crtc_helper.c \
	$S/dev/pci/drm/drm_edid.c $S/dev/pci/drm/drm_dp_helper.c \
	$S/dev/pci/drm/drm_fb_helper.c \
	$S/dev/pci/drm/ttm/ttm_agp_backend.c $S/dev/pci/drm/ttm/ttm_bo.c \
	$S/dev/pci/drm/ttm/ttm_bo_manager.c \
	$S/dev/pci/drm/ttm/ttm_bo_util.c $S/dev/pci/drm/ttm/ttm_bo_vm.c \
	$S/dev/pci/drm/ttm/ttm_execbuf_util.c \
	$S/dev/pci/drm/ttm/ttm_lock.c $S/dev/pci/drm/ttm/ttm_memory.c \
	$S/dev/pci/drm/ttm/ttm_object.c \
	$S/dev/pci/drm/ttm/ttm_page_alloc.c $S/dev/pci/drm/ttm/ttm_tt.c \
	$S/dev/pci/drm/i915/i915_dma.c $S/dev/pci/drm/i915/i915_drv.c \
	$S/dev/pci/drm/i915/i915_gem.c \
	$S/dev/pci/drm/i915/i915_gem_context.c \
	$S/dev/pci/drm/i915/i915_gem_evict.c \
	$S/dev/pci/drm/i915/i915_gem_execbuffer.c \
	$S/dev/pci/drm/i915/i915_gem_gtt.c \
	$S/dev/pci/drm/i915/i915_gem_tiling.c \
	$S/dev/pci/drm/i915/i915_irq.c $S/dev/pci/drm/i915/i915_suspend.c \
	$S/dev/pci/drm/i915/intel_bios.c $S/dev/pci/drm/i915/intel_crt.c \
	$S/dev/pci/drm/i915/intel_ddi.c \
	$S/dev/pci/drm/i915/intel_display.c \
	$S/dev/pci/drm/i915/intel_dp.c $S/dev/pci/drm/i915/intel_dvo.c \
	$S/dev/pci/drm/i915/intel_fb.c $S/dev/pci/drm/i915/intel_hdmi.c \
	$S/dev/pci/drm/i915/intel_i2c.c $S/dev/pci/drm/i915/intel_lvds.c \
	$S/dev/pci/drm/i915/intel_modes.c \
	$S/dev/pci/drm/i915/intel_opregion.c \
	$S/dev/pci/drm/i915/intel_overlay.c \
	$S/dev/pci/drm/i915/intel_panel.c $S/dev/pci/drm/i915/intel_pm.c \
	$S/dev/pci/drm/i915/intel_ringbuffer.c \
	$S/dev/pci/drm/i915/intel_sdvo.c \
	$S/dev/pci/drm/i915/intel_sprite.c $S/dev/pci/drm/i915/intel_tv.c \
	$S/dev/pci/drm/i915/dvo_ch7017.c $S/dev/pci/drm/i915/dvo_ch7xxx.c \
	$S/dev/pci/drm/i915/dvo_ivch.c $S/dev/pci/drm/i915/dvo_ns2501.c \
	$S/dev/pci/drm/i915/dvo_sil164.c $S/dev/pci/drm/i915/dvo_tfp410.c \
	$S/dev/pci/drm/radeon/atom.c \
	$S/dev/pci/drm/radeon/atombios_crtc.c \
	$S/dev/pci/drm/radeon/atombios_dp.c \
	$S/dev/pci/drm/radeon/atombios_encoders.c \
	$S/dev/pci/drm/radeon/atombios_i2c.c \
	$S/dev/pci/drm/radeon/cayman_blit_shaders.c \
	$S/dev/pci/drm/radeon/evergreen.c \
	$S/dev/pci/drm/radeon/evergreen_blit_kms.c \
	$S/dev/pci/drm/radeon/evergreen_blit_shaders.c \
	$S/dev/pci/drm/radeon/evergreen_cs.c \
	$S/dev/pci/drm/radeon/evergreen_hdmi.c $S/dev/pci/drm/radeon/ni.c \
	$S/dev/pci/drm/radeon/r100.c $S/dev/pci/drm/radeon/r200.c \
	$S/dev/pci/drm/radeon/r300.c $S/dev/pci/drm/radeon/r420.c \
	$S/dev/pci/drm/radeon/r520.c $S/dev/pci/drm/radeon/r600.c \
	$S/dev/pci/drm/radeon/r600_audio.c \
	$S/dev/pci/drm/radeon/r600_blit_kms.c \
	$S/dev/pci/drm/radeon/r600_blit_shaders.c \
	$S/dev/pci/drm/radeon/r600_cs.c $S/dev/pci/drm/radeon/r600_hdmi.c \
	$S/dev/pci/drm/radeon/radeon_agp.c \
	$S/dev/pci/drm/radeon/radeon_asic.c \
	$S/dev/pci/drm/radeon/radeon_atombios.c \
	$S/dev/pci/drm/radeon/radeon_benchmark.c \
	$S/dev/pci/drm/radeon/radeon_bios.c \
	$S/dev/pci/drm/radeon/radeon_clocks.c \
	$S/dev/pci/drm/radeon/radeon_combios.c \
	$S/dev/pci/drm/radeon/radeon_connectors.c \
	$S/dev/pci/drm/radeon/radeon_cs.c \
	$S/dev/pci/drm/radeon/radeon_cursor.c \
	$S/dev/pci/drm/radeon/radeon_device.c \
	$S/dev/pci/drm/radeon/radeon_display.c \
	$S/dev/pci/drm/radeon/radeon_encoders.c \
	$S/dev/pci/drm/radeon/radeon_fb.c \
	$S/dev/pci/drm/radeon/radeon_fence.c \
	$S/dev/pci/drm/radeon/radeon_gart.c \
	$S/dev/pci/drm/radeon/radeon_gem.c \
	$S/dev/pci/drm/radeon/radeon_i2c.c \
	$S/dev/pci/drm/radeon/radeon_irq_kms.c \
	$S/dev/pci/drm/radeon/radeon_kms.c \
	$S/dev/pci/drm/radeon/radeon_legacy_crtc.c \
	$S/dev/pci/drm/radeon/radeon_legacy_encoders.c \
	$S/dev/pci/drm/radeon/radeon_legacy_tv.c \
	$S/dev/pci/drm/radeon/radeon_object.c \
	$S/dev/pci/drm/radeon/radeon_pm.c \
	$S/dev/pci/drm/radeon/radeon_ring.c \
	$S/dev/pci/drm/radeon/radeon_sa.c \
	$S/dev/pci/drm/radeon/radeon_semaphore.c \
	$S/dev/pci/drm/radeon/radeon_test.c \
	$S/dev/pci/drm/radeon/radeon_ttm.c $S/dev/pci/drm/radeon/rs400.c \
	$S/dev/pci/drm/radeon/rs600.c $S/dev/pci/drm/radeon/rs690.c \
	$S/dev/pci/drm/radeon/rv515.c $S/dev/pci/drm/radeon/rv770.c \
	$S/dev/pci/drm/radeon/si.c \
	$S/dev/pci/drm/radeon/si_blit_shaders.c \
	$S/arch/amd64/pci/pci_machdep.c \
	$S/arch/amd64/pci/pciide_machdep.c $S/arch/amd64/pci/vga_post.c \
	$S/arch/amd64/pci/pchb.c $S/dev/pci/amas.c \
	$S/arch/amd64/pci/agp_machdep.c $S/dev/cardbus/cardslot.c \
	$S/dev/cardbus/cardbus.c $S/dev/cardbus/cardbus_map.c \
	$S/dev/cardbus/cardbus_exrom.c $S/dev/cardbus/rbus.c \
	$S/dev/cardbus/com_cardbus.c $S/dev/cardbus/if_xl_cardbus.c \
	$S/dev/cardbus/if_dc_cardbus.c $S/dev/cardbus/if_fxp_cardbus.c \
	$S/dev/cardbus/if_rl_cardbus.c $S/dev/cardbus/if_re_cardbus.c \
	$S/dev/cardbus/if_ath_cardbus.c $S/dev/cardbus/if_athn_cardbus.c \
	$S/dev/cardbus/if_atw_cardbus.c $S/dev/cardbus/if_rtw_cardbus.c \
	$S/dev/cardbus/if_ral_cardbus.c $S/dev/cardbus/if_acx_cardbus.c \
	$S/dev/cardbus/if_pgt_cardbus.c $S/dev/cardbus/ehci_cardbus.c \
	$S/dev/cardbus/ohci_cardbus.c $S/dev/cardbus/uhci_cardbus.c \
	$S/dev/cardbus/if_malo_cardbus.c $S/dev/cardbus/if_bwi_cardbus.c \
	$S/arch/amd64/amd64/rbus_machdep.c $S/dev/pcmcia/pcmcia.c \
	$S/dev/pcmcia/pcmcia_cis.c $S/dev/pcmcia/pcmcia_cis_quirks.c \
	$S/dev/pcmcia/if_ep_pcmcia.c $S/dev/pcmcia/if_ne_pcmcia.c \
	$S/dev/pcmcia/aic_pcmcia.c $S/dev/pcmcia/com_pcmcia.c \
	$S/dev/pcmcia/wdc_pcmcia.c $S/dev/pcmcia/if_sm_pcmcia.c \
	$S/dev/pcmcia/if_xe.c $S/dev/pcmcia/if_cnw.c \
	$S/dev/pcmcia/if_wi_pcmcia.c $S/dev/pcmcia/if_malo.c \
	$S/dev/pcmcia/if_an_pcmcia.c $S/arch/amd64/pci/pcib.c \
	$S/dev/pci/amdpcib.c $S/dev/pci/tcpcib.c \
	$S/arch/amd64/pci/aapic.c $S/dev/ic/hme.c $S/dev/pci/if_hme_pci.c \
	$S/dev/isa/isa.c $S/dev/isa/isadma.c $S/dev/isa/com_isa.c \
	$S/dev/isa/pckbc_isa.c $S/dev/isa/vga_isa.c $S/dev/isa/wdc_isa.c \
	$S/dev/isa/mpu401.c $S/dev/isa/mpu_isa.c $S/dev/isa/pcppi.c \
	$S/dev/isa/spkr.c $S/dev/isa/lpt_isa.c $S/dev/isa/wbsio.c \
	$S/dev/isa/sch311x.c $S/dev/isa/lm78_isa.c $S/dev/isa/it.c \
	$S/dev/isa/uguru.c $S/dev/isa/aps.c \
	$S/arch/amd64/isa/isa_machdep.c $S/dev/wscons/wsdisplay.c \
	$S/dev/wscons/wsdisplay_compat_usl.c $S/dev/wscons/wsevent.c \
	$S/dev/wscons/wskbd.c $S/dev/wscons/wskbdutil.c \
	$S/dev/wscons/wsmouse.c $S/dev/wscons/wsmux.c \
	$S/dev/wscons/wsemulconf.c $S/dev/wscons/wsemul_subr.c \
	$S/dev/wscons/wsemul_vt100.c $S/dev/wscons/wsemul_vt100_subr.c \
	$S/dev/wscons/wsemul_vt100_chars.c \
	$S/dev/wscons/wsemul_vt100_keys.c $S/dev/pckbc/pckbd.c \
	$S/dev/pckbc/wskbdmap_mfii.c $S/dev/pckbc/pms.c \
	$S/arch/amd64/amd64/wscons_machdep.c $S/dev/isa/fdc.c \
	$S/dev/isa/fd.c $S/arch/amd64/amd64/pctr.c \
	$S/arch/amd64/amd64/nvram.c $S/dev/usb/usb.c $S/dev/usb/usbdi.c \
	$S/dev/usb/usbdi_util.c $S/dev/usb/usb_mem.c \
	$S/dev/usb/usb_subr.c $S/dev/usb/usb_quirks.c $S/dev/usb/uhub.c \
	$S/dev/usb/uaudio.c $S/dev/usb/uvideo.c $S/dev/usb/udl.c \
	$S/dev/usb/umidi.c $S/dev/usb/umidi_quirks.c $S/dev/usb/ucom.c \
	$S/dev/usb/ugen.c $S/dev/usb/hid.c $S/dev/usb/uhidev.c \
	$S/dev/usb/uhid.c $S/dev/usb/hidkbd.c $S/dev/usb/ukbdmap.c \
	$S/dev/usb/ukbd.c $S/dev/usb/hidms.c $S/dev/usb/ums.c \
	$S/dev/usb/uts.c $S/dev/usb/ubcmtp.c $S/dev/usb/ucycom.c \
	$S/dev/usb/ulpt.c $S/dev/usb/umass.c $S/dev/usb/umass_quirks.c \
	$S/dev/usb/umass_scsi.c $S/dev/usb/uthum.c $S/dev/usb/ugold.c \
	$S/dev/usb/utrh.c $S/dev/usb/uoak_subr.c $S/dev/usb/uoakrh.c \
	$S/dev/usb/uoaklux.c $S/dev/usb/uoakv.c $S/dev/usb/udcf.c \
	$S/dev/usb/urio.c $S/dev/usb/uvisor.c $S/dev/usb/udsbr.c \
	$S/dev/usb/utwitch.c $S/dev/usb/if_aue.c $S/dev/usb/if_axe.c \
	$S/dev/usb/if_axen.c $S/dev/usb/if_smsc.c $S/dev/usb/if_cue.c \
	$S/dev/usb/if_kue.c $S/dev/usb/if_cdce.c $S/dev/usb/if_urndis.c \
	$S/dev/usb/if_mos.c $S/dev/usb/if_udav.c $S/dev/usb/if_upl.c \
	$S/dev/usb/if_ugl.c $S/dev/usb/if_url.c $S/dev/usb/umodem.c \
	$S/dev/usb/uftdi.c $S/dev/usb/uplcom.c $S/dev/usb/umct.c \
	$S/dev/usb/uvscom.c $S/dev/usb/ubsa.c $S/dev/usb/uslcom.c \
	$S/dev/usb/uark.c $S/dev/usb/moscom.c $S/dev/usb/uipaq.c \
	$S/dev/usb/umsm.c $S/dev/usb/uchcom.c $S/dev/usb/uticom.c \
	$S/dev/usb/if_wi_usb.c $S/dev/usb/if_atu.c $S/dev/usb/if_ral.c \
	$S/dev/usb/if_rum.c $S/dev/usb/if_run.c $S/dev/usb/if_zyd.c \
	$S/dev/usb/if_upgt.c $S/dev/usb/if_urtw.c $S/dev/usb/if_urtwn.c \
	$S/dev/usb/if_rsu.c $S/dev/usb/if_otus.c $S/dev/usb/if_uath.c \
	$S/dev/usb/if_athn_usb.c $S/dev/usb/uow.c $S/dev/usb/uberry.c \
	$S/dev/usb/upd.c $S/dev/i2c/i2c.c $S/dev/i2c/i2c_exec.c \
	$S/dev/i2c/i2c_scan.c $S/dev/i2c/i2c_bitbang.c $S/dev/i2c/lm75.c \
	$S/dev/i2c/lm93.c $S/dev/i2c/lm87.c $S/dev/i2c/maxim6690.c \
	$S/dev/i2c/ad741x.c $S/dev/i2c/adm1021.c $S/dev/i2c/adm1024.c \
	$S/dev/i2c/adm1025.c $S/dev/i2c/adm1030.c $S/dev/i2c/adm1031.c \
	$S/dev/i2c/ds1631.c $S/dev/i2c/adt7460.c $S/dev/i2c/lm78_i2c.c \
	$S/dev/i2c/adm1026.c $S/dev/i2c/w83793g.c $S/dev/i2c/w83795g.c \
	$S/dev/i2c/asc7621.c $S/dev/i2c/asc7611.c $S/dev/i2c/spdmem_i2c.c \
	$S/dev/i2c/sdtemp.c $S/dev/i2c/lis331dl.c $S/dev/acpi/acpi.c \
	$S/dev/acpi/acpiutil.c $S/dev/acpi/dsdt.c $S/dev/acpi/acpidebug.c \
	$S/dev/acpi/acpitimer.c $S/dev/acpi/acpiac.c \
	$S/dev/acpi/acpibat.c $S/dev/acpi/acpibtn.c $S/dev/acpi/acpicpu.c \
	$S/dev/acpi/acpihpet.c $S/dev/acpi/acpiec.c $S/dev/acpi/acpitz.c \
	$S/dev/acpi/acpimadt.c $S/dev/acpi/acpimcfg.c \
	$S/dev/acpi/acpiprt.c $S/dev/acpi/acpidock.c \
	$S/dev/acpi/acpiasus.c $S/dev/acpi/acpithinkpad.c \
	$S/dev/acpi/acpitoshiba.c $S/dev/acpi/acpisony.c \
	$S/dev/acpi/acpivideo.c $S/dev/acpi/acpivout.c \
	$S/dev/acpi/acpipwrres.c $S/dev/acpi/atk0110.c \
	$S/arch/amd64/amd64/acpi_machdep.c $S/dev/sdmmc/sdmmc.c \
	$S/dev/sdmmc/sdmmc_cis.c $S/dev/sdmmc/sdmmc_io.c \
	$S/dev/sdmmc/sdmmc_mem.c $S/dev/sdmmc/sdmmc_scsi.c \
	$S/dev/onewire/onewire.c $S/dev/onewire/onewire_subr.c \
	$S/dev/onewire/owid.c $S/dev/onewire/owsbm.c \
	$S/dev/onewire/owtemp.c $S/dev/onewire/owctr.c

SFILES=	$S/lib/libkern/arch/amd64/strchr.S \
	$S/lib/libkern/arch/amd64/strrchr.S \
	$S/lib/libkern/arch/amd64/memchr.S \
	$S/lib/libkern/arch/amd64/memcmp.S \
	$S/lib/libkern/arch/amd64/bcmp.S \
	$S/lib/libkern/arch/amd64/bzero.S \
	$S/lib/libkern/arch/amd64/bcopy.S \
	$S/lib/libkern/arch/amd64/memcpy.S \
	$S/lib/libkern/arch/amd64/memmove.S \
	$S/lib/libkern/arch/amd64/ffs.S \
	$S/lib/libkern/arch/amd64/memset.S \
	$S/lib/libkern/arch/amd64/strcmp.S \
	$S/lib/libkern/arch/amd64/scanc.S \
	$S/lib/libkern/arch/amd64/skpc.S \
	$S/lib/libkern/arch/amd64/htonl.S \
	$S/lib/libkern/arch/amd64/htons.S $S/arch/amd64/amd64/aes_intel.S \
	$S/arch/amd64/amd64/mutex.S $S/arch/amd64/amd64/vector.S \
	$S/arch/amd64/amd64/copy.S $S/arch/amd64/amd64/spl.S \
	$S/arch/amd64/amd64/acpi_wakecode.S

# load lines for config "xxx" will be emitted as:
# xxx: ${SYSTEM_DEP} swapxxx.o
#	${SYSTEM_LD_HEAD}
#	${SYSTEM_LD} swapxxx.o
#	${SYSTEM_LD_TAIL}
SYSTEM_HEAD=	locore.o param.o ioconf.o
SYSTEM_OBJ=	${SYSTEM_HEAD} ${OBJS}
SYSTEM_DEP=	Makefile ${SYSTEM_OBJ}
SYSTEM_LD_HEAD=	@rm -f $@
SYSTEM_LD=	@echo ${LD} ${LINKFLAGS} -o $@ '$${SYSTEM_HEAD} vers.o $${OBJS}'; \
		${LD} ${LINKFLAGS} -o $@ ${SYSTEM_HEAD} vers.o ${OBJS}
SYSTEM_LD_TAIL=	@${SIZE} $@; chmod 755 $@

DEBUG?=
.if ${DEBUG} == "-g"
LINKFLAGS+=	-X
STRIPFLAGS=	-g -x
SYSTEM_LD_TAIL+=; \
		echo mv $@ $@.gdb; rm -f $@.gdb; mv $@ $@.gdb; \
		echo ${STRIP} ${STRIPFLAGS} -o $@ $@.gdb; \
		${STRIP} ${STRIPFLAGS} -o $@ $@.gdb
.else
LINKFLAGS+=	-S -x
.endif

all: bsd

bsd: ${SYSTEM_DEP} swapgeneric.o vers.o
	${SYSTEM_LD_HEAD}
	${SYSTEM_LD} swapgeneric.o
	${SYSTEM_LD_TAIL}

swapgeneric.o: $S/conf/swapgeneric.c
	${NORMAL_C}


# cc's -MD puts the source and output paths in the dependency file;
# since those are temp files here we need to fix it up.  It also
# puts the file in /tmp, so we use -MF to put it in the current
# directory as assym.P and then generate assym.d from it with a
# good target name
assym.h: $S/kern/genassym.sh Makefile \
	 ${_archdir}/${_arch}/genassym.cf ${_machdir}/${_mach}/genassym.cf
	cat ${_archdir}/${_arch}/genassym.cf ${_machdir}/${_mach}/genassym.cf | \
	    sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} -MF assym.P > assym.h.tmp
	sed '1s/.*/assym.h: \\/' assym.P > assym.d
	sort -u assym.h.tmp > assym.h

param.c: $S/conf/param.c
	rm -f param.c
	cp $S/conf/param.c .

param.o: param.c Makefile
	${NORMAL_C}

mcount.o: $S/lib/libkern/mcount.c Makefile
	${NORMAL_C_NOP}

ioconf.o: ioconf.c
	${NORMAL_C}

vers.o: ${SYSTEM_DEP} ${SYSTEM_SWAP_DEP}
	sh $S/conf/newvers.sh
	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c vers.c

clean::
	rm -f eddep *bsd *bsd.gdb tags *.[dio] [a-z]*.s \
	    [Ee]rrs linterrs assym.h ${DB_STRUCTINFO}

lint:
	@lint -hbxncez -Dvolatile= ${CPPFLAGS} -UKGDB \
	    ${CFILES} ioconf.c param.c | \
	    grep -v 'static function .* unused'

depend:
	@touch $@

tags:
	@echo "see $S/kern/Makefile for tags"

db_structinfo.h: $S/ddb/db_structinfo.c $S/ddb/parse_structinfo.pl
	${CC} ${CFLAGS} ${CPPFLAGS} -MT $@ -gstabs -c $S/ddb/db_structinfo.c
	objdump -g db_structinfo.o | perl $S/ddb/parse_structinfo.pl > $@
	rm -f db_structinfo.o

locore.o: ${_machdir}/${_mach}/locore.S assym.h
mutex.o vector.o copy.o spl.o mptramp.o acpi_wakecode.o: assym.h

# The install target can be redefined by putting a
# install-kernel-${MACHINE_NAME} target into /etc/mk.conf
MACHINE_NAME!=  uname -n
install: install-kernel-${MACHINE_NAME}
.if !target(install-kernel-${MACHINE_NAME}})
install-kernel-${MACHINE_NAME}:
	rm -f /obsd
	ln /bsd /obsd
	cp bsd /nbsd
	mv /nbsd /bsd
.endif

# pull in the dependency information
.if !empty(DB_STRUCTINFO) && !exists(${DB_STRUCTINFO})
 ${SYSTEM_OBJ}: ${DB_STRUCTINFO}
.endif
.ifnmake clean
. for o in ${SYSTEM_OBJ} assym.h ${DB_STRUCTINFO}
.  if exists(${o:R}.d)
.   include "${o:R}.d"
.  elif exists($o)
    .PHONY: $o
.  endif
. endfor
.endif

.SUFFIXES:
.SUFFIXES: .s .S .c .o

.PHONY: depend all install clean tags

.c.o:
	${NORMAL_C}

.s.o:
	${NORMAL_S}

.S.o:
	${NORMAL_S}

smc93cx6.o: $S/dev/ic/smc93cx6.c
pcdisplay_subr.o: $S/dev/ic/pcdisplay_subr.c
pcdisplay_chars.o: $S/dev/ic/pcdisplay_chars.c
vga.o: $S/dev/ic/vga.c
vga_subr.o: $S/dev/ic/vga_subr.c
edid.o: $S/dev/videomode/edid.c
vesagtf.o: $S/dev/videomode/vesagtf.c
videomode.o: $S/dev/videomode/videomode.c
mii_bitbang.o: $S/dev/mii/mii_bitbang.c
wdc.o: $S/dev/ic/wdc.c
aic7xxx.o: $S/dev/ic/aic7xxx.c
aic7xxx_openbsd.o: $S/dev/ic/aic7xxx_openbsd.c
aic7xxx_seeprom.o: $S/dev/ic/aic7xxx_seeprom.c
aic79xx.o: $S/dev/ic/aic79xx.c
aic79xx_openbsd.o: $S/dev/ic/aic79xx_openbsd.c
aic6360.o: $S/dev/ic/aic6360.c
adw.o: $S/dev/ic/adw.c
gdt_common.o: $S/dev/ic/gdt_common.c
twe.o: $S/dev/ic/twe.c
ciss.o: $S/dev/ic/ciss.c
ami.o: $S/dev/ic/ami.c
mfi.o: $S/dev/ic/mfi.c
qlw.o: $S/dev/ic/qlw.c
qla.o: $S/dev/ic/qla.c
isp_fw2100.o: $S/dev/microcode/isp/isp_fw2100.c
isp_fw2200.o: $S/dev/microcode/isp/isp_fw2200.c
isp_fw2300.o: $S/dev/microcode/isp/isp_fw2300.c
ahci.o: $S/dev/ic/ahci.c
mpi.o: $S/dev/ic/mpi.c
sili.o: $S/dev/ic/sili.c
ncr53c9x.o: $S/dev/ic/ncr53c9x.c
siop_common.o: $S/dev/ic/siop_common.c
siop.o: $S/dev/ic/siop.c
elink3.o: $S/dev/ic/elink3.c
if_wi.o: $S/dev/ic/if_wi.c
if_wi_hostap.o: $S/dev/ic/if_wi_hostap.c
an.o: $S/dev/ic/an.c
xl.o: $S/dev/ic/xl.c
fxp.o: $S/dev/ic/fxp.c
rtl81x9.o: $S/dev/ic/rtl81x9.c
re.o: $S/dev/ic/re.c
dc.o: $S/dev/ic/dc.c
smc91cxx.o: $S/dev/ic/smc91cxx.c
smc83c170.o: $S/dev/ic/smc83c170.c
ne2000.o: $S/dev/ic/ne2000.c
dl10019.o: $S/dev/ic/dl10019.c
ax88190.o: $S/dev/ic/ax88190.c
gem.o: $S/dev/ic/gem.c
ti.o: $S/dev/ic/ti.c
com.o: $S/dev/ic/com.c
pckbc.o: $S/dev/ic/pckbc.c
ac97.o: $S/dev/ic/ac97.c
lpt.o: $S/dev/ic/lpt.c
iha.o: $S/dev/ic/iha.c
lm78.o: $S/dev/ic/lm78.c
ar5xxx.o: $S/dev/ic/ar5xxx.c
ar5210.o: $S/dev/ic/ar5210.c
ar5211.o: $S/dev/ic/ar5211.c
ar5212.o: $S/dev/ic/ar5212.c
ath.o: $S/dev/ic/ath.c
athn.o: $S/dev/ic/athn.c
ar5008.o: $S/dev/ic/ar5008.c
ar5416.o: $S/dev/ic/ar5416.c
ar9280.o: $S/dev/ic/ar9280.c
ar9285.o: $S/dev/ic/ar9285.c
ar9287.o: $S/dev/ic/ar9287.c
ar9003.o: $S/dev/ic/ar9003.c
ar9380.o: $S/dev/ic/ar9380.c
atw.o: $S/dev/ic/atw.c
rtw.o: $S/dev/ic/rtw.c
rt2560.o: $S/dev/ic/rt2560.c
rt2661.o: $S/dev/ic/rt2661.c
rt2860.o: $S/dev/ic/rt2860.c
acx.o: $S/dev/ic/acx.c
acx111.o: $S/dev/ic/acx111.c
acx100.o: $S/dev/ic/acx100.c
pgt.o: $S/dev/ic/pgt.c
aic6915.o: $S/dev/ic/aic6915.c
malo.o: $S/dev/ic/malo.c
bwi.o: $S/dev/ic/bwi.c
uhci.o: $S/dev/usb/uhci.c
ohci.o: $S/dev/usb/ohci.c
ehci.o: $S/dev/usb/ehci.c
sdhc.o: $S/dev/sdmmc/sdhc.c
rtsx.o: $S/dev/ic/rtsx.c
radio.o: $S/dev/radio.c
ipmi.o: $S/dev/ipmi.c
vmt.o: $S/dev/vmt.c
vscsi.o: $S/dev/vscsi.c
mpath.o: $S/scsi/mpath.c
softraid.o: $S/dev/softraid.c
softraid_concat.o: $S/dev/softraid_concat.c
softraid_crypto.o: $S/dev/softraid_crypto.c
softraid_raid0.o: $S/dev/softraid_raid0.c
softraid_raid1.o: $S/dev/softraid_raid1.c
softraid_raid5.o: $S/dev/softraid_raid5.c
softraid_raid6.o: $S/dev/softraid_raid6.c
spdmem.o: $S/dev/spdmem.c
ksyms.o: $S/dev/ksyms.c
fuse_device.o: $S/miscfs/fuse/fuse_device.c
fuse_file.o: $S/miscfs/fuse/fuse_file.c
fuse_lookup.o: $S/miscfs/fuse/fuse_lookup.c
fuse_vfsops.o: $S/miscfs/fuse/fuse_vfsops.c
fuse_vnops.o: $S/miscfs/fuse/fuse_vnops.c
fusebuf.o: $S/miscfs/fuse/fusebuf.c
pf.o: $S/net/pf.c
pf_norm.o: $S/net/pf_norm.c
pf_ruleset.o: $S/net/pf_ruleset.c
pf_ioctl.o: $S/net/pf_ioctl.c
pf_table.o: $S/net/pf_table.c
pf_osfp.o: $S/net/pf_osfp.c
pf_if.o: $S/net/pf_if.c
pf_lb.o: $S/net/pf_lb.c
hfsc.o: $S/net/hfsc.c
if_pflog.o: $S/net/if_pflog.c
if_pfsync.o: $S/net/if_pfsync.c
if_pflow.o: $S/net/if_pflow.c
bio.o: $S/dev/bio.c
hotplug.o: $S/dev/hotplug.c
if_pppoe.o: $S/net/if_pppoe.c
db_access.o: $S/ddb/db_access.c
db_break.o: $S/ddb/db_break.c
db_command.o: $S/ddb/db_command.c
db_elf.o: $S/ddb/db_elf.c
db_examine.o: $S/ddb/db_examine.c
db_expr.o: $S/ddb/db_expr.c
db_hangman.o: $S/ddb/db_hangman.c
db_input.o: $S/ddb/db_input.c
db_lex.o: $S/ddb/db_lex.c
db_output.o: $S/ddb/db_output.c
db_print.o: $S/ddb/db_print.c
db_run.o: $S/ddb/db_run.c
db_sym.o: $S/ddb/db_sym.c
db_trap.o: $S/ddb/db_trap.c
db_variables.o: $S/ddb/db_variables.c
db_watch.o: $S/ddb/db_watch.c
db_write_cmd.o: $S/ddb/db_write_cmd.c
db_usrreq.o: $S/ddb/db_usrreq.c
auconv.o: $S/dev/auconv.c
audio.o: $S/dev/audio.c
cons.o: $S/dev/cons.c
diskmap.o: $S/dev/diskmap.c
firmload.o: $S/dev/firmload.c
dp8390.o: $S/dev/ic/dp8390.c
rtl80x9.o: $S/dev/ic/rtl80x9.c
midi.o: $S/dev/midi.c
mulaw.o: $S/dev/mulaw.c
systrace.o: $S/dev/systrace.c
vnd.o: $S/dev/vnd.c
rnd.o: $S/dev/rnd.c
video.o: $S/dev/video.c
cd9660_bmap.o: $S/isofs/cd9660/cd9660_bmap.c
cd9660_lookup.o: $S/isofs/cd9660/cd9660_lookup.c
cd9660_node.o: $S/isofs/cd9660/cd9660_node.c
cd9660_rrip.o: $S/isofs/cd9660/cd9660_rrip.c
cd9660_util.o: $S/isofs/cd9660/cd9660_util.c
cd9660_vfsops.o: $S/isofs/cd9660/cd9660_vfsops.c
cd9660_vnops.o: $S/isofs/cd9660/cd9660_vnops.c
udf_subr.o: $S/isofs/udf/udf_subr.c
udf_vfsops.o: $S/isofs/udf/udf_vfsops.c
udf_vnops.o: $S/isofs/udf/udf_vnops.c
clock_subr.o: $S/kern/clock_subr.c
exec_conf.o: $S/kern/exec_conf.c
exec_elf32.o: $S/kern/exec_elf32.c
exec_elf64.o: $S/kern/exec_elf64.c
exec_script.o: $S/kern/exec_script.c
exec_subr.o: $S/kern/exec_subr.c
init_main.o: $S/kern/init_main.c
init_sysent.o: $S/kern/init_sysent.c
kern_acct.o: $S/kern/kern_acct.c
kern_bufq.o: $S/kern/kern_bufq.c
kern_clock.o: $S/kern/kern_clock.c
kern_descrip.o: $S/kern/kern_descrip.c
kern_event.o: $S/kern/kern_event.c
kern_exec.o: $S/kern/kern_exec.c
kern_exit.o: $S/kern/kern_exit.c
kern_fork.o: $S/kern/kern_fork.c
kern_kthread.o: $S/kern/kern_kthread.c
kern_ktrace.o: $S/kern/kern_ktrace.c
kern_lock.o: $S/kern/kern_lock.c
kern_lkm.o: $S/kern/kern_lkm.c
kern_malloc.o: $S/kern/kern_malloc.c
kern_rwlock.o: $S/kern/kern_rwlock.c
kern_physio.o: $S/kern/kern_physio.c
kern_proc.o: $S/kern/kern_proc.c
kern_prot.o: $S/kern/kern_prot.c
kern_resource.o: $S/kern/kern_resource.c
kern_sched.o: $S/kern/kern_sched.c
kern_sensors.o: $S/kern/kern_sensors.c
kern_sig.o: $S/kern/kern_sig.c
kern_subr.o: $S/kern/kern_subr.c
kern_sysctl.o: $S/kern/kern_sysctl.c
kern_synch.o: $S/kern/kern_synch.c
kern_tc.o: $S/kern/kern_tc.c
kern_time.o: $S/kern/kern_time.c
kern_timeout.o: $S/kern/kern_timeout.c
kern_watchdog.o: $S/kern/kern_watchdog.c
kern_workq.o: $S/kern/kern_workq.c
kern_task.o: $S/kern/kern_task.c
kern_xxx.o: $S/kern/kern_xxx.c
kern_uuid.o: $S/kern/kern_uuid.c
kern_objcache.o: $S/kern/kern_objcache.c
sched_bsd.o: $S/kern/sched_bsd.c
subr_autoconf.o: $S/kern/subr_autoconf.c
subr_disk.o: $S/kern/subr_disk.c
subr_evcount.o: $S/kern/subr_evcount.c
subr_extent.o: $S/kern/subr_extent.c
subr_hibernate.o: $S/kern/subr_hibernate.c
subr_log.o: $S/kern/subr_log.c
subr_poison.o: $S/kern/subr_poison.c
subr_pool.o: $S/kern/subr_pool.c
dma_alloc.o: $S/kern/dma_alloc.c
subr_prf.o: $S/kern/subr_prf.c
subr_prof.o: $S/kern/subr_prof.c
subr_userconf.o: $S/kern/subr_userconf.c
subr_xxx.o: $S/kern/subr_xxx.c
sys_generic.o: $S/kern/sys_generic.c
sys_pipe.o: $S/kern/sys_pipe.c
sys_process.o: $S/kern/sys_process.c
sys_socket.o: $S/kern/sys_socket.c
sysv_ipc.o: $S/kern/sysv_ipc.c
sysv_msg.o: $S/kern/sysv_msg.c
sysv_sem.o: $S/kern/sysv_sem.c
sysv_shm.o: $S/kern/sysv_shm.c
tty.o: $S/kern/tty.c
tty_conf.o: $S/kern/tty_conf.c
tty_pty.o: $S/kern/tty_pty.c
tty_nmea.o: $S/kern/tty_nmea.c
tty_msts.o: $S/kern/tty_msts.c
tty_endrun.o: $S/kern/tty_endrun.c
tty_subr.o: $S/kern/tty_subr.c
tty_tty.o: $S/kern/tty_tty.c
uipc_domain.o: $S/kern/uipc_domain.c
uipc_mbuf.o: $S/kern/uipc_mbuf.c
uipc_mbuf2.o: $S/kern/uipc_mbuf2.c
uipc_proto.o: $S/kern/uipc_proto.c
uipc_socket.o: $S/kern/uipc_socket.c
uipc_socket2.o: $S/kern/uipc_socket2.c
uipc_syscalls.o: $S/kern/uipc_syscalls.c
uipc_usrreq.o: $S/kern/uipc_usrreq.c
vfs_bio.o: $S/kern/vfs_bio.c
vfs_biomem.o: $S/kern/vfs_biomem.c
vfs_cache.o: $S/kern/vfs_cache.c
vfs_cluster.o: $S/kern/vfs_cluster.c
vfs_default.o: $S/kern/vfs_default.c
vfs_init.o: $S/kern/vfs_init.c
vfs_lockf.o: $S/kern/vfs_lockf.c
vfs_lookup.o: $S/kern/vfs_lookup.c
vfs_subr.o: $S/kern/vfs_subr.c
vfs_sync.o: $S/kern/vfs_sync.c
vfs_syscalls.o: $S/kern/vfs_syscalls.c
vfs_vops.o: $S/kern/vfs_vops.c
vfs_vnops.o: $S/kern/vfs_vnops.c
vfs_getcwd.o: $S/kern/vfs_getcwd.c
spec_vnops.o: $S/kern/spec_vnops.c
dead_vnops.o: $S/miscfs/deadfs/dead_vnops.c
fifo_vnops.o: $S/miscfs/fifofs/fifo_vnops.c
msdosfs_conv.o: $S/msdosfs/msdosfs_conv.c
msdosfs_denode.o: $S/msdosfs/msdosfs_denode.c
msdosfs_fat.o: $S/msdosfs/msdosfs_fat.c
msdosfs_lookup.o: $S/msdosfs/msdosfs_lookup.c
msdosfs_vfsops.o: $S/msdosfs/msdosfs_vfsops.c
msdosfs_vnops.o: $S/msdosfs/msdosfs_vnops.c
hammer2_ccms.o: $S/hammer2/hammer2_ccms.c
hammer2_chain.o: $S/hammer2/hammer2_chain.c
hammer2_cluster.o: $S/hammer2/hammer2_cluster.c
hammer2_flush.o: $S/hammer2/hammer2_flush.c
hammer2_freemap.o: $S/hammer2/hammer2_freemap.c
hammer2_inode.o: $S/hammer2/hammer2_inode.c
hammer2_io.o: $S/hammer2/hammer2_io.c
hammer2_ioctl.o: $S/hammer2/hammer2_ioctl.c
hammer2_lz4.o: $S/hammer2/hammer2_lz4.c
hammer2_msgops.o: $S/hammer2/hammer2_msgops.c
hammer2_subr.o: $S/hammer2/hammer2_subr.c
hammer2_vfsops.o: $S/hammer2/hammer2_vfsops.c
hammer2_vnops.o: $S/hammer2/hammer2_vnops.c
ntfs_compr.o: $S/ntfs/ntfs_compr.c
ntfs_conv.o: $S/ntfs/ntfs_conv.c
ntfs_ihash.o: $S/ntfs/ntfs_ihash.c
ntfs_subr.o: $S/ntfs/ntfs_subr.c
ntfs_vfsops.o: $S/ntfs/ntfs_vfsops.c
ntfs_vnops.o: $S/ntfs/ntfs_vnops.c
tmpfs_mem.o: $S/tmpfs/tmpfs_mem.c
tmpfs_subr.o: $S/tmpfs/tmpfs_subr.c
tmpfs_vfsops.o: $S/tmpfs/tmpfs_vfsops.c
tmpfs_vnops.o: $S/tmpfs/tmpfs_vnops.c
tmpfs_specops.o: $S/tmpfs/tmpfs_specops.c
tmpfs_fifoops.o: $S/tmpfs/tmpfs_fifoops.c
bpf.o: $S/net/bpf.c
bpf_filter.o: $S/net/bpf_filter.c
if.o: $S/net/if.c
if_ethersubr.o: $S/net/if_ethersubr.c
if_spppsubr.o: $S/net/if_spppsubr.c
if_loop.o: $S/net/if_loop.c
if_media.o: $S/net/if_media.c
if_sl.o: $S/net/if_sl.c
if_ppp.o: $S/net/if_ppp.c
netisr.o: $S/net/netisr.c
ppp_tty.o: $S/net/ppp_tty.c
bsd-comp.o: $S/net/bsd-comp.c
ppp-deflate.o: $S/net/ppp-deflate.c
if_tun.o: $S/net/if_tun.c
if_bridge.o: $S/net/if_bridge.c
bridgestp.o: $S/net/bridgestp.c
if_vlan.o: $S/net/if_vlan.c
pipex.o: $S/net/pipex.c
radix.o: $S/net/radix.c
radix_mpath.o: $S/net/radix_mpath.c
raw_cb.o: $S/net/raw_cb.c
raw_usrreq.o: $S/net/raw_usrreq.c
route.o: $S/net/route.c
rtsock.o: $S/net/rtsock.c
slcompress.o: $S/net/slcompress.c
if_enc.o: $S/net/if_enc.c
if_gre.o: $S/net/if_gre.c
if_trunk.o: $S/net/if_trunk.c
trunklacp.o: $S/net/trunklacp.c
if_mpe.o: $S/net/if_mpe.c
if_vether.o: $S/net/if_vether.c
if_pppx.o: $S/net/if_pppx.c
if_vxlan.o: $S/net/if_vxlan.c
ieee80211.o: $S/net80211/ieee80211.c
ieee80211_amrr.o: $S/net80211/ieee80211_amrr.c
ieee80211_crypto.o: $S/net80211/ieee80211_crypto.c
ieee80211_crypto_bip.o: $S/net80211/ieee80211_crypto_bip.c
ieee80211_crypto_ccmp.o: $S/net80211/ieee80211_crypto_ccmp.c
ieee80211_crypto_tkip.o: $S/net80211/ieee80211_crypto_tkip.c
ieee80211_crypto_wep.o: $S/net80211/ieee80211_crypto_wep.c
ieee80211_input.o: $S/net80211/ieee80211_input.c
ieee80211_ioctl.o: $S/net80211/ieee80211_ioctl.c
ieee80211_node.o: $S/net80211/ieee80211_node.c
ieee80211_output.o: $S/net80211/ieee80211_output.c
ieee80211_pae_input.o: $S/net80211/ieee80211_pae_input.c
ieee80211_pae_output.o: $S/net80211/ieee80211_pae_output.c
ieee80211_proto.o: $S/net80211/ieee80211_proto.c
ieee80211_rssadapt.o: $S/net80211/ieee80211_rssadapt.c
ieee80211_regdomain.o: $S/net80211/ieee80211_regdomain.c
if_ether.o: $S/netinet/if_ether.c
igmp.o: $S/netinet/igmp.c
in.o: $S/netinet/in.c
in_pcb.o: $S/netinet/in_pcb.c
in_proto.o: $S/netinet/in_proto.c
inet_nat64.o: $S/netinet/inet_nat64.c
inet_ntop.o: $S/netinet/inet_ntop.c
ip_divert.o: $S/netinet/ip_divert.c
ip_icmp.o: $S/netinet/ip_icmp.c
ip_id.o: $S/netinet/ip_id.c
ip_input.o: $S/netinet/ip_input.c
ip_mroute.o: $S/netinet/ip_mroute.c
ip_output.o: $S/netinet/ip_output.c
raw_ip.o: $S/netinet/raw_ip.c
tcp_debug.o: $S/netinet/tcp_debug.c
tcp_input.o: $S/netinet/tcp_input.c
tcp_output.o: $S/netinet/tcp_output.c
tcp_subr.o: $S/netinet/tcp_subr.c
tcp_timer.o: $S/netinet/tcp_timer.c
tcp_usrreq.o: $S/netinet/tcp_usrreq.c
udp_usrreq.o: $S/netinet/udp_usrreq.c
ip_gre.o: $S/netinet/ip_gre.c
ip_ipsp.o: $S/netinet/ip_ipsp.c
ip_spd.o: $S/netinet/ip_spd.c
ip_ipip.o: $S/netinet/ip_ipip.c
ip_ether.o: $S/netinet/ip_ether.c
ipsec_input.o: $S/netinet/ipsec_input.c
ipsec_output.o: $S/netinet/ipsec_output.c
ip_esp.o: $S/netinet/ip_esp.c
ip_ah.o: $S/netinet/ip_ah.c
ip_carp.o: $S/netinet/ip_carp.c
ip_ipcomp.o: $S/netinet/ip_ipcomp.c
rijndael.o: $S/crypto/rijndael.c
md5.o: $S/crypto/md5.c
rmd160.o: $S/crypto/rmd160.c
sha1.o: $S/crypto/sha1.c
sha2.o: $S/crypto/sha2.c
blf.o: $S/crypto/blf.c
cast.o: $S/crypto/cast.c
ecb_enc.o: $S/crypto/ecb_enc.c
set_key.o: $S/crypto/set_key.c
ecb3_enc.o: $S/crypto/ecb3_enc.c
crypto.o: $S/crypto/crypto.c
cryptodev.o: $S/crypto/cryptodev.c
criov.o: $S/crypto/criov.c
cryptosoft.o: $S/crypto/cryptosoft.c
xform.o: $S/crypto/xform.c
xform_ipcomp.o: $S/crypto/xform_ipcomp.c
arc4.o: $S/crypto/arc4.c
michael.o: $S/crypto/michael.c
cmac.o: $S/crypto/cmac.c
hmac.o: $S/crypto/hmac.c
gmac.o: $S/crypto/gmac.c
key_wrap.o: $S/crypto/key_wrap.c
idgen.o: $S/crypto/idgen.c
mpls_input.o: $S/netmpls/mpls_input.c
mpls_output.o: $S/netmpls/mpls_output.c
mpls_proto.o: $S/netmpls/mpls_proto.c
mpls_raw.o: $S/netmpls/mpls_raw.c
mpls_shim.o: $S/netmpls/mpls_shim.c
krpc_subr.o: $S/nfs/krpc_subr.c
nfs_bio.o: $S/nfs/nfs_bio.c
nfs_boot.o: $S/nfs/nfs_boot.c
nfs_debug.o: $S/nfs/nfs_debug.c
nfs_node.o: $S/nfs/nfs_node.c
nfs_kq.o: $S/nfs/nfs_kq.c
nfs_serv.o: $S/nfs/nfs_serv.c
nfs_socket.o: $S/nfs/nfs_socket.c
nfs_srvcache.o: $S/nfs/nfs_srvcache.c
nfs_subs.o: $S/nfs/nfs_subs.c
nfs_syscalls.o: $S/nfs/nfs_syscalls.c
nfs_vfsops.o: $S/nfs/nfs_vfsops.c
nfs_vnops.o: $S/nfs/nfs_vnops.c
ffs_alloc.o: $S/ufs/ffs/ffs_alloc.c
ffs_balloc.o: $S/ufs/ffs/ffs_balloc.c
ffs_inode.o: $S/ufs/ffs/ffs_inode.c
ffs_subr.o: $S/ufs/ffs/ffs_subr.c
ffs_softdep_stub.o: $S/ufs/ffs/ffs_softdep_stub.c
ffs_tables.o: $S/ufs/ffs/ffs_tables.c
ffs_vfsops.o: $S/ufs/ffs/ffs_vfsops.c
ffs_vnops.o: $S/ufs/ffs/ffs_vnops.c
ffs_softdep.o: $S/ufs/ffs/ffs_softdep.c
mfs_vfsops.o: $S/ufs/mfs/mfs_vfsops.c
mfs_vnops.o: $S/ufs/mfs/mfs_vnops.c
ufs_bmap.o: $S/ufs/ufs/ufs_bmap.c
ufs_dirhash.o: $S/ufs/ufs/ufs_dirhash.c
ufs_ihash.o: $S/ufs/ufs/ufs_ihash.c
ufs_inode.o: $S/ufs/ufs/ufs_inode.c
ufs_lookup.o: $S/ufs/ufs/ufs_lookup.c
ufs_quota.o: $S/ufs/ufs/ufs_quota.c
ufs_quota_stub.o: $S/ufs/ufs/ufs_quota_stub.c
ufs_vfsops.o: $S/ufs/ufs/ufs_vfsops.c
ufs_vnops.o: $S/ufs/ufs/ufs_vnops.c
ext2fs_alloc.o: $S/ufs/ext2fs/ext2fs_alloc.c
ext2fs_balloc.o: $S/ufs/ext2fs/ext2fs_balloc.c
ext2fs_bmap.o: $S/ufs/ext2fs/ext2fs_bmap.c
ext2fs_bswap.o: $S/ufs/ext2fs/ext2fs_bswap.c
ext2fs_inode.o: $S/ufs/ext2fs/ext2fs_inode.c
ext2fs_lookup.o: $S/ufs/ext2fs/ext2fs_lookup.c
ext2fs_readwrite.o: $S/ufs/ext2fs/ext2fs_readwrite.c
ext2fs_subr.o: $S/ufs/ext2fs/ext2fs_subr.c
ext2fs_vfsops.o: $S/ufs/ext2fs/ext2fs_vfsops.c
ext2fs_vnops.o: $S/ufs/ext2fs/ext2fs_vnops.c
uvm_addr.o: $S/uvm/uvm_addr.c
uvm_amap.o: $S/uvm/uvm_amap.c
uvm_anon.o: $S/uvm/uvm_anon.c
uvm_aobj.o: $S/uvm/uvm_aobj.c
uvm_device.o: $S/uvm/uvm_device.c
uvm_fault.o: $S/uvm/uvm_fault.c
uvm_glue.o: $S/uvm/uvm_glue.c
uvm_init.o: $S/uvm/uvm_init.c
uvm_io.o: $S/uvm/uvm_io.c
uvm_km.o: $S/uvm/uvm_km.c
uvm_map.o: $S/uvm/uvm_map.c
uvm_meter.o: $S/uvm/uvm_meter.c
uvm_mmap.o: $S/uvm/uvm_mmap.c
uvm_object.o: $S/uvm/uvm_object.c
uvm_page.o: $S/uvm/uvm_page.c
uvm_pager.o: $S/uvm/uvm_pager.c
uvm_pdaemon.o: $S/uvm/uvm_pdaemon.c
uvm_pmemrange.o: $S/uvm/uvm_pmemrange.c
uvm_stat.o: $S/uvm/uvm_stat.c
uvm_swap.o: $S/uvm/uvm_swap.c
uvm_swap_encrypt.o: $S/uvm/uvm_swap_encrypt.c
uvm_unix.o: $S/uvm/uvm_unix.c
uvm_user.o: $S/uvm/uvm_user.c
uvm_vnode.o: $S/uvm/uvm_vnode.c
if_gif.o: $S/net/if_gif.c
ip_ecn.o: $S/netinet/ip_ecn.c
in_gif.o: $S/netinet/in_gif.c
in6_gif.o: $S/netinet6/in6_gif.c
in6_pcb.o: $S/netinet6/in6_pcb.c
in6.o: $S/netinet6/in6.c
ip6_divert.o: $S/netinet6/ip6_divert.c
in6_ifattach.o: $S/netinet6/in6_ifattach.c
in6_cksum.o: $S/netinet6/in6_cksum.c
in6_src.o: $S/netinet6/in6_src.c
in6_proto.o: $S/netinet6/in6_proto.c
dest6.o: $S/netinet6/dest6.c
frag6.o: $S/netinet6/frag6.c
icmp6.o: $S/netinet6/icmp6.c
ip6_id.o: $S/netinet6/ip6_id.c
ip6_input.o: $S/netinet6/ip6_input.c
ip6_forward.o: $S/netinet6/ip6_forward.c
ip6_mroute.o: $S/netinet6/ip6_mroute.c
ip6_output.o: $S/netinet6/ip6_output.c
route6.o: $S/netinet6/route6.c
mld6.o: $S/netinet6/mld6.c
nd6.o: $S/netinet6/nd6.c
nd6_nbr.o: $S/netinet6/nd6_nbr.c
nd6_rtr.o: $S/netinet6/nd6_rtr.c
raw_ip6.o: $S/netinet6/raw_ip6.c
udp6_output.o: $S/netinet6/udp6_output.c
pfkey.o: $S/net/pfkey.c
pfkeyv2.o: $S/net/pfkeyv2.c
pfkeyv2_parsemessage.o: $S/net/pfkeyv2_parsemessage.c
pfkeyv2_convert.o: $S/net/pfkeyv2_convert.c
compat_util.o: $S/compat/common/compat_util.c
x86emu.o: $S/dev/x86emu/x86emu.c
x86emu_util.o: $S/dev/x86emu/x86emu_util.c
getsn.o: $S/lib/libkern/getsn.c
random.o: $S/lib/libkern/random.c
explicit_bzero.o: $S/lib/libkern/explicit_bzero.c
timingsafe_bcmp.o: $S/lib/libkern/timingsafe_bcmp.c
strchr.o: $S/lib/libkern/arch/amd64/strchr.S
strrchr.o: $S/lib/libkern/arch/amd64/strrchr.S
__main.o: $S/lib/libkern/__main.c
imax.o: $S/lib/libkern/imax.c
imin.o: $S/lib/libkern/imin.c
lmax.o: $S/lib/libkern/lmax.c
lmin.o: $S/lib/libkern/lmin.c
max.o: $S/lib/libkern/max.c
min.o: $S/lib/libkern/min.c
ulmax.o: $S/lib/libkern/ulmax.c
ulmin.o: $S/lib/libkern/ulmin.c
memchr.o: $S/lib/libkern/arch/amd64/memchr.S
memcmp.o: $S/lib/libkern/arch/amd64/memcmp.S
bcmp.o: $S/lib/libkern/arch/amd64/bcmp.S
bzero.o: $S/lib/libkern/arch/amd64/bzero.S
bcopy.o: $S/lib/libkern/arch/amd64/bcopy.S
memcpy.o: $S/lib/libkern/arch/amd64/memcpy.S
memmove.o: $S/lib/libkern/arch/amd64/memmove.S
ffs.o: $S/lib/libkern/arch/amd64/ffs.S
fls.o: $S/lib/libkern/fls.c
flsl.o: $S/lib/libkern/flsl.c
memset.o: $S/lib/libkern/arch/amd64/memset.S
strcmp.o: $S/lib/libkern/arch/amd64/strcmp.S
strlcat.o: $S/lib/libkern/strlcat.c
strlcpy.o: $S/lib/libkern/strlcpy.c
strlen.o: $S/lib/libkern/strlen.c
strncmp.o: $S/lib/libkern/strncmp.c
strncpy.o: $S/lib/libkern/strncpy.c
strnlen.o: $S/lib/libkern/strnlen.c
scanc.o: $S/lib/libkern/arch/amd64/scanc.S
skpc.o: $S/lib/libkern/arch/amd64/skpc.S
htonl.o: $S/lib/libkern/arch/amd64/htonl.S
htons.o: $S/lib/libkern/arch/amd64/htons.S
strncasecmp.o: $S/lib/libkern/strncasecmp.c
adler32.o: $S/lib/libz/adler32.c
crc32.o: $S/lib/libz/crc32.c
infback.o: $S/lib/libz/infback.c
inffast.o: $S/lib/libz/inffast.c
inflate.o: $S/lib/libz/inflate.c
inftrees.o: $S/lib/libz/inftrees.c
deflate.o: $S/lib/libz/deflate.c
zutil.o: $S/lib/libz/zutil.c
zopenbsd.o: $S/lib/libz/zopenbsd.c
trees.o: $S/lib/libz/trees.c
compress.o: $S/lib/libz/compress.c
autoconf.o: $S/arch/amd64/amd64/autoconf.c
conf.o: $S/arch/amd64/amd64/conf.c
disksubr.o: $S/arch/amd64/amd64/disksubr.c
machdep.o: $S/arch/amd64/amd64/machdep.c
hibernate_machdep.o: $S/arch/amd64/amd64/hibernate_machdep.c
identcpu.o: $S/arch/amd64/amd64/identcpu.c
via.o: $S/arch/amd64/amd64/via.c
aes_intel.o: $S/arch/amd64/amd64/aes_intel.S
aesni.o: $S/arch/amd64/amd64/aesni.c
amd64errata.o: $S/arch/amd64/amd64/amd64errata.c
mem.o: $S/arch/amd64/amd64/mem.c
amd64_mem.o: $S/arch/amd64/amd64/amd64_mem.c
mtrr.o: $S/arch/amd64/amd64/mtrr.c
pmap.o: $S/arch/amd64/amd64/pmap.c
process_machdep.o: $S/arch/amd64/amd64/process_machdep.c
sys_machdep.o: $S/arch/amd64/amd64/sys_machdep.c
trap.o: $S/arch/amd64/amd64/trap.c
vm_machdep.o: $S/arch/amd64/amd64/vm_machdep.c
fpu.o: $S/arch/amd64/amd64/fpu.c
softintr.o: $S/arch/amd64/amd64/softintr.c
i8259.o: $S/arch/amd64/amd64/i8259.c
cacheinfo.o: $S/arch/amd64/amd64/cacheinfo.c
mutex.o: $S/arch/amd64/amd64/mutex.S
vector.o: $S/arch/amd64/amd64/vector.S
copy.o: $S/arch/amd64/amd64/copy.S
spl.o: $S/arch/amd64/amd64/spl.S
intr.o: $S/arch/amd64/amd64/intr.c
bus_space.o: $S/arch/amd64/amd64/bus_space.c
bus_dma.o: $S/arch/amd64/amd64/bus_dma.c
apic.o: $S/arch/amd64/amd64/apic.c
consinit.o: $S/arch/amd64/amd64/consinit.c
cninit.o: $S/dev/cninit.c
dkcsum.o: $S/arch/amd64/amd64/dkcsum.c
db_disasm.o: $S/arch/amd64/amd64/db_disasm.c
db_interface.o: $S/arch/amd64/amd64/db_interface.c
db_memrw.o: $S/arch/amd64/amd64/db_memrw.c
db_trace.o: $S/arch/amd64/amd64/db_trace.c
in_cksum.o: $S/netinet/in_cksum.c
in4_cksum.o: $S/netinet/in4_cksum.c
clock.o: $S/arch/amd64/isa/clock.c
powernow-k8.o: $S/arch/amd64/amd64/powernow-k8.c
est.o: $S/arch/amd64/amd64/est.c
k1x-pstate.o: $S/arch/amd64/amd64/k1x-pstate.c
rasops.o: $S/dev/rasops/rasops.c
rasops8.o: $S/dev/rasops/rasops8.c
rasops15.o: $S/dev/rasops/rasops15.c
rasops32.o: $S/dev/rasops/rasops32.c
wsfont.o: $S/dev/wsfont/wsfont.c
mii.o: $S/dev/mii/mii.c
mii_physubr.o: $S/dev/mii/mii_physubr.c
ukphy_subr.o: $S/dev/mii/ukphy_subr.c
nsphy.o: $S/dev/mii/nsphy.c
nsphyter.o: $S/dev/mii/nsphyter.c
gentbi.o: $S/dev/mii/gentbi.c
qsphy.o: $S/dev/mii/qsphy.c
inphy.o: $S/dev/mii/inphy.c
iophy.o: $S/dev/mii/iophy.c
eephy.o: $S/dev/mii/eephy.c
exphy.o: $S/dev/mii/exphy.c
rlphy.o: $S/dev/mii/rlphy.c
lxtphy.o: $S/dev/mii/lxtphy.c
luphy.o: $S/dev/mii/luphy.c
mtdphy.o: $S/dev/mii/mtdphy.c
icsphy.o: $S/dev/mii/icsphy.c
sqphy.o: $S/dev/mii/sqphy.c
tqphy.o: $S/dev/mii/tqphy.c
ukphy.o: $S/dev/mii/ukphy.c
dcphy.o: $S/dev/mii/dcphy.c
bmtphy.o: $S/dev/mii/bmtphy.c
brgphy.o: $S/dev/mii/brgphy.c
xmphy.o: $S/dev/mii/xmphy.c
amphy.o: $S/dev/mii/amphy.c
acphy.o: $S/dev/mii/acphy.c
nsgphy.o: $S/dev/mii/nsgphy.c
urlphy.o: $S/dev/mii/urlphy.c
rgephy.o: $S/dev/mii/rgephy.c
ciphy.o: $S/dev/mii/ciphy.c
ipgphy.o: $S/dev/mii/ipgphy.c
etphy.o: $S/dev/mii/etphy.c
jmphy.o: $S/dev/mii/jmphy.c
atphy.o: $S/dev/mii/atphy.c
scsi_base.o: $S/scsi/scsi_base.c
scsi_ioctl.o: $S/scsi/scsi_ioctl.c
scsiconf.o: $S/scsi/scsiconf.c
cd.o: $S/scsi/cd.c
ch.o: $S/scsi/ch.c
sd.o: $S/scsi/sd.c
st.o: $S/scsi/st.c
uk.o: $S/scsi/uk.c
safte.o: $S/scsi/safte.c
ses.o: $S/scsi/ses.c
mpath_sym.o: $S/scsi/mpath_sym.c
mpath_rdac.o: $S/scsi/mpath_rdac.c
mpath_emc.o: $S/scsi/mpath_emc.c
mpath_hds.o: $S/scsi/mpath_hds.c
atapiscsi.o: $S/dev/atapiscsi/atapiscsi.c
wd.o: $S/dev/ata/wd.c
ata_wdc.o: $S/dev/ata/ata_wdc.c
ata.o: $S/dev/ata/ata.c
atascsi.o: $S/dev/ata/atascsi.c
mainbus.o: $S/arch/amd64/amd64/mainbus.c
bios.o: $S/arch/amd64/amd64/bios.c
mpbios.o: $S/arch/amd64/amd64/mpbios.c
mpbios_intr_fixup.o: $S/arch/amd64/amd64/mpbios_intr_fixup.c
cpu.o: $S/arch/amd64/amd64/cpu.c
lapic.o: $S/arch/amd64/amd64/lapic.c
ioapic.o: $S/arch/amd64/amd64/ioapic.c
pci.o: $S/dev/pci/pci.c
pci_map.o: $S/dev/pci/pci_map.c
pci_quirks.o: $S/dev/pci/pci_quirks.c
pci_subr.o: $S/dev/pci/pci_subr.c
vga_pci.o: $S/dev/pci/vga_pci.c
vga_pci_common.o: $S/dev/pci/vga_pci_common.c
cy82c693.o: $S/dev/pci/cy82c693.c
ahc_pci.o: $S/dev/pci/ahc_pci.c
ahd_pci.o: $S/dev/pci/ahd_pci.c
adw_pci.o: $S/dev/pci/adw_pci.c
adwlib.o: $S/dev/ic/adwlib.c
adwmcode.o: $S/dev/microcode/adw/adwmcode.c
twe_pci.o: $S/dev/pci/twe_pci.c
arc.o: $S/dev/pci/arc.c
jmb.o: $S/dev/pci/jmb.c
ahci_pci.o: $S/dev/pci/ahci_pci.c
ami_pci.o: $S/dev/pci/ami_pci.c
mfi_pci.o: $S/dev/pci/mfi_pci.c
mfii.o: $S/dev/pci/mfii.c
ips.o: $S/dev/pci/ips.c
eap.o: $S/dev/pci/eap.c
auacer.o: $S/dev/pci/auacer.c
auich.o: $S/dev/pci/auich.c
azalia.o: $S/dev/pci/azalia.c
azalia_codec.o: $S/dev/pci/azalia_codec.c
envy.o: $S/dev/pci/envy.c
emuxki.o: $S/dev/pci/emuxki.c
auixp.o: $S/dev/pci/auixp.c
cs4280.o: $S/dev/pci/cs4280.c
yds.o: $S/dev/pci/yds.c
auvia.o: $S/dev/pci/auvia.c
gdt_pci.o: $S/dev/pci/gdt_pci.c
ciss_pci.o: $S/dev/pci/ciss_pci.c
qlw_pci.o: $S/dev/pci/qlw_pci.c
qla_pci.o: $S/dev/pci/qla_pci.c
qle.o: $S/dev/pci/qle.c
mpi_pci.o: $S/dev/pci/mpi_pci.c
mpii.o: $S/dev/pci/mpii.c
sili_pci.o: $S/dev/pci/sili_pci.c
if_de.o: $S/dev/pci/if_de.c
if_ep_pci.o: $S/dev/pci/if_ep_pci.c
if_pcn.o: $S/dev/pci/if_pcn.c
siop_pci_common.o: $S/dev/pci/siop_pci_common.c
siop_pci.o: $S/dev/pci/siop_pci.c
pciide.o: $S/dev/pci/pciide.c
ppb.o: $S/dev/pci/ppb.c
if_lmc.o: $S/dev/pci/if_lmc.c
if_lmc_common.o: $S/dev/pci/if_lmc_common.c
if_lmc_media.o: $S/dev/pci/if_lmc_media.c
if_lmc_obsd.o: $S/dev/pci/if_lmc_obsd.c
if_rl_pci.o: $S/dev/pci/if_rl_pci.c
if_re_pci.o: $S/dev/pci/if_re_pci.c
if_vr.o: $S/dev/pci/if_vr.c
if_txp.o: $S/dev/pci/if_txp.c
bktr_audio.o: $S/dev/pci/bktr/bktr_audio.c
bktr_card.o: $S/dev/pci/bktr/bktr_card.c
bktr_core.o: $S/dev/pci/bktr/bktr_core.c
bktr_os.o: $S/dev/pci/bktr/bktr_os.c
bktr_tuner.o: $S/dev/pci/bktr/bktr_tuner.c
if_xl_pci.o: $S/dev/pci/if_xl_pci.c
if_fxp_pci.o: $S/dev/pci/if_fxp_pci.c
if_em.o: $S/dev/pci/if_em.c
if_em_hw.o: $S/dev/pci/if_em_hw.c
if_em_soc.o: $S/dev/pci/if_em_soc.c
if_ixgb.o: $S/dev/pci/if_ixgb.c
ixgb_ee.o: $S/dev/pci/ixgb_ee.c
ixgb_hw.o: $S/dev/pci/ixgb_hw.c
if_ix.o: $S/dev/pci/if_ix.c
ixgbe.o: $S/dev/pci/ixgbe.c
ixgbe_82598.o: $S/dev/pci/ixgbe_82598.c
ixgbe_82599.o: $S/dev/pci/ixgbe_82599.c
ixgbe_x540.o: $S/dev/pci/ixgbe_x540.c
ixgbe_phy.o: $S/dev/pci/ixgbe_phy.c
if_xge.o: $S/dev/pci/if_xge.c
if_tht.o: $S/dev/pci/if_tht.c
if_myx.o: $S/dev/pci/if_myx.c
if_oce.o: $S/dev/pci/if_oce.c
if_dc_pci.o: $S/dev/pci/if_dc_pci.c
if_epic_pci.o: $S/dev/pci/if_epic_pci.c
if_ti_pci.o: $S/dev/pci/if_ti_pci.c
if_ne_pci.o: $S/dev/pci/if_ne_pci.c
if_gem_pci.o: $S/dev/pci/if_gem_pci.c
lofn.o: $S/dev/pci/lofn.c
hifn7751.o: $S/dev/pci/hifn7751.c
nofn.o: $S/dev/pci/nofn.c
ubsec.o: $S/dev/pci/ubsec.c
safe.o: $S/dev/pci/safe.c
if_sf_pci.o: $S/dev/pci/if_sf_pci.c
if_sis.o: $S/dev/pci/if_sis.c
if_se.o: $S/dev/pci/if_se.c
uhci_pci.o: $S/dev/pci/uhci_pci.c
ohci_pci.o: $S/dev/pci/ohci_pci.c
ehci_pci.o: $S/dev/pci/ehci_pci.c
pccbb.o: $S/dev/pci/pccbb.c
if_sk.o: $S/dev/pci/if_sk.c
if_msk.o: $S/dev/pci/if_msk.c
puc.o: $S/dev/pci/puc.c
pucdata.o: $S/dev/pci/pucdata.c
com_puc.o: $S/dev/puc/com_puc.c
lpt_puc.o: $S/dev/puc/lpt_puc.c
if_wi_pci.o: $S/dev/pci/if_wi_pci.c
if_an_pci.o: $S/dev/pci/if_an_pci.c
if_iwi.o: $S/dev/pci/if_iwi.c
if_wpi.o: $S/dev/pci/if_wpi.c
if_iwn.o: $S/dev/pci/if_iwn.c
cmpci.o: $S/dev/pci/cmpci.c
iha_pci.o: $S/dev/pci/iha_pci.c
pcscp.o: $S/dev/pci/pcscp.c
if_bge.o: $S/dev/pci/if_bge.c
if_bnx.o: $S/dev/pci/if_bnx.c
if_vge.o: $S/dev/pci/if_vge.c
if_stge.o: $S/dev/pci/if_stge.c
if_nfe.o: $S/dev/pci/if_nfe.c
if_et.o: $S/dev/pci/if_et.c
if_jme.o: $S/dev/pci/if_jme.c
if_age.o: $S/dev/pci/if_age.c
if_alc.o: $S/dev/pci/if_alc.c
if_ale.o: $S/dev/pci/if_ale.c
amdpm.o: $S/dev/pci/amdpm.c
if_bce.o: $S/dev/pci/if_bce.c
if_ath_pci.o: $S/dev/pci/if_ath_pci.c
if_athn_pci.o: $S/dev/pci/if_athn_pci.c
if_atw_pci.o: $S/dev/pci/if_atw_pci.c
if_rtw_pci.o: $S/dev/pci/if_rtw_pci.c
if_ral_pci.o: $S/dev/pci/if_ral_pci.c
if_acx_pci.o: $S/dev/pci/if_acx_pci.c
if_pgt_pci.o: $S/dev/pci/if_pgt_pci.c
if_malo_pci.o: $S/dev/pci/if_malo_pci.c
if_bwi_pci.o: $S/dev/pci/if_bwi_pci.c
if_sandrv.o: $S/dev/pci/if_sandrv.c
if_san_common.o: $S/dev/pci/if_san_common.c
if_san_obsd.o: $S/dev/pci/if_san_obsd.c
if_san_te1.o: $S/dev/pci/if_san_te1.c
if_san_xilinx.o: $S/dev/pci/if_san_xilinx.c
piixpm.o: $S/dev/pci/piixpm.c
musycc.o: $S/dev/pci/musycc.c
musycc_obsd.o: $S/dev/pci/musycc_obsd.c
if_art.o: $S/dev/pci/if_art.c
bt8370.o: $S/dev/pci/bt8370.c
if_vic.o: $S/dev/pci/if_vic.c
if_vmx.o: $S/dev/pci/if_vmx.c
vmwpvs.o: $S/dev/pci/vmwpvs.c
if_lii.o: $S/dev/pci/if_lii.c
ichiic.o: $S/dev/pci/ichiic.c
viapm.o: $S/dev/pci/viapm.c
amdiic.o: $S/dev/pci/amdiic.c
nviic.o: $S/dev/pci/nviic.c
sdhc_pci.o: $S/dev/pci/sdhc_pci.c
kate.o: $S/dev/pci/kate.c
km.o: $S/dev/pci/km.c
itherm.o: $S/dev/pci/itherm.c
rtsx_pci.o: $S/dev/pci/rtsx_pci.c
virtio.o: $S/dev/pci/virtio.c
virtio_pci.o: $S/dev/pci/virtio_pci.c
if_vio.o: $S/dev/pci/if_vio.c
vioblk.o: $S/dev/pci/vioblk.c
viomb.o: $S/dev/pci/viomb.c
viornd.o: $S/dev/pci/viornd.c
vioscsi.o: $S/dev/pci/vioscsi.c
agp.o: $S/dev/pci/agp.c
agp_i810.o: $S/dev/pci/agp_i810.c
drm_agpsupport.o: $S/dev/pci/drm/drm_agpsupport.c
drm_drv.o: $S/dev/pci/drm/drm_drv.c
drm_global.o: $S/dev/pci/drm/drm_global.c
drm_hashtab.o: $S/dev/pci/drm/drm_hashtab.c
drm_irq.o: $S/dev/pci/drm/drm_irq.c
drm_memory.o: $S/dev/pci/drm/drm_memory.c
drm_mm.o: $S/dev/pci/drm/drm_mm.c
drm_crtc.o: $S/dev/pci/drm/drm_crtc.c
drm_modes.o: $S/dev/pci/drm/drm_modes.c
drm_crtc_helper.o: $S/dev/pci/drm/drm_crtc_helper.c
drm_edid.o: $S/dev/pci/drm/drm_edid.c
drm_dp_helper.o: $S/dev/pci/drm/drm_dp_helper.c
drm_fb_helper.o: $S/dev/pci/drm/drm_fb_helper.c
ttm_agp_backend.o: $S/dev/pci/drm/ttm/ttm_agp_backend.c
ttm_bo.o: $S/dev/pci/drm/ttm/ttm_bo.c
ttm_bo_manager.o: $S/dev/pci/drm/ttm/ttm_bo_manager.c
ttm_bo_util.o: $S/dev/pci/drm/ttm/ttm_bo_util.c
ttm_bo_vm.o: $S/dev/pci/drm/ttm/ttm_bo_vm.c
ttm_execbuf_util.o: $S/dev/pci/drm/ttm/ttm_execbuf_util.c
ttm_lock.o: $S/dev/pci/drm/ttm/ttm_lock.c
ttm_memory.o: $S/dev/pci/drm/ttm/ttm_memory.c
ttm_object.o: $S/dev/pci/drm/ttm/ttm_object.c
ttm_page_alloc.o: $S/dev/pci/drm/ttm/ttm_page_alloc.c
ttm_tt.o: $S/dev/pci/drm/ttm/ttm_tt.c
i915_dma.o: $S/dev/pci/drm/i915/i915_dma.c
i915_drv.o: $S/dev/pci/drm/i915/i915_drv.c
i915_gem.o: $S/dev/pci/drm/i915/i915_gem.c
i915_gem_context.o: $S/dev/pci/drm/i915/i915_gem_context.c
i915_gem_evict.o: $S/dev/pci/drm/i915/i915_gem_evict.c
i915_gem_execbuffer.o: $S/dev/pci/drm/i915/i915_gem_execbuffer.c
i915_gem_gtt.o: $S/dev/pci/drm/i915/i915_gem_gtt.c
i915_gem_tiling.o: $S/dev/pci/drm/i915/i915_gem_tiling.c
i915_irq.o: $S/dev/pci/drm/i915/i915_irq.c
i915_suspend.o: $S/dev/pci/drm/i915/i915_suspend.c
intel_bios.o: $S/dev/pci/drm/i915/intel_bios.c
intel_crt.o: $S/dev/pci/drm/i915/intel_crt.c
intel_ddi.o: $S/dev/pci/drm/i915/intel_ddi.c
intel_display.o: $S/dev/pci/drm/i915/intel_display.c
intel_dp.o: $S/dev/pci/drm/i915/intel_dp.c
intel_dvo.o: $S/dev/pci/drm/i915/intel_dvo.c
intel_fb.o: $S/dev/pci/drm/i915/intel_fb.c
intel_hdmi.o: $S/dev/pci/drm/i915/intel_hdmi.c
intel_i2c.o: $S/dev/pci/drm/i915/intel_i2c.c
intel_lvds.o: $S/dev/pci/drm/i915/intel_lvds.c
intel_modes.o: $S/dev/pci/drm/i915/intel_modes.c
intel_opregion.o: $S/dev/pci/drm/i915/intel_opregion.c
intel_overlay.o: $S/dev/pci/drm/i915/intel_overlay.c
intel_panel.o: $S/dev/pci/drm/i915/intel_panel.c
intel_pm.o: $S/dev/pci/drm/i915/intel_pm.c
intel_ringbuffer.o: $S/dev/pci/drm/i915/intel_ringbuffer.c
intel_sdvo.o: $S/dev/pci/drm/i915/intel_sdvo.c
intel_sprite.o: $S/dev/pci/drm/i915/intel_sprite.c
intel_tv.o: $S/dev/pci/drm/i915/intel_tv.c
dvo_ch7017.o: $S/dev/pci/drm/i915/dvo_ch7017.c
dvo_ch7xxx.o: $S/dev/pci/drm/i915/dvo_ch7xxx.c
dvo_ivch.o: $S/dev/pci/drm/i915/dvo_ivch.c
dvo_ns2501.o: $S/dev/pci/drm/i915/dvo_ns2501.c
dvo_sil164.o: $S/dev/pci/drm/i915/dvo_sil164.c
dvo_tfp410.o: $S/dev/pci/drm/i915/dvo_tfp410.c
atom.o: $S/dev/pci/drm/radeon/atom.c
atombios_crtc.o: $S/dev/pci/drm/radeon/atombios_crtc.c
atombios_dp.o: $S/dev/pci/drm/radeon/atombios_dp.c
atombios_encoders.o: $S/dev/pci/drm/radeon/atombios_encoders.c
atombios_i2c.o: $S/dev/pci/drm/radeon/atombios_i2c.c
cayman_blit_shaders.o: $S/dev/pci/drm/radeon/cayman_blit_shaders.c
evergreen.o: $S/dev/pci/drm/radeon/evergreen.c
evergreen_blit_kms.o: $S/dev/pci/drm/radeon/evergreen_blit_kms.c
evergreen_blit_shaders.o: $S/dev/pci/drm/radeon/evergreen_blit_shaders.c
evergreen_cs.o: $S/dev/pci/drm/radeon/evergreen_cs.c
evergreen_hdmi.o: $S/dev/pci/drm/radeon/evergreen_hdmi.c
ni.o: $S/dev/pci/drm/radeon/ni.c
r100.o: $S/dev/pci/drm/radeon/r100.c
r200.o: $S/dev/pci/drm/radeon/r200.c
r300.o: $S/dev/pci/drm/radeon/r300.c
r420.o: $S/dev/pci/drm/radeon/r420.c
r520.o: $S/dev/pci/drm/radeon/r520.c
r600.o: $S/dev/pci/drm/radeon/r600.c
r600_audio.o: $S/dev/pci/drm/radeon/r600_audio.c
r600_blit_kms.o: $S/dev/pci/drm/radeon/r600_blit_kms.c
r600_blit_shaders.o: $S/dev/pci/drm/radeon/r600_blit_shaders.c
r600_cs.o: $S/dev/pci/drm/radeon/r600_cs.c
r600_hdmi.o: $S/dev/pci/drm/radeon/r600_hdmi.c
radeon_agp.o: $S/dev/pci/drm/radeon/radeon_agp.c
radeon_asic.o: $S/dev/pci/drm/radeon/radeon_asic.c
radeon_atombios.o: $S/dev/pci/drm/radeon/radeon_atombios.c
radeon_benchmark.o: $S/dev/pci/drm/radeon/radeon_benchmark.c
radeon_bios.o: $S/dev/pci/drm/radeon/radeon_bios.c
radeon_clocks.o: $S/dev/pci/drm/radeon/radeon_clocks.c
radeon_combios.o: $S/dev/pci/drm/radeon/radeon_combios.c
radeon_connectors.o: $S/dev/pci/drm/radeon/radeon_connectors.c
radeon_cs.o: $S/dev/pci/drm/radeon/radeon_cs.c
radeon_cursor.o: $S/dev/pci/drm/radeon/radeon_cursor.c
radeon_device.o: $S/dev/pci/drm/radeon/radeon_device.c
radeon_display.o: $S/dev/pci/drm/radeon/radeon_display.c
radeon_encoders.o: $S/dev/pci/drm/radeon/radeon_encoders.c
radeon_fb.o: $S/dev/pci/drm/radeon/radeon_fb.c
radeon_fence.o: $S/dev/pci/drm/radeon/radeon_fence.c
radeon_gart.o: $S/dev/pci/drm/radeon/radeon_gart.c
radeon_gem.o: $S/dev/pci/drm/radeon/radeon_gem.c
radeon_i2c.o: $S/dev/pci/drm/radeon/radeon_i2c.c
radeon_irq_kms.o: $S/dev/pci/drm/radeon/radeon_irq_kms.c
radeon_kms.o: $S/dev/pci/drm/radeon/radeon_kms.c
radeon_legacy_crtc.o: $S/dev/pci/drm/radeon/radeon_legacy_crtc.c
radeon_legacy_encoders.o: $S/dev/pci/drm/radeon/radeon_legacy_encoders.c
radeon_legacy_tv.o: $S/dev/pci/drm/radeon/radeon_legacy_tv.c
radeon_object.o: $S/dev/pci/drm/radeon/radeon_object.c
radeon_pm.o: $S/dev/pci/drm/radeon/radeon_pm.c
radeon_ring.o: $S/dev/pci/drm/radeon/radeon_ring.c
radeon_sa.o: $S/dev/pci/drm/radeon/radeon_sa.c
radeon_semaphore.o: $S/dev/pci/drm/radeon/radeon_semaphore.c
radeon_test.o: $S/dev/pci/drm/radeon/radeon_test.c
radeon_ttm.o: $S/dev/pci/drm/radeon/radeon_ttm.c
rs400.o: $S/dev/pci/drm/radeon/rs400.c
rs600.o: $S/dev/pci/drm/radeon/rs600.c
rs690.o: $S/dev/pci/drm/radeon/rs690.c
rv515.o: $S/dev/pci/drm/radeon/rv515.c
rv770.o: $S/dev/pci/drm/radeon/rv770.c
si.o: $S/dev/pci/drm/radeon/si.c
si_blit_shaders.o: $S/dev/pci/drm/radeon/si_blit_shaders.c
pci_machdep.o: $S/arch/amd64/pci/pci_machdep.c
pciide_machdep.o: $S/arch/amd64/pci/pciide_machdep.c
vga_post.o: $S/arch/amd64/pci/vga_post.c
pchb.o: $S/arch/amd64/pci/pchb.c
amas.o: $S/dev/pci/amas.c
agp_machdep.o: $S/arch/amd64/pci/agp_machdep.c
cardslot.o: $S/dev/cardbus/cardslot.c
cardbus.o: $S/dev/cardbus/cardbus.c
cardbus_map.o: $S/dev/cardbus/cardbus_map.c
cardbus_exrom.o: $S/dev/cardbus/cardbus_exrom.c
rbus.o: $S/dev/cardbus/rbus.c
com_cardbus.o: $S/dev/cardbus/com_cardbus.c
if_xl_cardbus.o: $S/dev/cardbus/if_xl_cardbus.c
if_dc_cardbus.o: $S/dev/cardbus/if_dc_cardbus.c
if_fxp_cardbus.o: $S/dev/cardbus/if_fxp_cardbus.c
if_rl_cardbus.o: $S/dev/cardbus/if_rl_cardbus.c
if_re_cardbus.o: $S/dev/cardbus/if_re_cardbus.c
if_ath_cardbus.o: $S/dev/cardbus/if_ath_cardbus.c
if_athn_cardbus.o: $S/dev/cardbus/if_athn_cardbus.c
if_atw_cardbus.o: $S/dev/cardbus/if_atw_cardbus.c
if_rtw_cardbus.o: $S/dev/cardbus/if_rtw_cardbus.c
if_ral_cardbus.o: $S/dev/cardbus/if_ral_cardbus.c
if_acx_cardbus.o: $S/dev/cardbus/if_acx_cardbus.c
if_pgt_cardbus.o: $S/dev/cardbus/if_pgt_cardbus.c
ehci_cardbus.o: $S/dev/cardbus/ehci_cardbus.c
ohci_cardbus.o: $S/dev/cardbus/ohci_cardbus.c
uhci_cardbus.o: $S/dev/cardbus/uhci_cardbus.c
if_malo_cardbus.o: $S/dev/cardbus/if_malo_cardbus.c
if_bwi_cardbus.o: $S/dev/cardbus/if_bwi_cardbus.c
rbus_machdep.o: $S/arch/amd64/amd64/rbus_machdep.c
pcmcia.o: $S/dev/pcmcia/pcmcia.c
pcmcia_cis.o: $S/dev/pcmcia/pcmcia_cis.c
pcmcia_cis_quirks.o: $S/dev/pcmcia/pcmcia_cis_quirks.c
if_ep_pcmcia.o: $S/dev/pcmcia/if_ep_pcmcia.c
if_ne_pcmcia.o: $S/dev/pcmcia/if_ne_pcmcia.c
aic_pcmcia.o: $S/dev/pcmcia/aic_pcmcia.c
com_pcmcia.o: $S/dev/pcmcia/com_pcmcia.c
wdc_pcmcia.o: $S/dev/pcmcia/wdc_pcmcia.c
if_sm_pcmcia.o: $S/dev/pcmcia/if_sm_pcmcia.c
if_xe.o: $S/dev/pcmcia/if_xe.c
if_cnw.o: $S/dev/pcmcia/if_cnw.c
if_wi_pcmcia.o: $S/dev/pcmcia/if_wi_pcmcia.c
if_malo.o: $S/dev/pcmcia/if_malo.c
if_an_pcmcia.o: $S/dev/pcmcia/if_an_pcmcia.c
pcib.o: $S/arch/amd64/pci/pcib.c
amdpcib.o: $S/dev/pci/amdpcib.c
tcpcib.o: $S/dev/pci/tcpcib.c
aapic.o: $S/arch/amd64/pci/aapic.c
hme.o: $S/dev/ic/hme.c
if_hme_pci.o: $S/dev/pci/if_hme_pci.c
isa.o: $S/dev/isa/isa.c
isadma.o: $S/dev/isa/isadma.c
com_isa.o: $S/dev/isa/com_isa.c
pckbc_isa.o: $S/dev/isa/pckbc_isa.c
vga_isa.o: $S/dev/isa/vga_isa.c
wdc_isa.o: $S/dev/isa/wdc_isa.c
mpu401.o: $S/dev/isa/mpu401.c
mpu_isa.o: $S/dev/isa/mpu_isa.c
pcppi.o: $S/dev/isa/pcppi.c
spkr.o: $S/dev/isa/spkr.c
lpt_isa.o: $S/dev/isa/lpt_isa.c
wbsio.o: $S/dev/isa/wbsio.c
sch311x.o: $S/dev/isa/sch311x.c
lm78_isa.o: $S/dev/isa/lm78_isa.c
it.o: $S/dev/isa/it.c
uguru.o: $S/dev/isa/uguru.c
aps.o: $S/dev/isa/aps.c
isa_machdep.o: $S/arch/amd64/isa/isa_machdep.c
wsdisplay.o: $S/dev/wscons/wsdisplay.c
wsdisplay_compat_usl.o: $S/dev/wscons/wsdisplay_compat_usl.c
wsevent.o: $S/dev/wscons/wsevent.c
wskbd.o: $S/dev/wscons/wskbd.c
wskbdutil.o: $S/dev/wscons/wskbdutil.c
wsmouse.o: $S/dev/wscons/wsmouse.c
wsmux.o: $S/dev/wscons/wsmux.c
wsemulconf.o: $S/dev/wscons/wsemulconf.c
wsemul_subr.o: $S/dev/wscons/wsemul_subr.c
wsemul_vt100.o: $S/dev/wscons/wsemul_vt100.c
wsemul_vt100_subr.o: $S/dev/wscons/wsemul_vt100_subr.c
wsemul_vt100_chars.o: $S/dev/wscons/wsemul_vt100_chars.c
wsemul_vt100_keys.o: $S/dev/wscons/wsemul_vt100_keys.c
pckbd.o: $S/dev/pckbc/pckbd.c
wskbdmap_mfii.o: $S/dev/pckbc/wskbdmap_mfii.c
pms.o: $S/dev/pckbc/pms.c
wscons_machdep.o: $S/arch/amd64/amd64/wscons_machdep.c
fdc.o: $S/dev/isa/fdc.c
fd.o: $S/dev/isa/fd.c
pctr.o: $S/arch/amd64/amd64/pctr.c
nvram.o: $S/arch/amd64/amd64/nvram.c
usb.o: $S/dev/usb/usb.c
usbdi.o: $S/dev/usb/usbdi.c
usbdi_util.o: $S/dev/usb/usbdi_util.c
usb_mem.o: $S/dev/usb/usb_mem.c
usb_subr.o: $S/dev/usb/usb_subr.c
usb_quirks.o: $S/dev/usb/usb_quirks.c
uhub.o: $S/dev/usb/uhub.c
uaudio.o: $S/dev/usb/uaudio.c
uvideo.o: $S/dev/usb/uvideo.c
udl.o: $S/dev/usb/udl.c
umidi.o: $S/dev/usb/umidi.c
umidi_quirks.o: $S/dev/usb/umidi_quirks.c
ucom.o: $S/dev/usb/ucom.c
ugen.o: $S/dev/usb/ugen.c
hid.o: $S/dev/usb/hid.c
uhidev.o: $S/dev/usb/uhidev.c
uhid.o: $S/dev/usb/uhid.c
hidkbd.o: $S/dev/usb/hidkbd.c
ukbdmap.o: $S/dev/usb/ukbdmap.c
ukbd.o: $S/dev/usb/ukbd.c
hidms.o: $S/dev/usb/hidms.c
ums.o: $S/dev/usb/ums.c
uts.o: $S/dev/usb/uts.c
ubcmtp.o: $S/dev/usb/ubcmtp.c
ucycom.o: $S/dev/usb/ucycom.c
ulpt.o: $S/dev/usb/ulpt.c
umass.o: $S/dev/usb/umass.c
umass_quirks.o: $S/dev/usb/umass_quirks.c
umass_scsi.o: $S/dev/usb/umass_scsi.c
uthum.o: $S/dev/usb/uthum.c
ugold.o: $S/dev/usb/ugold.c
utrh.o: $S/dev/usb/utrh.c
uoak_subr.o: $S/dev/usb/uoak_subr.c
uoakrh.o: $S/dev/usb/uoakrh.c
uoaklux.o: $S/dev/usb/uoaklux.c
uoakv.o: $S/dev/usb/uoakv.c
udcf.o: $S/dev/usb/udcf.c
urio.o: $S/dev/usb/urio.c
uvisor.o: $S/dev/usb/uvisor.c
udsbr.o: $S/dev/usb/udsbr.c
utwitch.o: $S/dev/usb/utwitch.c
if_aue.o: $S/dev/usb/if_aue.c
if_axe.o: $S/dev/usb/if_axe.c
if_axen.o: $S/dev/usb/if_axen.c
if_smsc.o: $S/dev/usb/if_smsc.c
if_cue.o: $S/dev/usb/if_cue.c
if_kue.o: $S/dev/usb/if_kue.c
if_cdce.o: $S/dev/usb/if_cdce.c
if_urndis.o: $S/dev/usb/if_urndis.c
if_mos.o: $S/dev/usb/if_mos.c
if_udav.o: $S/dev/usb/if_udav.c
if_upl.o: $S/dev/usb/if_upl.c
if_ugl.o: $S/dev/usb/if_ugl.c
if_url.o: $S/dev/usb/if_url.c
umodem.o: $S/dev/usb/umodem.c
uftdi.o: $S/dev/usb/uftdi.c
uplcom.o: $S/dev/usb/uplcom.c
umct.o: $S/dev/usb/umct.c
uvscom.o: $S/dev/usb/uvscom.c
ubsa.o: $S/dev/usb/ubsa.c
uslcom.o: $S/dev/usb/uslcom.c
uark.o: $S/dev/usb/uark.c
moscom.o: $S/dev/usb/moscom.c
uipaq.o: $S/dev/usb/uipaq.c
umsm.o: $S/dev/usb/umsm.c
uchcom.o: $S/dev/usb/uchcom.c
uticom.o: $S/dev/usb/uticom.c
if_wi_usb.o: $S/dev/usb/if_wi_usb.c
if_atu.o: $S/dev/usb/if_atu.c
if_ral.o: $S/dev/usb/if_ral.c
if_rum.o: $S/dev/usb/if_rum.c
if_run.o: $S/dev/usb/if_run.c
if_zyd.o: $S/dev/usb/if_zyd.c
if_upgt.o: $S/dev/usb/if_upgt.c
if_urtw.o: $S/dev/usb/if_urtw.c
if_urtwn.o: $S/dev/usb/if_urtwn.c
if_rsu.o: $S/dev/usb/if_rsu.c
if_otus.o: $S/dev/usb/if_otus.c
if_uath.o: $S/dev/usb/if_uath.c
if_athn_usb.o: $S/dev/usb/if_athn_usb.c
uow.o: $S/dev/usb/uow.c
uberry.o: $S/dev/usb/uberry.c
upd.o: $S/dev/usb/upd.c
i2c.o: $S/dev/i2c/i2c.c
i2c_exec.o: $S/dev/i2c/i2c_exec.c
i2c_scan.o: $S/dev/i2c/i2c_scan.c
i2c_bitbang.o: $S/dev/i2c/i2c_bitbang.c
lm75.o: $S/dev/i2c/lm75.c
lm93.o: $S/dev/i2c/lm93.c
lm87.o: $S/dev/i2c/lm87.c
maxim6690.o: $S/dev/i2c/maxim6690.c
ad741x.o: $S/dev/i2c/ad741x.c
adm1021.o: $S/dev/i2c/adm1021.c
adm1024.o: $S/dev/i2c/adm1024.c
adm1025.o: $S/dev/i2c/adm1025.c
adm1030.o: $S/dev/i2c/adm1030.c
adm1031.o: $S/dev/i2c/adm1031.c
ds1631.o: $S/dev/i2c/ds1631.c
adt7460.o: $S/dev/i2c/adt7460.c
lm78_i2c.o: $S/dev/i2c/lm78_i2c.c
adm1026.o: $S/dev/i2c/adm1026.c
w83793g.o: $S/dev/i2c/w83793g.c
w83795g.o: $S/dev/i2c/w83795g.c
asc7621.o: $S/dev/i2c/asc7621.c
asc7611.o: $S/dev/i2c/asc7611.c
spdmem_i2c.o: $S/dev/i2c/spdmem_i2c.c
sdtemp.o: $S/dev/i2c/sdtemp.c
lis331dl.o: $S/dev/i2c/lis331dl.c
acpi.o: $S/dev/acpi/acpi.c
acpiutil.o: $S/dev/acpi/acpiutil.c
dsdt.o: $S/dev/acpi/dsdt.c
acpidebug.o: $S/dev/acpi/acpidebug.c
acpitimer.o: $S/dev/acpi/acpitimer.c
acpiac.o: $S/dev/acpi/acpiac.c
acpibat.o: $S/dev/acpi/acpibat.c
acpibtn.o: $S/dev/acpi/acpibtn.c
acpicpu.o: $S/dev/acpi/acpicpu.c
acpihpet.o: $S/dev/acpi/acpihpet.c
acpiec.o: $S/dev/acpi/acpiec.c
acpitz.o: $S/dev/acpi/acpitz.c
acpimadt.o: $S/dev/acpi/acpimadt.c
acpimcfg.o: $S/dev/acpi/acpimcfg.c
acpiprt.o: $S/dev/acpi/acpiprt.c
acpidock.o: $S/dev/acpi/acpidock.c
acpiasus.o: $S/dev/acpi/acpiasus.c
acpithinkpad.o: $S/dev/acpi/acpithinkpad.c
acpitoshiba.o: $S/dev/acpi/acpitoshiba.c
acpisony.o: $S/dev/acpi/acpisony.c
acpivideo.o: $S/dev/acpi/acpivideo.c
acpivout.o: $S/dev/acpi/acpivout.c
acpipwrres.o: $S/dev/acpi/acpipwrres.c
atk0110.o: $S/dev/acpi/atk0110.c
acpi_machdep.o: $S/arch/amd64/amd64/acpi_machdep.c
acpi_wakecode.o: $S/arch/amd64/amd64/acpi_wakecode.S
sdmmc.o: $S/dev/sdmmc/sdmmc.c
sdmmc_cis.o: $S/dev/sdmmc/sdmmc_cis.c
sdmmc_io.o: $S/dev/sdmmc/sdmmc_io.c
sdmmc_mem.o: $S/dev/sdmmc/sdmmc_mem.c
sdmmc_scsi.o: $S/dev/sdmmc/sdmmc_scsi.c
onewire.o: $S/dev/onewire/onewire.c
onewire_subr.o: $S/dev/onewire/onewire_subr.c
owid.o: $S/dev/onewire/owid.c
owsbm.o: $S/dev/onewire/owsbm.c
owtemp.o: $S/dev/onewire/owtemp.c
owctr.o: $S/dev/onewire/owctr.c

.PHONY: config
config:
	cd ../../conf && config GENERIC
