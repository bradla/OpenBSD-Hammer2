/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "GENERIC.MP"
 */

#include <sys/param.h>
#include <sys/device.h>

extern struct cfdriver video_cd;
extern struct cfdriver audio_cd;
extern struct cfdriver midi_cd;
extern struct cfdriver vga_cd;
extern struct cfdriver wdc_cd;
extern struct cfdriver ahc_cd;
extern struct cfdriver ahd_cd;
extern struct cfdriver aic_cd;
extern struct cfdriver adw_cd;
extern struct cfdriver gdt_cd;
extern struct cfdriver twe_cd;
extern struct cfdriver ciss_cd;
extern struct cfdriver ami_cd;
extern struct cfdriver mfi_cd;
extern struct cfdriver qlw_cd;
extern struct cfdriver qla_cd;
extern struct cfdriver ahci_cd;
extern struct cfdriver mpi_cd;
extern struct cfdriver sili_cd;
extern struct cfdriver siop_cd;
extern struct cfdriver ep_cd;
extern struct cfdriver wi_cd;
extern struct cfdriver an_cd;
extern struct cfdriver xl_cd;
extern struct cfdriver fxp_cd;
extern struct cfdriver rl_cd;
extern struct cfdriver re_cd;
extern struct cfdriver dc_cd;
extern struct cfdriver sm_cd;
extern struct cfdriver epic_cd;
extern struct cfdriver ne_cd;
extern struct cfdriver gem_cd;
extern struct cfdriver ti_cd;
extern struct cfdriver com_cd;
extern struct cfdriver pckbc_cd;
extern struct cfdriver lpt_cd;
extern struct cfdriver iha_cd;
extern struct cfdriver lm_cd;
extern struct cfdriver ath_cd;
extern struct cfdriver athn_cd;
extern struct cfdriver atw_cd;
extern struct cfdriver rtw_cd;
extern struct cfdriver ral_cd;
extern struct cfdriver acx_cd;
extern struct cfdriver pgt_cd;
extern struct cfdriver sf_cd;
extern struct cfdriver malo_cd;
extern struct cfdriver bwi_cd;
extern struct cfdriver uhci_cd;
extern struct cfdriver ohci_cd;
extern struct cfdriver ehci_cd;
extern struct cfdriver sdhc_cd;
extern struct cfdriver rtsx_cd;
extern struct cfdriver radio_cd;
extern struct cfdriver ipmi_cd;
extern struct cfdriver vmt_cd;
extern struct cfdriver vscsi_cd;
extern struct cfdriver mpath_cd;
extern struct cfdriver softraid_cd;
extern struct cfdriver spdmem_cd;
extern struct cfdriver nsphy_cd;
extern struct cfdriver nsphyter_cd;
extern struct cfdriver gentbi_cd;
extern struct cfdriver qsphy_cd;
extern struct cfdriver inphy_cd;
extern struct cfdriver iophy_cd;
extern struct cfdriver eephy_cd;
extern struct cfdriver exphy_cd;
extern struct cfdriver rlphy_cd;
extern struct cfdriver lxtphy_cd;
extern struct cfdriver luphy_cd;
extern struct cfdriver mtdphy_cd;
extern struct cfdriver icsphy_cd;
extern struct cfdriver sqphy_cd;
extern struct cfdriver tqphy_cd;
extern struct cfdriver ukphy_cd;
extern struct cfdriver dcphy_cd;
extern struct cfdriver bmtphy_cd;
extern struct cfdriver brgphy_cd;
extern struct cfdriver xmphy_cd;
extern struct cfdriver amphy_cd;
extern struct cfdriver acphy_cd;
extern struct cfdriver nsgphy_cd;
extern struct cfdriver urlphy_cd;
extern struct cfdriver rgephy_cd;
extern struct cfdriver ciphy_cd;
extern struct cfdriver ipgphy_cd;
extern struct cfdriver etphy_cd;
extern struct cfdriver jmphy_cd;
extern struct cfdriver atphy_cd;
extern struct cfdriver scsibus_cd;
extern struct cfdriver cd_cd;
extern struct cfdriver ch_cd;
extern struct cfdriver sd_cd;
extern struct cfdriver st_cd;
extern struct cfdriver uk_cd;
extern struct cfdriver safte_cd;
extern struct cfdriver ses_cd;
extern struct cfdriver sym_cd;
extern struct cfdriver rdac_cd;
extern struct cfdriver emc_cd;
extern struct cfdriver hds_cd;
extern struct cfdriver atapiscsi_cd;
extern struct cfdriver wd_cd;
extern struct cfdriver mainbus_cd;
extern struct cfdriver bios_cd;
extern struct cfdriver mpbios_cd;
extern struct cfdriver cpu_cd;
extern struct cfdriver ioapic_cd;
extern struct cfdriver pci_cd;
extern struct cfdriver arc_cd;
extern struct cfdriver jmb_cd;
extern struct cfdriver mfii_cd;
extern struct cfdriver ips_cd;
extern struct cfdriver eap_cd;
extern struct cfdriver auacer_cd;
extern struct cfdriver auich_cd;
extern struct cfdriver azalia_cd;
extern struct cfdriver envy_cd;
extern struct cfdriver emu_cd;
extern struct cfdriver auixp_cd;
extern struct cfdriver clcs_cd;
extern struct cfdriver yds_cd;
extern struct cfdriver auvia_cd;
extern struct cfdriver qle_cd;
extern struct cfdriver mpii_cd;
extern struct cfdriver de_cd;
extern struct cfdriver pcn_cd;
extern struct cfdriver pciide_cd;
extern struct cfdriver ppb_cd;
extern struct cfdriver lmc_cd;
extern struct cfdriver vr_cd;
extern struct cfdriver txp_cd;
extern struct cfdriver bktr_cd;
extern struct cfdriver em_cd;
extern struct cfdriver ixgb_cd;
extern struct cfdriver ix_cd;
extern struct cfdriver xge_cd;
extern struct cfdriver thtc_cd;
extern struct cfdriver tht_cd;
extern struct cfdriver myx_cd;
extern struct cfdriver oce_cd;
extern struct cfdriver lofn_cd;
extern struct cfdriver hifn_cd;
extern struct cfdriver nofn_cd;
extern struct cfdriver ubsec_cd;
extern struct cfdriver safe_cd;
extern struct cfdriver sis_cd;
extern struct cfdriver se_cd;
extern struct cfdriver cbb_cd;
extern struct cfdriver skc_cd;
extern struct cfdriver sk_cd;
extern struct cfdriver mskc_cd;
extern struct cfdriver msk_cd;
extern struct cfdriver puc_cd;
extern struct cfdriver iwi_cd;
extern struct cfdriver wpi_cd;
extern struct cfdriver iwn_cd;
extern struct cfdriver cmpci_cd;
extern struct cfdriver pcscp_cd;
extern struct cfdriver bge_cd;
extern struct cfdriver bnx_cd;
extern struct cfdriver vge_cd;
extern struct cfdriver stge_cd;
extern struct cfdriver nfe_cd;
extern struct cfdriver et_cd;
extern struct cfdriver jme_cd;
extern struct cfdriver age_cd;
extern struct cfdriver alc_cd;
extern struct cfdriver ale_cd;
extern struct cfdriver amdpm_cd;
extern struct cfdriver bce_cd;
extern struct cfdriver san_cd;
extern struct cfdriver piixpm_cd;
extern struct cfdriver musycc_cd;
extern struct cfdriver art_cd;
extern struct cfdriver vic_cd;
extern struct cfdriver vmx_cd;
extern struct cfdriver vmwpvs_cd;
extern struct cfdriver lii_cd;
extern struct cfdriver ichiic_cd;
extern struct cfdriver viapm_cd;
extern struct cfdriver amdiic_cd;
extern struct cfdriver nviic_cd;
extern struct cfdriver kate_cd;
extern struct cfdriver km_cd;
extern struct cfdriver itherm_cd;
extern struct cfdriver virtio_cd;
extern struct cfdriver vio_cd;
extern struct cfdriver vioblk_cd;
extern struct cfdriver viomb_cd;
extern struct cfdriver viornd_cd;
extern struct cfdriver vioscsi_cd;
extern struct cfdriver agp_cd;
extern struct cfdriver intagp_cd;
extern struct cfdriver drm_cd;
extern struct cfdriver inteldrm_cd;
extern struct cfdriver radeondrm_cd;
extern struct cfdriver pchb_cd;
extern struct cfdriver amas_cd;
extern struct cfdriver cardslot_cd;
extern struct cfdriver cardbus_cd;
extern struct cfdriver pcmcia_cd;
extern struct cfdriver xe_cd;
extern struct cfdriver cnw_cd;
extern struct cfdriver pcib_cd;
extern struct cfdriver amdpcib_cd;
extern struct cfdriver tcpcib_cd;
extern struct cfdriver aapic_cd;
extern struct cfdriver hme_cd;
extern struct cfdriver isa_cd;
extern struct cfdriver isadma_cd;
extern struct cfdriver mpu_cd;
extern struct cfdriver pcppi_cd;
extern struct cfdriver spkr_cd;
extern struct cfdriver wbsio_cd;
extern struct cfdriver schsio_cd;
extern struct cfdriver it_cd;
extern struct cfdriver uguru_cd;
extern struct cfdriver aps_cd;
extern struct cfdriver wsdisplay_cd;
extern struct cfdriver wskbd_cd;
extern struct cfdriver wsmouse_cd;
extern struct cfdriver pckbd_cd;
extern struct cfdriver pms_cd;
extern struct cfdriver fdc_cd;
extern struct cfdriver fd_cd;
extern struct cfdriver usb_cd;
extern struct cfdriver uhub_cd;
extern struct cfdriver uaudio_cd;
extern struct cfdriver uvideo_cd;
extern struct cfdriver udl_cd;
extern struct cfdriver umidi_cd;
extern struct cfdriver ucom_cd;
extern struct cfdriver ugen_cd;
extern struct cfdriver uhidev_cd;
extern struct cfdriver uhid_cd;
extern struct cfdriver ukbd_cd;
extern struct cfdriver ums_cd;
extern struct cfdriver uts_cd;
extern struct cfdriver ubcmtp_cd;
extern struct cfdriver ucycom_cd;
extern struct cfdriver ulpt_cd;
extern struct cfdriver umass_cd;
extern struct cfdriver uthum_cd;
extern struct cfdriver ugold_cd;
extern struct cfdriver utrh_cd;
extern struct cfdriver uoakrh_cd;
extern struct cfdriver uoaklux_cd;
extern struct cfdriver uoakv_cd;
extern struct cfdriver udcf_cd;
extern struct cfdriver urio_cd;
extern struct cfdriver uvisor_cd;
extern struct cfdriver udsbr_cd;
extern struct cfdriver utwitch_cd;
extern struct cfdriver aue_cd;
extern struct cfdriver axe_cd;
extern struct cfdriver axen_cd;
extern struct cfdriver smsc_cd;
extern struct cfdriver cue_cd;
extern struct cfdriver kue_cd;
extern struct cfdriver cdce_cd;
extern struct cfdriver urndis_cd;
extern struct cfdriver mos_cd;
extern struct cfdriver udav_cd;
extern struct cfdriver upl_cd;
extern struct cfdriver ugl_cd;
extern struct cfdriver url_cd;
extern struct cfdriver umodem_cd;
extern struct cfdriver uftdi_cd;
extern struct cfdriver uplcom_cd;
extern struct cfdriver umct_cd;
extern struct cfdriver uvscom_cd;
extern struct cfdriver ubsa_cd;
extern struct cfdriver uslcom_cd;
extern struct cfdriver uark_cd;
extern struct cfdriver moscom_cd;
extern struct cfdriver uipaq_cd;
extern struct cfdriver umsm_cd;
extern struct cfdriver uchcom_cd;
extern struct cfdriver uticom_cd;
extern struct cfdriver atu_cd;
extern struct cfdriver ural_cd;
extern struct cfdriver rum_cd;
extern struct cfdriver run_cd;
extern struct cfdriver zyd_cd;
extern struct cfdriver upgt_cd;
extern struct cfdriver urtw_cd;
extern struct cfdriver urtwn_cd;
extern struct cfdriver rsu_cd;
extern struct cfdriver otus_cd;
extern struct cfdriver uath_cd;
extern struct cfdriver uow_cd;
extern struct cfdriver uberry_cd;
extern struct cfdriver upd_cd;
extern struct cfdriver iic_cd;
extern struct cfdriver lmtemp_cd;
extern struct cfdriver lmn_cd;
extern struct cfdriver lmenv_cd;
extern struct cfdriver maxtmp_cd;
extern struct cfdriver adc_cd;
extern struct cfdriver admtemp_cd;
extern struct cfdriver admlc_cd;
extern struct cfdriver admtm_cd;
extern struct cfdriver admtmp_cd;
extern struct cfdriver admtt_cd;
extern struct cfdriver maxds_cd;
extern struct cfdriver adt_cd;
extern struct cfdriver admcts_cd;
extern struct cfdriver wbng_cd;
extern struct cfdriver nvt_cd;
extern struct cfdriver adl_cd;
extern struct cfdriver andl_cd;
extern struct cfdriver sdtemp_cd;
extern struct cfdriver lisa_cd;
extern struct cfdriver acpi_cd;
extern struct cfdriver acpitimer_cd;
extern struct cfdriver acpiac_cd;
extern struct cfdriver acpibat_cd;
extern struct cfdriver acpibtn_cd;
extern struct cfdriver acpicpu_cd;
extern struct cfdriver acpihpet_cd;
extern struct cfdriver acpiec_cd;
extern struct cfdriver acpitz_cd;
extern struct cfdriver acpimadt_cd;
extern struct cfdriver acpimcfg_cd;
extern struct cfdriver acpiprt_cd;
extern struct cfdriver acpidock_cd;
extern struct cfdriver acpiasus_cd;
extern struct cfdriver acpithinkpad_cd;
extern struct cfdriver acpitoshiba_cd;
extern struct cfdriver acpisony_cd;
extern struct cfdriver acpivideo_cd;
extern struct cfdriver acpivout_cd;
extern struct cfdriver acpipwrres_cd;
extern struct cfdriver aibs_cd;
extern struct cfdriver sdmmc_cd;
extern struct cfdriver onewire_cd;
extern struct cfdriver owid_cd;
extern struct cfdriver owsbm_cd;
extern struct cfdriver owtemp_cd;
extern struct cfdriver owctr_cd;

