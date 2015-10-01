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
PKG_RELEASE:=2
PKG_SOURCE_URL:=http://heatermeter.com/devel/depends/
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_MD5SUM:=f6a051d38f1941b784a497f5b09a53c1

PKG_BUILD_PARALLEL:=1

TAR_OPTIONS += --strip-components=1 -C $(PKG_BUILD_DIR)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/8192cu
  SUBMENU:=Wireless Drivers
  TITLE:=Realtek 8192cu WiFi
  FILES:= \
	$(PKG_BUILD_DIR)/8192cu.ko
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  DEPENDS:=+kmod-incompat-wireless
  KCONFIG:=
  AUTOLOAD:=$(call AutoLoad,50, 8192cu)
endef

define KernelPackage/8192cu/description
  Updated blob driver for Realtek 8192cu wireless devices.
  Driver requires incompat-wireless, the non 'compat-wireless'
  version of cfg80211/mac80211.
endef

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	USER_EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN" \
	KSRC=$(LINUX_DIR)

define Build/Compile
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) $(MAKE_OPTS)
endef

$(eval $(call KernelPackage,8192cu))

