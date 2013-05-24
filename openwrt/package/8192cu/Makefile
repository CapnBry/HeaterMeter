#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=8192cu
PKG_VERSION:=3.4.3
PKG_RELEASE:=1

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/8192cu
  SUBMENU:=Wireless Drivers
  TITLE:=Realtek 8192cu WiFi
  FILES:= \
	$(LINUX_DIR)/net/wireless/cfg80211.ko \
	$(PKG_BUILD_DIR)/8192cu.ko
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  DEPENDS:=+@DRIVER_11N_SUPPORT
  KCONFIG:=CONFIG_CFG80211 \
	CONFIG_NL80211_TESTMODE=n CONFIG_CFG80211_REG_DEBUG=n \
	CONFIG_CFG80211_DEVELOPER_WARNINGS=n \
	CONFIG_CFG80211_DEFAULT_PS=y \
	CONFIG_CFG80211_DEBUGFS=n CONFIG_CFG80211_INTERNAL_REGDB=n \
	CONFIG_CFG80211_WEXT=y \
	CONFIG_ATH_COMMON=n CONFIG_IWM=n CONFIG_MWIFIEX=n
  AUTOLOAD:=$(call AutoLoad,50, cfg80211 8192cu)
endef

define KernelPackage/8192cu/description
  Updated blob driver for Realtek 8192cu wireless devices.
  This driver also includes a non 'compat-wireless' built version
  of cfg80211.ko so do not build with the kmod-cfg80211 or kmod-mac80211
  packages built into the image (they'll still have to be compiled as
  modules though). Enabling kmod-mac80211 as a module is required to
  enable the nl80211 support in all the wireless tools.
endef

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	USER_EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN" \
	KSRC=$(LINUX_DIR)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(Build/Patch)
endef

define Build/Compile
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) $(MAKE_OPTS)
endef

define KernelPackage/8192cu/install
	mkdir -p $(1)/lib/wifi
	# compat-wireless, I hate your bastard kernel module but I need your script
	$(INSTALL_DATA) $(TOPDIR)/package/mac80211/files/lib/wifi/mac80211.sh $(1)/lib/wifi/.
endef

$(eval $(call KernelPackage,8192cu))