extern struct cfattach video_ca;
extern struct cfattach audio_ca;
extern struct cfattach midi_ca;
extern struct cfattach radio_ca;
extern struct cfattach vscsi_ca;
extern struct cfattach mpath_ca;
extern struct cfattach softraid_ca;
extern struct cfattach nsphy_ca;
extern struct cfattach nsphyter_ca;
extern struct cfattach gentbi_ca;
extern struct cfattach qsphy_ca;
extern struct cfattach inphy_ca;
extern struct cfattach iophy_ca;
extern struct cfattach eephy_ca;
extern struct cfattach exphy_ca;
extern struct cfattach rlphy_ca;
extern struct cfattach lxtphy_ca;
extern struct cfattach luphy_ca;
extern struct cfattach mtdphy_ca;
extern struct cfattach icsphy_ca;
extern struct cfattach sqphy_ca;
extern struct cfattach tqphy_ca;
extern struct cfattach ukphy_ca;
extern struct cfattach dcphy_ca;
extern struct cfattach bmtphy_ca;
extern struct cfattach brgphy_ca;
extern struct cfattach xmphy_ca;
extern struct cfattach amphy_ca;
extern struct cfattach acphy_ca;
extern struct cfattach nsgphy_ca;
extern struct cfattach urlphy_ca;
extern struct cfattach rgephy_ca;
extern struct cfattach ciphy_ca;
extern struct cfattach ipgphy_ca;
extern struct cfattach etphy_ca;
extern struct cfattach jmphy_ca;
extern struct cfattach atphy_ca;
extern struct cfattach scsibus_ca;
extern struct cfattach cd_ca;
extern struct cfattach ch_ca;
extern struct cfattach sd_ca;
extern struct cfattach st_ca;
extern struct cfattach uk_ca;
extern struct cfattach safte_ca;
extern struct cfattach ses_ca;
extern struct cfattach sym_ca;
extern struct cfattach rdac_ca;
extern struct cfattach emc_ca;
extern struct cfattach hds_ca;
extern struct cfattach atapiscsi_ca;
extern struct cfattach wd_ca;
extern struct cfattach mainbus_ca;
extern struct cfattach bios_ca;
extern struct cfattach mpbios_ca;
extern struct cfattach cpu_ca;
extern struct cfattach ioapic_ca;
extern struct cfattach pci_ca;
extern struct cfattach vga_pci_ca;
extern struct cfattach ahc_pci_ca;
extern struct cfattach ahd_pci_ca;
extern struct cfattach adw_pci_ca;
extern struct cfattach twe_pci_ca;
extern struct cfattach arc_ca;
extern struct cfattach jmb_ca;
extern struct cfattach ahci_pci_ca;
extern struct cfattach ahci_jmb_ca;
extern struct cfattach ami_pci_ca;
extern struct cfattach mfi_pci_ca;
extern struct cfattach mfii_ca;
extern struct cfattach ips_ca;
extern struct cfattach eap_ca;
extern struct cfattach auacer_ca;
extern struct cfattach auich_ca;
extern struct cfattach azalia_ca;
extern struct cfattach envy_ca;
extern struct cfattach emu_ca;
extern struct cfattach auixp_ca;
extern struct cfattach clcs_ca;
extern struct cfattach yds_ca;
extern struct cfattach auvia_ca;
extern struct cfattach gdt_pci_ca;
extern struct cfattach ciss_pci_ca;
extern struct cfattach qlw_pci_ca;
extern struct cfattach qla_pci_ca;
extern struct cfattach qle_ca;
extern struct cfattach mpi_pci_ca;
extern struct cfattach mpii_ca;
extern struct cfattach sili_pci_ca;
extern struct cfattach de_ca;
extern struct cfattach ep_pci_ca;
extern struct cfattach pcn_ca;
extern struct cfattach siop_pci_ca;
extern struct cfattach pciide_pci_ca;
extern struct cfattach pciide_jmb_ca;
extern struct cfattach ppb_ca;
extern struct cfattach lmc_ca;
extern struct cfattach rl_pci_ca;
extern struct cfattach re_pci_ca;
extern struct cfattach vr_ca;
extern struct cfattach txp_ca;
extern struct cfattach bktr_ca;
extern struct cfattach xl_pci_ca;
extern struct cfattach fxp_pci_ca;
extern struct cfattach em_ca;
extern struct cfattach ixgb_ca;
extern struct cfattach ix_ca;
extern struct cfattach xge_ca;
extern struct cfattach thtc_ca;
extern struct cfattach tht_ca;
extern struct cfattach myx_ca;
extern struct cfattach oce_ca;
extern struct cfattach dc_pci_ca;
extern struct cfattach epic_pci_ca;
extern struct cfattach ti_pci_ca;
extern struct cfattach ne_pci_ca;
extern struct cfattach gem_pci_ca;
extern struct cfattach lofn_ca;
extern struct cfattach hifn_ca;
extern struct cfattach nofn_ca;
extern struct cfattach ubsec_ca;
extern struct cfattach safe_ca;
extern struct cfattach sf_pci_ca;
extern struct cfattach sis_ca;
extern struct cfattach se_ca;
extern struct cfattach uhci_pci_ca;
extern struct cfattach ohci_pci_ca;
extern struct cfattach ehci_pci_ca;
extern struct cfattach cbb_pci_ca;
extern struct cfattach skc_ca;
extern struct cfattach sk_ca;
extern struct cfattach mskc_ca;
extern struct cfattach msk_ca;
extern struct cfattach com_puc_ca;
extern struct cfattach lpt_puc_ca;
extern struct cfattach puc_pci_ca;
extern struct cfattach wi_pci_ca;
extern struct cfattach an_pci_ca;
extern struct cfattach iwi_ca;
extern struct cfattach wpi_ca;
extern struct cfattach iwn_ca;
extern struct cfattach cmpci_ca;
extern struct cfattach iha_pci_ca;
extern struct cfattach pcscp_ca;
extern struct cfattach bge_ca;
extern struct cfattach bnx_ca;
extern struct cfattach vge_ca;
extern struct cfattach stge_ca;
extern struct cfattach nfe_ca;
extern struct cfattach et_ca;
extern struct cfattach jme_ca;
extern struct cfattach age_ca;
extern struct cfattach alc_ca;
extern struct cfattach ale_ca;
extern struct cfattach amdpm_ca;
extern struct cfattach bce_ca;
extern struct cfattach ath_pci_ca;
extern struct cfattach athn_pci_ca;
extern struct cfattach atw_pci_ca;
extern struct cfattach rtw_pci_ca;
extern struct cfattach ral_pci_ca;
extern struct cfattach acx_pci_ca;
extern struct cfattach pgt_pci_ca;
extern struct cfattach malo_pci_ca;
extern struct cfattach bwi_pci_ca;
extern struct cfattach san_ca;
extern struct cfattach piixpm_ca;
extern struct cfattach musycc_ca;
extern struct cfattach art_ca;
extern struct cfattach vic_ca;
extern struct cfattach vmx_ca;
extern struct cfattach vmwpvs_ca;
extern struct cfattach lii_ca;
extern struct cfattach ichiic_ca;
extern struct cfattach viapm_ca;
extern struct cfattach amdiic_ca;
extern struct cfattach nviic_ca;
extern struct cfattach sdhc_pci_ca;
extern struct cfattach kate_ca;
extern struct cfattach km_ca;
extern struct cfattach itherm_ca;
extern struct cfattach rtsx_pci_ca;
extern struct cfattach virtio_pci_ca;
extern struct cfattach vio_ca;
extern struct cfattach vioblk_ca;
extern struct cfattach viomb_ca;
extern struct cfattach viornd_ca;
extern struct cfattach vioscsi_ca;
extern struct cfattach agp_ca;
extern struct cfattach intagp_ca;
extern struct cfattach drm_ca;
extern struct cfattach inteldrm_ca;
extern struct cfattach radeondrm_ca;
extern struct cfattach pchb_ca;
extern struct cfattach amas_ca;
extern struct cfattach cardslot_ca;
extern struct cfattach cardbus_ca;
extern struct cfattach com_cardbus_ca;
extern struct cfattach xl_cardbus_ca;
extern struct cfattach dc_cardbus_ca;
extern struct cfattach fxp_cardbus_ca;
extern struct cfattach rl_cardbus_ca;
extern struct cfattach re_cardbus_ca;
extern struct cfattach ath_cardbus_ca;
extern struct cfattach athn_cardbus_ca;
extern struct cfattach atw_cardbus_ca;
extern struct cfattach rtw_cardbus_ca;
extern struct cfattach ral_cardbus_ca;
extern struct cfattach acx_cardbus_ca;
extern struct cfattach pgt_cardbus_ca;
extern struct cfattach ehci_cardbus_ca;
extern struct cfattach ohci_cardbus_ca;
extern struct cfattach uhci_cardbus_ca;
extern struct cfattach malo_cardbus_ca;
extern struct cfattach bwi_cardbus_ca;
extern struct cfattach pcmcia_ca;
extern struct cfattach ep_pcmcia_ca;
extern struct cfattach ne_pcmcia_ca;
extern struct cfattach aic_pcmcia_ca;
extern struct cfattach com_pcmcia_ca;
extern struct cfattach wdc_pcmcia_ca;
extern struct cfattach sm_pcmcia_ca;
extern struct cfattach xe_pcmcia_ca;
extern struct cfattach cnw_ca;
extern struct cfattach wi_pcmcia_ca;
extern struct cfattach malo_pcmcia_ca;
extern struct cfattach an_pcmcia_ca;
extern struct cfattach pcib_ca;
extern struct cfattach amdpcib_ca;
extern struct cfattach tcpcib_ca;
extern struct cfattach aapic_ca;
extern struct cfattach hme_pci_ca;
extern struct cfattach isa_ca;
extern struct cfattach isadma_ca;
extern struct cfattach com_isa_ca;
extern struct cfattach pckbc_isa_ca;
extern struct cfattach vga_isa_ca;
extern struct cfattach wdc_isa_ca;
extern struct cfattach mpu_isa_ca;
extern struct cfattach pcppi_ca;
extern struct cfattach spkr_ca;
extern struct cfattach lpt_isa_ca;
extern struct cfattach wbsio_ca;
extern struct cfattach schsio_ca;
extern struct cfattach lm_isa_ca;
extern struct cfattach lm_wbsio_ca;
extern struct cfattach it_ca;
extern struct cfattach uguru_ca;
extern struct cfattach aps_ca;
extern struct cfattach wsdisplay_emul_ca;
extern struct cfattach wskbd_ca;
extern struct cfattach wsmouse_ca;
extern struct cfattach pckbd_ca;
extern struct cfattach pms_ca;
extern struct cfattach fdc_ca;
extern struct cfattach fd_ca;
extern struct cfattach usb_ca;
extern struct cfattach uhub_ca;
extern struct cfattach uhub_uhub_ca;
extern struct cfattach uaudio_ca;
extern struct cfattach uvideo_ca;
extern struct cfattach udl_ca;
extern struct cfattach umidi_ca;
extern struct cfattach ucom_ca;
extern struct cfattach ugen_ca;
extern struct cfattach uhidev_ca;
extern struct cfattach uhid_ca;
extern struct cfattach ukbd_ca;
extern struct cfattach ums_ca;
extern struct cfattach uts_ca;
extern struct cfattach ubcmtp_ca;
extern struct cfattach ucycom_ca;
extern struct cfattach ulpt_ca;
extern struct cfattach umass_ca;
extern struct cfattach uthum_ca;
extern struct cfattach ugold_ca;
extern struct cfattach utrh_ca;
extern struct cfattach uoakrh_ca;
extern struct cfattach uoaklux_ca;
extern struct cfattach uoakv_ca;
extern struct cfattach udcf_ca;
extern struct cfattach urio_ca;
extern struct cfattach uvisor_ca;
extern struct cfattach udsbr_ca;
extern struct cfattach utwitch_ca;
extern struct cfattach aue_ca;
extern struct cfattach axe_ca;
extern struct cfattach axen_ca;
extern struct cfattach smsc_ca;
extern struct cfattach cue_ca;
extern struct cfattach kue_ca;
extern struct cfattach cdce_ca;
extern struct cfattach urndis_ca;
extern struct cfattach mos_ca;
extern struct cfattach udav_ca;
extern struct cfattach upl_ca;
extern struct cfattach ugl_ca;
extern struct cfattach url_ca;
extern struct cfattach umodem_ca;
extern struct cfattach uftdi_ca;
extern struct cfattach uplcom_ca;
extern struct cfattach umct_ca;
extern struct cfattach uvscom_ca;
extern struct cfattach ubsa_ca;
extern struct cfattach uslcom_ca;
extern struct cfattach uark_ca;
extern struct cfattach moscom_ca;
extern struct cfattach uipaq_ca;
extern struct cfattach umsm_ca;
extern struct cfattach uchcom_ca;
extern struct cfattach uticom_ca;
extern struct cfattach wi_usb_ca;
extern struct cfattach atu_ca;
extern struct cfattach ural_ca;
extern struct cfattach rum_ca;
extern struct cfattach run_ca;
extern struct cfattach zyd_ca;
extern struct cfattach upgt_ca;
extern struct cfattach urtw_ca;
extern struct cfattach urtwn_ca;
extern struct cfattach rsu_ca;
extern struct cfattach otus_ca;
extern struct cfattach uath_ca;
extern struct cfattach athn_usb_ca;
extern struct cfattach uow_ca;
extern struct cfattach uberry_ca;
extern struct cfattach upd_ca;
extern struct cfattach iic_ca;
extern struct cfattach lmtemp_ca;
extern struct cfattach lmn_ca;
extern struct cfattach lmenv_ca;
extern struct cfattach maxtmp_ca;
extern struct cfattach adc_ca;
extern struct cfattach admtemp_ca;
extern struct cfattach admlc_ca;
extern struct cfattach admtm_ca;
extern struct cfattach admtmp_ca;
extern struct cfattach admtt_ca;
extern struct cfattach maxds_ca;
extern struct cfattach adt_ca;
extern struct cfattach lm_i2c_ca;
extern struct cfattach admcts_ca;
extern struct cfattach wbng_ca;
extern struct cfattach nvt_ca;
extern struct cfattach adl_ca;
extern struct cfattach andl_ca;
extern struct cfattach spdmem_iic_ca;
extern struct cfattach sdtemp_ca;
extern struct cfattach lisa_ca;
extern struct cfattach acpi_ca;
extern struct cfattach acpitimer_ca;
extern struct cfattach acpiac_ca;
extern struct cfattach acpibat_ca;
extern struct cfattach acpibtn_ca;
extern struct cfattach acpicpu_ca;
extern struct cfattach acpihpet_ca;
extern struct cfattach acpiec_ca;
extern struct cfattach acpitz_ca;
extern struct cfattach acpimadt_ca;
extern struct cfattach acpimcfg_ca;
extern struct cfattach acpiprt_ca;
extern struct cfattach acpidock_ca;
extern struct cfattach acpiasus_ca;
extern struct cfattach acpithinkpad_ca;
extern struct cfattach acpitoshiba_ca;
extern struct cfattach acpisony_ca;
extern struct cfattach acpivideo_ca;
extern struct cfattach acpivout_ca;
extern struct cfattach acpipwrres_ca;
extern struct cfattach aibs_ca;
extern struct cfattach sdmmc_ca;
extern struct cfattach onewire_ca;
extern struct cfattach owid_ca;
extern struct cfattach owsbm_ca;
extern struct cfattach owtemp_ca;
extern struct cfattach owctr_ca;
extern struct cfattach ipmi_ca;
extern struct cfattach vmt_ca;


