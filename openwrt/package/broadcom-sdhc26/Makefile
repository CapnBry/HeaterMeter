#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=broadcom-sdhc26
PKG_RELEASE:=2

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/broadcom-sdhc26
  SUBMENU:=Block Devices
  TITLE:=Broadcom 47xx optimized MMHC/SDHC driver
  FILES:=$(PKG_BUILD_DIR)/bcm_sdhc.ko
  KCONFIG:=CONFIG_BROADCOM_SDHC26=y
endef
#  AUTOLOAD:=$(call AutoLoad,93,bcm_sdhc)

define KernelPackage/broadcom-sdhc26/description
  Driver for Linksys WRT54G/GS/GL MM/SD card modification.
  Supports standard and high capacity cards.
  May work for other Broadcom based routers as well (Buffalo, etc).
endef

define Package/kmod-broadcom-sdhc26/config
  menu "Default Configuration"
    depends PACKAGE_kmod-broadcom-sdhc26

  config KMOD_BROADCOM_SDHC26_DI_PIN
    int "GPIO DI (Data-In) pin"
    default 2

  config KMOD_BROADCOM_SDHC26_DO_PIN
    int "GPIO DO (Data-Out) pin"
    default 4

  config KMOD_BROADCOM_SDHC26_CLK_PIN
    int "GPIO CLK (Clock) pin"
    default 3

  config KMOD_BROADCOM_SDHC26_CS_PIN
    int "GPIO CS (Chip-Select) pin"
    default 7
  endmenu
endef

define KernelPackage/broadcom-sdhc26/conffiles
  /etc/config/broadcom_sdhc26
endef

EXTRA_KCONFIG:= \
	CONFIG_BROADCOM_SDHC26=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/broadcom-sdhc26/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/broadcom_sdhc26.config $(1)/etc/config/broadcom_sdhc26
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/broadcom_sdhc26.init $(1)/etc/init.d/broadcom_sdhc26
	$(INSTALL_DIR) $(1)/lib/functions
	$(INSTALL_DATA) ./files/broadcom_sdhc26.sh $(1)/lib/functions/broadcom_sdhc26.sh
	$(INSTALL_DIR) $(1)/lib/preinit
	$(INSTALL_DATA) ./files/45_init_broadcom_sdhc26 $(1)/lib/preinit/45_init_broadcom_sdhc26

	$(SED) 's,@GPIO_DI_PIN@,$(CONFIG_KMOD_BROADCOM_SDHC26_DI_PIN),g' \
		-e 's,@GPIO_DO_PIN@,$(CONFIG_KMOD_BROADCOM_SDHC26_DO_PIN),g' \
		-e 's,@GPIO_CLK_PIN@,$(CONFIG_KMOD_BROADCOM_SDHC26_CLK_PIN),g' \
		-e 's,@GPIO_CS_PIN@,$(CONFIG_KMOD_BROADCOM_SDHC26_CS_PIN),g' \
		$(1)/etc/config/broadcom_sdhc26
endef

$(eval $(call KernelPackage,broadcom-sdhc26))

