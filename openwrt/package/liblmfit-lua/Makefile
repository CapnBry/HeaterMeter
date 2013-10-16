#
# Copyright (C) 2007-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=liblmfit-lua
PKG_VERSION:=2.4
PKG_RELEASE:=1

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/liblmfit-lua
  CATEGORY:=Libraries
  TITLE:=Levenberg-Marquardt Curve Fitting
  URL:=http://luaforge.net/projects/liblmfit-lua/
  DEPENDS:=+liblua
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/liblmfit-lua/description
This is a lua module for doing Levenberg-Marquardt Curve Fitting
using the lmfit C library (included). A function for performing
Steinhart-Hart coeficient calculation is included.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) src/* $(PKG_BUILD_DIR)/
endef

define Package/liblmfit-lua/install
	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lmfit.so $(1)/usr/lib/lua
endef

$(eval $(call BuildPackage,liblmfit-lua))

