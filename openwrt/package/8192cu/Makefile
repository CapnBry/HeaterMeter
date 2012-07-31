#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=8192cu
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/8192cu
  SUBMENU:=Wireless Drivers
  TITLE:=Realtek 8192cu WiFi
  FILES:=$(PKG_BUILD_DIR)/8192cu.ko
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  DEPENDS:=+kmod-cfg80211 +@DRIVER_11N_SUPPORT
  AUTOLOAD:=$(call AutoLoad,50,8192cu)
endef

define KernelPackage/8192cu/description
  Updated blob driver for Realtek 8192cu wireless devices.
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

$(eval $(call KernelPackage,8192cu))