/* locators */
static int loc[130] = {
	0x2e, 0, -1, 0, -1, -1, -1, 0x3f0,
	0, -1, 0, 6, 2, -1, 0x1600, 0,
	-1, 0, -1, -1, -1, 0xe0, 0, -1,
	0, -1, -1, -1, 0x4e, 0, -1, 0,
	-1, -1, -1, 0x290, 0, -1, 0, -1,
	-1, -1, 0x164e, 0, -1, 0, -1, -1,
	-1, 0x162e, 0, -1, 0, -1, -1, -1,
	-1, 0, -1, 0, -1, -1, -1, 0x3f8,
	0, -1, 0, 4, -1, -1, 0x2f8, 0,
	-1, 0, 3, -1, -1, 0x3e8, 0, -1,
	0, 5, -1, -1, 0x2e8, 0, -1, 0,
	9, -1, -1, 0x1f0, 0, -1, 0, 0xe,
	-1, -1, 0x170, 0, -1, 0, 0xf, -1,
	-1, 0x330, 0, -1, 0, -1, -1, -1,
	0x378, 0, -1, 0, 7, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 1, 1,
	1, 0,
};

#ifndef MAXEXTRALOC
#define MAXEXTRALOC 32
#endif
int extraloc[MAXEXTRALOC] = { -1 };
int nextraloc = MAXEXTRALOC;
int uextraloc = 0;

