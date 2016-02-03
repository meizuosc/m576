# UFSHCD makefile
obj-$(CONFIG_SCSI_UFSHCD) += ufshcd.o
obj-$(CONFIG_SCSI_UFSHCD_PCI) += ufshcd-pci.o
obj-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ufshcd-pltfrm.o
obj-$(CONFIG_SCSI_UFS_EXYNOS) += ufs-exynos.o
obj-$(CONFIG_FIPS_FMP_UFS) += fips-fmp-ufs.o

ccflags-y	:= -Idrivers/crypto/fmp