char *locnames[] = {
	"phy",
	"target",
	"lun",
	"channel",
	"apid",
	"bus",
	"dev",
	"function",
	"irq",
	"port",
	"console",
	"slot",
	"size",
	"iomem",
	"iosiz",
	"drq",
	"drq2",
	"addr",
	"mux",
	"drive",
	"configuration",
	"interface",
	"vendor",
	"product",
	"release",
	"portno",
	"reportid",
};

/* each entry is an index into locnames[]; -1 terminates */
short locnamp[] = {
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 0, -1, 0,
	-1, 0, -1, 0, -1, 1, 2, -1,
	3, -1, 3, -1, 3, -1, 3, -1,
	3, -1, 4, -1, 5, -1, 5, -1,
	6, 7, -1, 7, 8, -1, 6, 7,
	-1, 9, -1, 10, -1, 10, -1, 11,
	-1, 11, -1, 9, 12, 13, 14, 8,
	15, 16, -1, 17, 12, -1, 10, 18,
	-1, 10, 18, -1, 10, 18, -1, 18,
	-1, 18, -1, 18, -1, 18, -1, 11,
	-1, 19, -1, 9, 20, 21, 22, 23,
	24, -1, 9, 20, 21, 22, 23, 24,
	-1, 25, -1, 25, -1, 25, -1, 25,
	-1, 25, -1, 25, -1, 25, -1, 25,
	-1, 25, -1, 25, -1, 25, -1, 25,
	-1, 25, -1, 25, -1, 25, -1, 26,
	-1,
};

/* size of parent vectors */
int pv_size = 232;

/* parent vectors */
short pv[232] = {
	309, 306, 305, 300, 299, 298, 297, 232, 226, 194, 167, 165, 164, 163, 162, 161,
	160, 159, 158, 157, 156, 142, 140, 131, 130, 102, 92, 129, 120, 123, 121, 122,
	119, 225, 117, 118, 100, 101, 98, 99, 107, 108, 105, 106, 90, 91, -1, 6,
	4, 381, 285, 209, 206, 193, 155, 87, 85, 70, 69, 63, 49, 5, 154, 93,
	88, 86, 66, 65, 84, 83, 68, 67, 82, 62, 81, 61, 223, 60, 59, -1,
	310, 294, 314, 315, 311, 312, 313, 316, 317, 318, 319, 320, 321, 322, 283, -1,
	271, 71, 75, 153, 78, 72, 73, 77, 80, 74, 79, 76, -1, 188, 195, 196,
	197, 198, 166, -1, 136, 137, 132, 133, 134, 135, -1, 241, 242, 224, 95, 94,
	-1, 282, 280, 281, 265, -1, 274, 71, 75, 243, -1, 51, 228, 229, 230, -1,
	51, 96, 216, -1, 240, 58, 215, -1, 214, 215, -1, 269, 270, -1, 240, 58,
	-1, 247, 248, -1, 295, 104, -1, 273, 215, -1, 279, 264, -1, 199, 203, -1,
	57, -1, 339, -1, 37, -1, 222, -1, 219, -1, 277, -1, 145, -1, 51, -1,
	218, -1, 138, -1, 239, -1, 266, -1, 204, -1, 189, -1, 141, -1, 139, -1,
	113, -1, 64, -1, 52, -1, 272, -1, 360, -1, 377, -1, 336, -1, 382, -1,
	244, -1, 233, -1, 268, -1, 211, -1,
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR
#define DNRM FSTATE_DNOTFOUND
#define DSTR FSTATE_DSTAR

struct cfdata cfdata[] = {
    /* attachment       driver        unit  state loc     flags parents nm starunit1 */
/*  0: video* at uvideo* */
    {&video_ca,		&video_cd,	 0, STAR,     loc,    0, pv+214, 0,    0},
/*  1: audio* at uaudio*|eap*|envy*|cmpci*|clcs*|auacer*|auich*|auixp*|auvia*|azalia*|yds*|emu* */
    {&audio_ca,		&audio_cd,	 0, STAR,     loc,    0, pv+96, 0,    0},
/*  2: midi* at umidi*|eap*|envy*|mpu* */
    {&midi_ca,		&midi_cd,	 0, STAR,     loc,    0, pv+134, 0,    0},
/*  3: radio* at udsbr*|bktr0 */
    {&radio_ca,		&radio_cd,	 0, STAR,     loc,    0, pv+164, 0,    0},
/*  4: vscsi0 at root */
    {&vscsi_ca,		&vscsi_cd,	 0, NORM,     loc,    0, pv+46, 0,    0},
/*  5: mpath0 at root */
    {&mpath_ca,		&mpath_cd,	 0, NORM,     loc,    0, pv+46, 0,    0},
/*  6: softraid0 at root */
    {&softraid_ca,	&softraid_cd,	 0, NORM,     loc,    0, pv+46, 0,    0},
/*  7: nsphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&nsphy_ca,		&nsphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/*  8: nsphyter* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&nsphyter_ca,	&nsphyter_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/*  9: gentbi* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&gentbi_ca,	&gentbi_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 10: qsphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&qsphy_ca,		&qsphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 11: inphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&inphy_ca,		&inphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 12: iophy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&iophy_ca,		&iophy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 13: eephy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&eephy_ca,		&eephy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 14: exphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&exphy_ca,		&exphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 15: rlphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&rlphy_ca,		&rlphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 16: lxtphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&lxtphy_ca,	&lxtphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 17: luphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&luphy_ca,		&luphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 18: mtdphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&mtdphy_ca,	&mtdphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 19: icsphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&icsphy_ca,	&icsphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 20: sqphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&sqphy_ca,		&sqphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 21: tqphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&tqphy_ca,		&tqphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 22: ukphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&ukphy_ca,		&ukphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 23: dcphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&dcphy_ca,		&dcphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 24: bmtphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&bmtphy_ca,	&bmtphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 25: brgphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&brgphy_ca,	&brgphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 26: xmphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&xmphy_ca,		&xmphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 27: amphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&amphy_ca,		&amphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 28: acphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&acphy_ca,		&acphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 29: nsgphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&nsgphy_ca,	&nsgphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 30: urlphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&urlphy_ca,	&urlphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 31: rgephy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&rgephy_ca,	&rgephy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 32: ciphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&ciphy_ca,		&ciphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 33: ipgphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&ipgphy_ca,	&ipgphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 34: etphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&etphy_ca,		&etphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 35: jmphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&jmphy_ca,		&jmphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 36: atphy* at url*|udav*|mos*|smsc*|axen*|axe*|aue*|hme*|xe*|lii*|bce*|ale*|alc*|age*|jme*|et*|nfe*|stge*|vge*|bnx*|bge*|msk*|sk*|se*|sis*|vr*|pcn*|sf*|ti*|gem*|ne*|ne*|epic*|sm*|dc*|dc*|re*|re*|rl*|rl*|fxp*|fxp*|xl*|xl*|ep*|ep* phy -1 */
    {&atphy_ca,		&atphy_cd,	 0, STAR, loc+124,    0, pv+ 0, 1,    0},
/* 37: scsibus* at softraid0|vscsi0|sdmmc*|umass*|vioscsi*|vioblk*|vmwpvs*|pcscp*|mpii*|qle*|ips*|mfii*|arc*|atapiscsi*|mpath0|iha*|siop*|sili*|mpi*|ahci*|ahci*|qla*|qlw*|mfi*|ami*|ciss*|twe*|gdt*|adw*|aic*|ahd*|ahc* */
    {&scsibus_ca,	&scsibus_cd,	 0, STAR,     loc,    0, pv+47, 92,    0},
/* 38: cd* at scsibus* target -1 lun -1 */
    {&cd_ca,		&cd_cd,		 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 39: ch* at scsibus* target -1 lun -1 */
    {&ch_ca,		&ch_cd,		 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 40: sd* at scsibus* target -1 lun -1 */
    {&sd_ca,		&sd_cd,		 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 41: st* at scsibus* target -1 lun -1 */
    {&st_ca,		&st_cd,		 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 42: uk* at scsibus* target -1 lun -1 */
    {&uk_ca,		&uk_cd,		 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 43: safte* at scsibus* target -1 lun -1 */
    {&safte_ca,		&safte_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 44: ses* at scsibus* target -1 lun -1 */
    {&ses_ca,		&ses_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 45: sym* at scsibus* target -1 lun -1 */
    {&sym_ca,		&sym_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 46: rdac* at scsibus* target -1 lun -1 */
    {&rdac_ca,		&rdac_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 47: emc* at scsibus* target -1 lun -1 */
    {&emc_ca,		&emc_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 48: hds* at scsibus* target -1 lun -1 */
    {&hds_ca,		&hds_cd,	 0, STAR, loc+123,    0, pv+180, 93,    0},
/* 49: atapiscsi* at wdc0|wdc1|wdc*|pciide*|pciide* channel -1 */
    {&atapiscsi_ca,	&atapiscsi_cd,	 0, STAR, loc+124,    0, pv+123, 96,    0},
/* 50: wd* at wdc0|wdc1|wdc*|pciide*|pciide* channel -1 drive -1 */
    {&wd_ca,		&wd_cd,		 0, STAR, loc+123,    0, pv+123, 96,    0},
/* 51: mainbus0 at root */
    {&mainbus_ca,	&mainbus_cd,	 0, NORM,     loc,    0, pv+46, 0,    0},
/* 52: bios0 at mainbus0 apid -1 */
    {&bios_ca,		&bios_cd,	 0, NORM, loc+124,    0, pv+190, 106,    0},
/* 53: mpbios0 at bios0 */
    {&mpbios_ca,	&mpbios_cd,	 0, NORM,     loc,    0, pv+212, 107,    0},
/* 54: cpu0 at mainbus0 apid -1 */
    {&cpu_ca,		&cpu_cd,	 0, NORM, loc+124,    0, pv+190, 106,    0},
/* 55: cpu* at mainbus0 apid -1 */
    {&cpu_ca,		&cpu_cd,	 1, STAR, loc+124,    0, pv+190, 106,    1},
/* 56: ioapic* at mainbus0 apid -1 */
    {&ioapic_ca,	&ioapic_cd,	 0, STAR, loc+124,    0, pv+190, 106,    0},
/* 57: pci* at mainbus0|ppb*|pchb* bus -1 */
    {&pci_ca,		&pci_cd,	 0, STAR, loc+124,    0, pv+144, 106,    0},
/* 58: vga* at pci* dev -1 function -1 */
    {&vga_pci_ca,	&vga_cd,	 1, STAR, loc+123,    0, pv+176, 112,    1},
/* 59: ahc* at pci* dev -1 function -1 */
    {&ahc_pci_ca,	&ahc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 60: ahd* at pci* dev -1 function -1 */
    {&ahd_pci_ca,	&ahd_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 61: adw* at pci* dev -1 function -1 */
    {&adw_pci_ca,	&adw_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 62: twe* at pci* dev -1 function -1 */
    {&twe_pci_ca,	&twe_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 63: arc* at pci* dev -1 function -1 */
    {&arc_ca,		&arc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 64: jmb* at pci* dev -1 function -1 */
    {&jmb_ca,		&jmb_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 65: ahci* at pci* dev -1 function -1 */
    {&ahci_pci_ca,	&ahci_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 66: ahci* at jmb* */
    {&ahci_jmb_ca,	&ahci_cd,	 0, STAR,     loc,    0, pv+210, 114,    0},
/* 67: ami* at pci* dev -1 function -1 */
    {&ami_pci_ca,	&ami_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 68: mfi* at pci* dev -1 function -1 */
    {&mfi_pci_ca,	&mfi_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 69: mfii* at pci* dev -1 function -1 */
    {&mfii_ca,		&mfii_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 70: ips* at pci* dev -1 function -1 */
    {&ips_ca,		&ips_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 71: eap* at pci* dev -1 function -1 */
    {&eap_ca,		&eap_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 72: auacer* at pci* dev -1 function -1 */
    {&auacer_ca,	&auacer_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 73: auich* at pci* dev -1 function -1 */
    {&auich_ca,		&auich_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 74: azalia* at pci* dev -1 function -1 */
    {&azalia_ca,	&azalia_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 75: envy* at pci* dev -1 function -1 */
    {&envy_ca,		&envy_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 76: emu* at pci* dev -1 function -1 */
    {&emu_ca,		&emu_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 77: auixp* at pci* dev -1 function -1 */
    {&auixp_ca,		&auixp_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 78: clcs* at pci* dev -1 function -1 */
    {&clcs_ca,		&clcs_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 79: yds* at pci* dev -1 function -1 */
    {&yds_ca,		&yds_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 80: auvia* at pci* dev -1 function -1 */
    {&auvia_ca,		&auvia_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 81: gdt* at pci* dev -1 function -1 */
    {&gdt_pci_ca,	&gdt_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 82: ciss* at pci* dev -1 function -1 */
    {&ciss_pci_ca,	&ciss_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 83: qlw* at pci* dev -1 function -1 */
    {&qlw_pci_ca,	&qlw_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 84: qla* at pci* dev -1 function -1 */
    {&qla_pci_ca,	&qla_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 85: qle* at pci* dev -1 function -1 */
    {&qle_ca,		&qle_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 86: mpi* at pci* dev -1 function -1 */
    {&mpi_pci_ca,	&mpi_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 87: mpii* at pci* dev -1 function -1 */
    {&mpii_ca,		&mpii_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 88: sili* at pci* dev -1 function -1 */
    {&sili_pci_ca,	&sili_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 89: de* at pci* dev -1 function -1 */
    {&de_ca,		&de_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 90: ep* at pci* dev -1 function -1 */
    {&ep_pci_ca,	&ep_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 91: ep* at pcmcia* function -1 irq -1 */
    {&ep_pcmcia_ca,	&ep_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/* 92: pcn* at pci* dev -1 function -1 */
    {&pcn_ca,		&pcn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 93: siop* at pci* dev -1 function -1 */
    {&siop_pci_ca,	&siop_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 94: pciide* at pci* dev -1 function -1 */
    {&pciide_pci_ca,	&pciide_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 95: pciide* at jmb* */
    {&pciide_jmb_ca,	&pciide_cd,	 0, STAR,     loc,    0, pv+210, 114,    0},
/* 96: ppb* at pci* dev -1 function -1 */
    {&ppb_ca,		&ppb_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 97: lmc* at pci* dev -1 function -1 */
    {&lmc_ca,		&lmc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 98: rl* at pci* dev -1 function -1 */
    {&rl_pci_ca,	&rl_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/* 99: rl* at cardbus* dev -1 function -1 */
    {&rl_cardbus_ca,	&rl_cd,		 0, STAR, loc+123,    0, pv+184, 118,    0},
/*100: re* at pci* dev -1 function -1 */
    {&re_pci_ca,	&re_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*101: re* at cardbus* dev -1 function -1 */
    {&re_cardbus_ca,	&re_cd,		 0, STAR, loc+123,    0, pv+184, 118,    0},
/*102: vr* at pci* dev -1 function -1 */
    {&vr_ca,		&vr_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*103: txp* at pci* dev -1 function -1 */
    {&txp_ca,		&txp_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*104: bktr0 at pci* dev -1 function -1 */
    {&bktr_ca,		&bktr_cd,	 0, NORM, loc+123,    0, pv+176, 112,    0},
/*105: xl* at pci* dev -1 function -1 */
    {&xl_pci_ca,	&xl_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*106: xl* at cardbus* dev -1 function -1 */
    {&xl_cardbus_ca,	&xl_cd,		 0, STAR, loc+123,    0, pv+184, 118,    0},
/*107: fxp* at pci* dev -1 function -1 */
    {&fxp_pci_ca,	&fxp_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*108: fxp* at cardbus* dev -1 function -1 */
    {&fxp_cardbus_ca,	&fxp_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*109: em* at pci* dev -1 function -1 */
    {&em_ca,		&em_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*110: ixgb* at pci* dev -1 function -1 */
    {&ixgb_ca,		&ixgb_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*111: ix* at pci* dev -1 function -1 */
    {&ix_ca,		&ix_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*112: xge* at pci* dev -1 function -1 */
    {&xge_ca,		&xge_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*113: thtc* at pci* dev -1 function -1 */
    {&thtc_ca,		&thtc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*114: tht* at thtc* */
    {&tht_ca,		&tht_cd,	 0, STAR,     loc,    0, pv+208, 120,    0},
/*115: myx* at pci* dev -1 function -1 */
    {&myx_ca,		&myx_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*116: oce* at pci* dev -1 function -1 */
    {&oce_ca,		&oce_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*117: dc* at pci* dev -1 function -1 */
    {&dc_pci_ca,	&dc_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*118: dc* at cardbus* dev -1 function -1 */
    {&dc_cardbus_ca,	&dc_cd,		 0, STAR, loc+123,    0, pv+184, 118,    0},
/*119: epic* at pci* dev -1 function -1 */
    {&epic_pci_ca,	&epic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*120: ti* at pci* dev -1 function -1 */
    {&ti_pci_ca,	&ti_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*121: ne* at pci* dev -1 function -1 */
    {&ne_pci_ca,	&ne_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*122: ne* at pcmcia* function -1 irq -1 */
    {&ne_pcmcia_ca,	&ne_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/*123: gem* at pci* dev -1 function -1 */
    {&gem_pci_ca,	&gem_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*124: lofn* at pci* dev -1 function -1 */
    {&lofn_ca,		&lofn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*125: hifn* at pci* dev -1 function -1 */
    {&hifn_ca,		&hifn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*126: nofn* at pci* dev -1 function -1 */
    {&nofn_ca,		&nofn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*127: ubsec* at pci* dev -1 function -1 */
    {&ubsec_ca,		&ubsec_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*128: safe* at pci* dev -1 function -1 */
    {&safe_ca,		&safe_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*129: sf* at pci* dev -1 function -1 */
    {&sf_pci_ca,	&sf_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*130: sis* at pci* dev -1 function -1 */
    {&sis_ca,		&sis_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*131: se* at pci* dev -1 function -1 */
    {&se_ca,		&se_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*132: uhci* at pci* dev -1 function -1 */
    {&uhci_pci_ca,	&uhci_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*133: uhci* at cardbus* dev -1 function -1 */
    {&uhci_cardbus_ca,	&uhci_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*134: ohci* at pci* dev -1 function -1 */
    {&ohci_pci_ca,	&ohci_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*135: ohci* at cardbus* dev -1 function -1 */
    {&ohci_cardbus_ca,	&ohci_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*136: ehci* at pci* dev -1 function -1 */
    {&ehci_pci_ca,	&ehci_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*137: ehci* at cardbus* dev -1 function -1 */
    {&ehci_cardbus_ca,	&ehci_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*138: cbb* at pci* dev -1 function -1 */
    {&cbb_pci_ca,	&cbb_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*139: skc* at pci* dev -1 function -1 */
    {&skc_ca,		&skc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*140: sk* at skc* */
    {&sk_ca,		&sk_cd,		 0, STAR,     loc,    0, pv+206, 120,    0},
/*141: mskc* at pci* dev -1 function -1 */
    {&mskc_ca,		&mskc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*142: msk* at mskc* */
    {&msk_ca,		&msk_cd,	 0, STAR,     loc,    0, pv+204, 120,    0},
/*143: com* at puc* port -1 */
    {&com_puc_ca,	&com_cd,	 4, STAR, loc+124,    0, pv+188, 121,    4},
/*144: lpt* at puc* port -1 */
    {&lpt_puc_ca,	&lpt_cd,	 1, STAR, loc+124,    0, pv+188, 121,    1},
/*145: puc* at pci* dev -1 function -1 */
    {&puc_pci_ca,	&puc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*146: wi* at pci* dev -1 function -1 */
    {&wi_pci_ca,	&wi_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*147: wi* at pcmcia* function -1 irq -1 */
    {&wi_pcmcia_ca,	&wi_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/*148: an* at pci* dev -1 function -1 */
    {&an_pci_ca,	&an_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*149: an* at pcmcia* function -1 irq -1 */
    {&an_pcmcia_ca,	&an_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/*150: iwi* at pci* dev -1 function -1 */
    {&iwi_ca,		&iwi_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*151: wpi* at pci* dev -1 function -1 */
    {&wpi_ca,		&wpi_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*152: iwn* at pci* dev -1 function -1 */
    {&iwn_ca,		&iwn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*153: cmpci* at pci* dev -1 function -1 */
    {&cmpci_ca,		&cmpci_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*154: iha* at pci* dev -1 function -1 */
    {&iha_pci_ca,	&iha_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*155: pcscp* at pci* dev -1 function -1 */
    {&pcscp_ca,		&pcscp_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*156: bge* at pci* dev -1 function -1 */
    {&bge_ca,		&bge_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*157: bnx* at pci* dev -1 function -1 */
    {&bnx_ca,		&bnx_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*158: vge* at pci* dev -1 function -1 */
    {&vge_ca,		&vge_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*159: stge* at pci* dev -1 function -1 */
    {&stge_ca,		&stge_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*160: nfe* at pci* dev -1 function -1 */
    {&nfe_ca,		&nfe_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*161: et* at pci* dev -1 function -1 */
    {&et_ca,		&et_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*162: jme* at pci* dev -1 function -1 */
    {&jme_ca,		&jme_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*163: age* at pci* dev -1 function -1 */
    {&age_ca,		&age_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*164: alc* at pci* dev -1 function -1 */
    {&alc_ca,		&alc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*165: ale* at pci* dev -1 function -1 */
    {&ale_ca,		&ale_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*166: amdpm* at pci* dev -1 function -1 */
    {&amdpm_ca,		&amdpm_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*167: bce* at pci* dev -1 function -1 */
    {&bce_ca,		&bce_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*168: ath* at pci* dev -1 function -1 */
    {&ath_pci_ca,	&ath_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*169: ath* at cardbus* dev -1 function -1 */
    {&ath_cardbus_ca,	&ath_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*170: athn* at pci* dev -1 function -1 */
    {&athn_pci_ca,	&athn_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*171: athn* at cardbus* dev -1 function -1 */
    {&athn_cardbus_ca,	&athn_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*172: atw* at pci* dev -1 function -1 */
    {&atw_pci_ca,	&atw_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*173: atw* at cardbus* dev -1 function -1 */
    {&atw_cardbus_ca,	&atw_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*174: rtw* at pci* dev -1 function -1 */
    {&rtw_pci_ca,	&rtw_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*175: rtw* at cardbus* dev -1 function -1 */
    {&rtw_cardbus_ca,	&rtw_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*176: ral* at pci* dev -1 function -1 */
    {&ral_pci_ca,	&ral_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*177: ral* at cardbus* dev -1 function -1 */
    {&ral_cardbus_ca,	&ral_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*178: acx* at pci* dev -1 function -1 */
    {&acx_pci_ca,	&acx_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*179: acx* at cardbus* dev -1 function -1 */
    {&acx_cardbus_ca,	&acx_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*180: pgt* at pci* dev -1 function -1 */
    {&pgt_pci_ca,	&pgt_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*181: pgt* at cardbus* dev -1 function -1 */
    {&pgt_cardbus_ca,	&pgt_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*182: malo* at pci* dev -1 function -1 */
    {&malo_pci_ca,	&malo_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*183: malo* at cardbus* dev -1 function -1 */
    {&malo_cardbus_ca,	&malo_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*184: malo* at pcmcia* function -1 irq -1 */
    {&malo_pcmcia_ca,	&malo_cd,	 0, STAR, loc+123,    0, pv+182, 115,    0},
/*185: bwi* at pci* dev -1 function -1 */
    {&bwi_pci_ca,	&bwi_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*186: bwi* at cardbus* dev -1 function -1 */
    {&bwi_cardbus_ca,	&bwi_cd,	 0, STAR, loc+123,    0, pv+184, 118,    0},
/*187: san* at pci* dev -1 function -1 */
    {&san_ca,		&san_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*188: piixpm* at pci* dev -1 function -1 */
    {&piixpm_ca,	&piixpm_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*189: musycc* at pci* dev -1 function -1 */
    {&musycc_ca,	&musycc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*190: art* at musycc* */
    {&art_ca,		&art_cd,	 0, STAR,     loc,    0, pv+202, 122,    0},
/*191: vic* at pci* dev -1 function -1 */
    {&vic_ca,		&vic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*192: vmx* at pci* dev -1 function -1 */
    {&vmx_ca,		&vmx_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*193: vmwpvs* at pci* dev -1 function -1 */
    {&vmwpvs_ca,	&vmwpvs_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*194: lii* at pci* dev -1 function -1 */
    {&lii_ca,		&lii_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*195: ichiic* at pci* dev -1 function -1 */
    {&ichiic_ca,	&ichiic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*196: viapm* at pci* dev -1 function -1 */
    {&viapm_ca,		&viapm_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*197: amdiic* at pci* dev -1 function -1 */
    {&amdiic_ca,	&amdiic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*198: nviic* at pci* dev -1 function -1 */
    {&nviic_ca,		&nviic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*199: sdhc* at pci* dev -1 function -1 */
    {&sdhc_pci_ca,	&sdhc_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*200: kate* at pci* dev -1 function -1 */
    {&kate_ca,		&kate_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*201: km* at pci* dev -1 function -1 */
    {&km_ca,		&km_cd,		 0, STAR, loc+123,    0, pv+176, 112,    0},
/*202: itherm* at pci* dev -1 function -1 */
    {&itherm_ca,	&itherm_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*203: rtsx* at pci* dev -1 function -1 */
    {&rtsx_pci_ca,	&rtsx_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*204: virtio* at pci* dev -1 function -1 */
    {&virtio_pci_ca,	&virtio_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*205: vio* at virtio* */
    {&vio_ca,		&vio_cd,	 0, STAR,     loc,    0, pv+200, 122,    0},
/*206: vioblk* at virtio* */
    {&vioblk_ca,	&vioblk_cd,	 0, STAR,     loc,    0, pv+200, 122,    0},
/*207: viomb* at virtio* */
    {&viomb_ca,		&viomb_cd,	 0, STAR,     loc,    0, pv+200, 122,    0},
/*208: viornd* at virtio* */
    {&viornd_ca,	&viornd_cd,	 0, STAR,     loc,    0, pv+200, 122,    0},
/*209: vioscsi* at virtio* */
    {&vioscsi_ca,	&vioscsi_cd,	 0, STAR,     loc,    0, pv+200, 122,    0},
/*210: agp* at intagp* */
    {&agp_ca,		&agp_cd,	 0, STAR,     loc,    0, pv+230, 122,    0},
/*211: intagp* at vga0|vga* */
    {&intagp_ca,	&intagp_cd,	 0, STAR,     loc,    0, pv+158, 122,    0},
/*212: drm0 at inteldrm*|radeondrm* console 1 */
    {&drm_ca,		&drm_cd,	 0, NORM, loc+128,    0, pv+152, 123,    0},
/*213: drm* at inteldrm*|radeondrm* console -1 */
    {&drm_ca,		&drm_cd,	 1, STAR, loc+124,    0, pv+152, 123,    1},
/*214: inteldrm* at vga0|vga* */
    {&inteldrm_ca,	&inteldrm_cd,	 0, STAR,     loc,    0, pv+158, 122,    0},
/*215: radeondrm* at pci* dev -1 function -1 */
    {&radeondrm_ca,	&radeondrm_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*216: pchb* at pci* dev -1 function -1 */
    {&pchb_ca,		&pchb_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*217: amas* at pci* dev -1 function -1 */
    {&amas_ca,		&amas_cd,	 0, DSTR, loc+123,    0, pv+176, 112,    0},
/*218: cardslot* at cbb* slot -1 */
    {&cardslot_ca,	&cardslot_cd,	 0, STAR, loc+124,    0, pv+194, 127,    0},
/*219: cardbus* at cardslot* slot -1 */
    {&cardbus_ca,	&cardbus_cd,	 0, STAR, loc+124,    0, pv+192, 129,    0},
/*220: com* at cardbus* dev -1 function -1 */
    {&com_cardbus_ca,	&com_cd,	 4, STAR, loc+123,    0, pv+184, 118,    4},
/*221: com* at pcmcia* function -1 irq -1 */
    {&com_pcmcia_ca,	&com_cd,	 4, STAR, loc+123,    0, pv+182, 115,    4},
/*222: pcmcia* at cardslot* controller -1 socket -1 */
    {&pcmcia_ca,	&pcmcia_cd,	 0, STAR, loc+123,    0, pv+192, 129,    0},
/*223: aic* at pcmcia* function -1 irq -1 */
    {&aic_pcmcia_ca,	&aic_cd,	 0, STAR, loc+123,    0, pv+182, 115,    0},
/*224: wdc* at pcmcia* function -1 irq -1 */
    {&wdc_pcmcia_ca,	&wdc_cd,	 2, STAR, loc+123,    0, pv+182, 115,    2},
/*225: sm* at pcmcia* function -1 irq -1 */
    {&sm_pcmcia_ca,	&sm_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/*226: xe* at pcmcia* function -1 irq -1 */
    {&xe_pcmcia_ca,	&xe_cd,		 0, STAR, loc+123,    0, pv+182, 115,    0},
/*227: cnw* at pcmcia* function -1 irq -1 */
    {&cnw_ca,		&cnw_cd,	 0, STAR, loc+123,    0, pv+182, 115,    0},
/*228: pcib* at pci* dev -1 function -1 */
    {&pcib_ca,		&pcib_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*229: amdpcib* at pci* dev -1 function -1 */
    {&amdpcib_ca,	&amdpcib_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*230: tcpcib* at pci* dev -1 function -1 */
    {&tcpcib_ca,	&tcpcib_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*231: aapic* at pci* dev -1 function -1 */
    {&aapic_ca,		&aapic_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*232: hme* at pci* dev -1 function -1 */
    {&hme_pci_ca,	&hme_cd,	 0, STAR, loc+123,    0, pv+176, 112,    0},
/*233: isa0 at mainbus0|pcib*|amdpcib*|tcpcib* */
    {&isa_ca,		&isa_cd,	 0, NORM,     loc,    0, pv+139, 106,    0},
/*234: isadma0 at isa0 port -1 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&isadma_ca,	&isadma_cd,	 0, NORM, loc+ 56,    0, pv+226, 131,    0},
/*235: com0 at isa0 port 0x3f8 size 0 iomem -1 iosiz 0 irq 4 drq -1 drq2 -1 */
    {&com_isa_ca,	&com_cd,	 0, NORM, loc+ 63,    0, pv+226, 131,    0},
/*236: com1 at isa0 port 0x2f8 size 0 iomem -1 iosiz 0 irq 3 drq -1 drq2 -1 */
    {&com_isa_ca,	&com_cd,	 1, NORM, loc+ 70,    0, pv+226, 131,    1},
/*237: com2 at isa0 port 0x3e8 size 0 iomem -1 iosiz 0 irq 5 drq -1 drq2 -1 */
    {&com_isa_ca,	&com_cd,	 2, NORM, loc+ 77,    0, pv+226, 131,    2},
/*238: com3 at isa0 port 0x2e8 size 0 iomem -1 iosiz 0 irq 9 drq -1 drq2 -1 */
    {&com_isa_ca,	&com_cd,	 3, DNRM, loc+ 84,    0, pv+226, 131,    3},
/*239: pckbc0 at isa0 port -1 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&pckbc_isa_ca,	&pckbc_cd,	 0, NORM, loc+ 56,    0, pv+226, 131,    0},
/*240: vga0 at isa0 port -1 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&vga_isa_ca,	&vga_cd,	 0, NORM, loc+ 56,    0, pv+226, 131,    0},
/*241: wdc0 at isa0 port 0x1f0 size 0 iomem -1 iosiz 0 irq 0xe drq -1 drq2 -1 */
    {&wdc_isa_ca,	&wdc_cd,	 0, DNRM, loc+ 91,    0, pv+226, 131,    0},
/*242: wdc1 at isa0 port 0x170 size 0 iomem -1 iosiz 0 irq 0xf drq -1 drq2 -1 */
    {&wdc_isa_ca,	&wdc_cd,	 1, DNRM, loc+ 98,    0, pv+226, 131,    1},
/*243: mpu* at isa0 port 0x330 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&mpu_isa_ca,	&mpu_cd,	 0, STAR, loc+105,    0, pv+226, 131,    0},
/*244: pcppi0 at isa0 port -1 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&pcppi_ca,		&pcppi_cd,	 0, NORM, loc+ 56,    0, pv+226, 131,    0},
/*245: spkr0 at pcppi0 */
    {&spkr_ca,		&spkr_cd,	 0, NORM,     loc,    0, pv+224, 138,    0},
/*246: lpt0 at isa0 port 0x378 size 0 iomem -1 iosiz 0 irq 7 drq -1 drq2 -1 */
    {&lpt_isa_ca,	&lpt_cd,	 0, NORM, loc+112,    0, pv+226, 131,    0},
/*247: wbsio* at isa0 port 0x2e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&wbsio_ca,		&wbsio_cd,	 0, STAR, loc+  0,    0, pv+226, 131,    0},
/*248: wbsio* at isa0 port 0x4e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&wbsio_ca,		&wbsio_cd,	 0, STAR, loc+ 28,    0, pv+226, 131,    0},
/*249: schsio* at isa0 port 0x2e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&schsio_ca,	&schsio_cd,	 0, STAR, loc+  0,    0, pv+226, 131,    0},
/*250: schsio* at isa0 port 0x4e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&schsio_ca,	&schsio_cd,	 0, STAR, loc+ 28,    0, pv+226, 131,    0},
/*251: schsio* at isa0 port 0x162e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&schsio_ca,	&schsio_cd,	 0, STAR, loc+ 49,    0, pv+226, 131,    0},
/*252: schsio* at isa0 port 0x164e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&schsio_ca,	&schsio_cd,	 0, STAR, loc+ 42,    0, pv+226, 131,    0},
/*253: lm0 at isa0 port 0x290 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&lm_isa_ca,	&lm_cd,		 0, NORM, loc+ 35,    0, pv+226, 131,    0},
/*254: lm* at wbsio*|wbsio* */
    {&lm_wbsio_ca,	&lm_cd,		 1, STAR,     loc,    0, pv+161, 138,    1},
/*255: lm* at iic* addr -1 size -1 */
    {&lm_i2c_ca,	&lm_cd,		 1, STAR, loc+123,    0, pv+178, 139,    1},
/*256: it* at isa0 port 0x2e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&it_ca,		&it_cd,		 0, STAR, loc+  0,    0, pv+226, 131,    0},
/*257: it* at isa0 port 0x4e size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&it_ca,		&it_cd,		 0, STAR, loc+ 28,    0, pv+226, 131,    0},
/*258: uguru0 at isa0 port 0xe0 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&uguru_ca,		&uguru_cd,	 0, DNRM, loc+ 21,    0, pv+226, 131,    0},
/*259: aps0 at isa0 port 0x1600 size 0 iomem -1 iosiz 0 irq -1 drq -1 drq2 -1 */
    {&aps_ca,		&aps_cd,	 0, NORM, loc+ 14,    0, pv+226, 131,    0},
/*260: wsdisplay* at udl*|radeondrm* console -1 mux 1 */
    {&wsdisplay_emul_ca,	&wsdisplay_cd,	 1, STAR, loc+125,    0, pv+167, 142,    1},
/*261: wsdisplay0 at vga0|vga*|radeondrm* console 1 mux 1 */
    {&wsdisplay_emul_ca,	&wsdisplay_cd,	 0, NORM, loc+127,    0, pv+148, 122,    0},
/*262: wskbd* at ukbd*|pckbd* console -1 mux 1 */
    {&wskbd_ca,		&wskbd_cd,	 0, STAR, loc+125,    0, pv+170, 145,    0},
/*263: wsmouse* at ubcmtp*|ums*|uts*|pms* mux 0 */
    {&wsmouse_ca,	&wsmouse_cd,	 0, STAR, loc+129,    0, pv+129, 151,    0},
/*264: pckbd* at pckbc0 slot -1 */
    {&pckbd_ca,		&pckbd_cd,	 0, STAR, loc+124,    0, pv+196, 159,    0},
/*265: pms* at pckbc0 slot -1 */
    {&pms_ca,		&pms_cd,	 0, STAR, loc+124,    0, pv+196, 159,    0},
/*266: fdc0 at isa0 port 0x3f0 size 0 iomem -1 iosiz 0 irq 6 drq 2 drq2 -1 */
    {&fdc_ca,		&fdc_cd,	 0, NORM, loc+  7,    0, pv+226, 131,    0},
/*267: fd* at fdc0 drive -1 */
    {&fd_ca,		&fd_cd,		 0, STAR, loc+124,    0, pv+198, 161,    0},
/*268: usb* at ehci*|ehci*|uhci*|uhci*|ohci*|ohci* */
    {&usb_ca,		&usb_cd,	 0, STAR,     loc,    0, pv+116, 162,    0},
/*269: uhub* at usb* */
    {&uhub_ca,		&uhub_cd,	 0, STAR,     loc,    0, pv+228, 162,    0},
/*270: uhub* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uhub_uhub_ca,	&uhub_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*271: uaudio* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uaudio_ca,	&uaudio_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*272: uvideo* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uvideo_ca,	&uvideo_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*273: udl* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&udl_ca,		&udl_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*274: umidi* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&umidi_ca,		&umidi_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*275: ucom* at umodem*|uvisor*|uvscom*|ubsa*|uftdi*|uplcom*|umct*|uslcom*|uark*|moscom*|uipaq*|umsm*|uchcom*|uticom*|ucycom* portno -1 */
    {&ucom_ca,		&ucom_cd,	 0, STAR, loc+124,    0, pv+80, 177,    0},
/*276: ugen* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ugen_ca,		&ugen_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*277: uhidev* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uhidev_ca,	&uhidev_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*278: uhid* at uhidev* reportid -1 */
    {&uhid_ca,		&uhid_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*279: ukbd* at uhidev* reportid -1 */
    {&ukbd_ca,		&ukbd_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*280: ums* at uhidev* reportid -1 */
    {&ums_ca,		&ums_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*281: uts* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uts_ca,		&uts_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*282: ubcmtp* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ubcmtp_ca,	&ubcmtp_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*283: ucycom* at uhidev* reportid -1 */
    {&ucycom_ca,	&ucycom_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*284: ulpt* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ulpt_ca,		&ulpt_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*285: umass* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&umass_ca,		&umass_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*286: uthum* at uhidev* reportid -1 */
    {&uthum_ca,		&uthum_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*287: ugold* at uhidev* reportid -1 */
    {&ugold_ca,		&ugold_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*288: utrh* at uhidev* reportid -1 */
    {&utrh_ca,		&utrh_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*289: uoakrh* at uhidev* reportid -1 */
    {&uoakrh_ca,	&uoakrh_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*290: uoaklux* at uhidev* reportid -1 */
    {&uoaklux_ca,	&uoaklux_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*291: uoakv* at uhidev* reportid -1 */
    {&uoakv_ca,		&uoakv_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*292: udcf* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&udcf_ca,		&udcf_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*293: urio* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&urio_ca,		&urio_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*294: uvisor* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uvisor_ca,	&uvisor_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*295: udsbr* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&udsbr_ca,		&udsbr_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*296: utwitch* at uhidev* reportid -1 */
    {&utwitch_ca,	&utwitch_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*297: aue* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&aue_ca,		&aue_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*298: axe* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&axe_ca,		&axe_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*299: axen* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&axen_ca,		&axen_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*300: smsc* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&smsc_ca,		&smsc_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*301: cue* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&cue_ca,		&cue_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*302: kue* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&kue_ca,		&kue_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*303: cdce* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&cdce_ca,		&cdce_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*304: urndis* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&urndis_ca,	&urndis_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*305: mos* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&mos_ca,		&mos_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*306: udav* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&udav_ca,		&udav_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*307: upl* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&upl_ca,		&upl_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*308: ugl* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ugl_ca,		&ugl_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*309: url* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&url_ca,		&url_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*310: umodem* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&umodem_ca,	&umodem_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*311: uftdi* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uftdi_ca,		&uftdi_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*312: uplcom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uplcom_ca,	&uplcom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*313: umct* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&umct_ca,		&umct_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*314: uvscom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uvscom_ca,	&uvscom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*315: ubsa* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ubsa_ca,		&ubsa_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*316: uslcom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uslcom_ca,	&uslcom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*317: uark* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uark_ca,		&uark_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*318: moscom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&moscom_ca,	&moscom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*319: uipaq* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uipaq_ca,		&uipaq_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*320: umsm* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&umsm_ca,		&umsm_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*321: uchcom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uchcom_ca,	&uchcom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*322: uticom* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uticom_ca,	&uticom_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*323: wi* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&wi_usb_ca,	&wi_cd,		 0, STAR, loc+119,    0, pv+155, 163,    0},
/*324: atu* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&atu_ca,		&atu_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*325: ural* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&ural_ca,		&ural_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*326: rum* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&rum_ca,		&rum_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*327: run* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&run_ca,		&run_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*328: zyd* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&zyd_ca,		&zyd_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*329: upgt* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&upgt_ca,		&upgt_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*330: urtw* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&urtw_ca,		&urtw_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*331: urtwn* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&urtwn_ca,		&urtwn_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*332: rsu* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&rsu_ca,		&rsu_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*333: otus* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&otus_ca,		&otus_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*334: uath* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uath_ca,		&uath_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*335: athn* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&athn_usb_ca,	&athn_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*336: uow* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uow_ca,		&uow_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*337: uberry* at uhub*|uhub* port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    {&uberry_ca,	&uberry_cd,	 0, STAR, loc+119,    0, pv+155, 163,    0},
/*338: upd* at uhidev* reportid -1 */
    {&upd_ca,		&upd_cd,	 0, STAR, loc+124,    0, pv+186, 207,    0},
/*339: iic* at piixpm*|ichiic*|viapm*|amdiic*|nviic*|amdpm* */
    {&iic_ca,		&iic_cd,	 0, STAR,     loc,    0, pv+109, 208,    0},
/*340: lmtemp* at iic* addr -1 size -1 */
    {&lmtemp_ca,	&lmtemp_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*341: lmn* at iic* addr -1 size -1 */
    {&lmn_ca,		&lmn_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*342: lmenv* at iic* addr -1 size -1 */
    {&lmenv_ca,		&lmenv_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*343: maxtmp* at iic* addr -1 size -1 */
    {&maxtmp_ca,	&maxtmp_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*344: adc* at iic* addr -1 size -1 */
    {&adc_ca,		&adc_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*345: admtemp* at iic* addr -1 size -1 */
    {&admtemp_ca,	&admtemp_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*346: admlc* at iic* addr -1 size -1 */
    {&admlc_ca,		&admlc_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*347: admtm* at iic* addr -1 size -1 */
    {&admtm_ca,		&admtm_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*348: admtmp* at iic* addr -1 size -1 */
    {&admtmp_ca,	&admtmp_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*349: admtt* at iic* addr -1 size -1 */
    {&admtt_ca,		&admtt_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*350: maxds* at iic* addr -1 size -1 */
    {&maxds_ca,		&maxds_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*351: adt* at iic* addr -1 size -1 */
    {&adt_ca,		&adt_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*352: admcts* at iic* addr -1 size -1 */
    {&admcts_ca,	&admcts_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*353: wbng* at iic* addr -1 size -1 */
    {&wbng_ca,		&wbng_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*354: nvt* at iic* addr -1 size -1 */
    {&nvt_ca,		&nvt_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*355: adl* at iic* addr -1 size -1 */
    {&adl_ca,		&adl_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*356: andl* at iic* addr -1 size -1 */
    {&andl_ca,		&andl_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*357: spdmem* at iic* addr -1 size -1 */
    {&spdmem_iic_ca,	&spdmem_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*358: sdtemp* at iic* addr -1 size -1 */
    {&sdtemp_ca,	&sdtemp_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*359: lisa* at iic* addr -1 size -1 */
    {&lisa_ca,		&lisa_cd,	 0, STAR, loc+123,    0, pv+178, 139,    0},
/*360: acpi0 at bios0 */
    {&acpi_ca,		&acpi_cd,	 0, NORM,     loc,    0, pv+212, 107,    0},
/*361: acpitimer* at acpi0 */
    {&acpitimer_ca,	&acpitimer_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*362: acpiac* at acpi0 */
    {&acpiac_ca,	&acpiac_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*363: acpibat* at acpi0 */
    {&acpibat_ca,	&acpibat_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*364: acpibtn* at acpi0 */
    {&acpibtn_ca,	&acpibtn_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*365: acpicpu* at acpi0 */
    {&acpicpu_ca,	&acpicpu_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*366: acpihpet* at acpi0 */
    {&acpihpet_ca,	&acpihpet_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*367: acpiec* at acpi0 */
    {&acpiec_ca,	&acpiec_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*368: acpitz* at acpi0 */
    {&acpitz_ca,	&acpitz_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*369: acpimadt0 at acpi0 */
    {&acpimadt_ca,	&acpimadt_cd,	 0, NORM,     loc,    0, pv+216, 208,    0},
/*370: acpimcfg* at acpi0 */
    {&acpimcfg_ca,	&acpimcfg_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*371: acpiprt* at acpi0 */
    {&acpiprt_ca,	&acpiprt_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*372: acpidock* at acpi0 */
    {&acpidock_ca,	&acpidock_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*373: acpiasus* at acpi0 */
    {&acpiasus_ca,	&acpiasus_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*374: acpithinkpad* at acpi0 */
    {&acpithinkpad_ca,	&acpithinkpad_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*375: acpitoshiba* at acpi0 */
    {&acpitoshiba_ca,	&acpitoshiba_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*376: acpisony* at acpi0 */
    {&acpisony_ca,	&acpisony_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*377: acpivideo* at acpi0 */
    {&acpivideo_ca,	&acpivideo_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*378: acpivout* at acpivideo* */
    {&acpivout_ca,	&acpivout_cd,	 0, STAR,     loc,    0, pv+218, 208,    0},
/*379: acpipwrres* at acpi0 */
    {&acpipwrres_ca,	&acpipwrres_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*380: aibs* at acpi0 */
    {&aibs_ca,		&aibs_cd,	 0, STAR,     loc,    0, pv+216, 208,    0},
/*381: sdmmc* at sdhc*|rtsx* */
    {&sdmmc_ca,		&sdmmc_cd,	 0, STAR,     loc,    0, pv+173, 208,    0},
/*382: onewire* at uow* */
    {&onewire_ca,	&onewire_cd,	 0, STAR,     loc,    0, pv+220, 208,    0},
/*383: owid* at onewire* */
    {&owid_ca,		&owid_cd,	 0, STAR,     loc,    0, pv+222, 208,    0},
/*384: owsbm* at onewire* */
    {&owsbm_ca,		&owsbm_cd,	 0, STAR,     loc,    0, pv+222, 208,    0},
/*385: owtemp* at onewire* */
    {&owtemp_ca,	&owtemp_cd,	 0, STAR,     loc,    0, pv+222, 208,    0},
/*386: owctr* at onewire* */
    {&owctr_ca,		&owctr_cd,	 0, STAR,     loc,    0, pv+222, 208,    0},
/*387: ipmi0 at mainbus0 apid -1 */
    {&ipmi_ca,		&ipmi_cd,	 0, DNRM, loc+124,    0, pv+190, 106,    0},
/*388: vmt0 at mainbus0 apid -1 */
    {&vmt_ca,		&vmt_cd,	 0, NORM, loc+124,    0, pv+190, 106,    0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {(struct cfattach *)-1}
};

short cfroots[] = {
	 4 /* vscsi0 */,
	 5 /* mpath0 */,
	 6 /* softraid0 */,
	51 /* mainbus0 */,
	-1
};

int cfroots_size = 5;

/* pseudo-devices */
extern void pfattach(int);
extern void pflogattach(int);
extern void pfsyncattach(int);
extern void pflowattach(int);
extern void encattach(int);
extern void ptyattach(int);
extern void nmeaattach(int);
extern void mstsattach(int);
extern void endrunattach(int);
extern void vndattach(int);
extern void ksymsattach(int);
extern void systraceattach(int);
extern void bpfilterattach(int);
extern void bridgeattach(int);
extern void carpattach(int);
extern void gifattach(int);
extern void greattach(int);
extern void loopattach(int);
extern void mpeattach(int);
extern void pppattach(int);
extern void pppoeattach(int);
extern void pppxattach(int);
extern void slattach(int);
extern void spppattach(int);
extern void trunkattach(int);
extern void tunattach(int);
extern void vetherattach(int);
extern void vxlanattach(int);
extern void vlanattach(int);
extern void bioattach(int);
extern void fuseattach(int);
extern void pctrattach(int);
extern void nvramattach(int);
extern void hotplugattach(int);
extern void wsmuxattach(int);
extern void cryptoattach(int);

char *pdevnames[] = {
	"pf",
	"pflog",
	"pfsync",
	"pflow",
	"enc",
	"pty",
	"nmea",
	"msts",
	"endrun",
	"vnd",
	"ksyms",
	"systrace",
	"bpfilter",
	"bridge",
	"carp",
	"gif",
	"gre",
	"loop",
	"mpe",
	"ppp",
	"pppoe",
	"pppx",
	"sl",
	"sppp",
	"trunk",
	"tun",
	"vether",
	"vxlan",
	"vlan",
	"bio",
	"fuse",
	"pctr",
	"nvram",
	"hotplug",
	"wsmux",
	"crypto",
};

int pdevnames_size = 36;

struct pdevinit pdevinit[] = {
	{ pfattach, 1 },
	{ pflogattach, 1 },
	{ pfsyncattach, 1 },
	{ pflowattach, 1 },
	{ encattach, 1 },
	{ ptyattach, 16 },
	{ nmeaattach, 1 },
	{ mstsattach, 1 },
	{ endrunattach, 1 },
	{ vndattach, 4 },
	{ ksymsattach, 1 },
	{ systraceattach, 1 },
	{ bpfilterattach, 1 },
	{ bridgeattach, 1 },
	{ carpattach, 1 },
	{ gifattach, 1 },
	{ greattach, 1 },
	{ loopattach, 1 },
	{ mpeattach, 1 },
	{ pppattach, 1 },
	{ pppoeattach, 1 },
	{ pppxattach, 1 },
	{ slattach, 1 },
	{ spppattach, 1 },
	{ trunkattach, 1 },
	{ tunattach, 1 },
	{ vetherattach, 1 },
	{ vxlanattach, 1 },
	{ vlanattach, 1 },
	{ bioattach, 1 },
	{ fuseattach, 1 },
	{ pctrattach, 1 },
	{ nvramattach, 1 },
	{ hotplugattach, 1 },
	{ wsmuxattach, 2 },
	{ cryptoattach, 1 },
	{ NULL, 0 }
};
