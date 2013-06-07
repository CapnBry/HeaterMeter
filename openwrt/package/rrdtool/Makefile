#
# Copyright (C) 2007-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=rrdtool
PKG_VERSION:=1.4.5
PKG_RELEASE:=LM3

PKG_SOURCE_URL:=http://oss.oetiker.ch/rrdtool/pub/
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_MD5SUM:=4d116dba9a0888d8aaac179e35d3980a

PKG_FIXUP:=libtool
PKG_BUILD_PARALLEL:=1
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/rrdtool/Default
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Round Robin Database (RRD) NOGRAPH
  URL:=http://oss.oetiker.ch/rrdtool/
endef

define Package/librrd
$(call Package/rrdtool/Default)
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE+= library (v1.4.x)
  DEPENDS+= +liblua
endef

define Package/rrdtool
$(call Package/rrdtool/Default)
  TITLE+= tools (v1.4.x)
  DEPENDS+= +librrd
endef

CONFIGURE_ARGS += \
	--enable-shared=yes \
	--enable-static=yes \
	--disable-rpath \
	--disable-pthread \
	--with-gnu-ld \
	--disable-mmap \
	--disable-perl \
	--disable-tcl \
	--disable-ruby \
	--disable-python \
	--disable-rrdcgi \
	--disable-rrd_graph \
	--disable-rrd_restore \
	--disable-rrd_cached \
	--enable-lua 

CONFIGURE_VARS += \
	rd_cv_ieee_works=yes \
	rd_cv_null_realloc=nope \
	ac_cv_func_mmap_fixed_mapped=yes \

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/rrdtool-1.4/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/rrd.h $(1)/usr/lib/rrdtool-1.4/include/
	$(INSTALL_DIR) $(1)/usr/lib/rrdtool-1.4/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/librrd.{a,so*} $(1)/usr/lib/rrdtool-1.4/lib/
endef

define Package/librrd/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) -L $(PKG_INSTALL_DIR)/usr/lib/librrd.so.4 $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(CP) -L $(PKG_INSTALL_DIR)/usr/lib/lua/5.1/rrd.so $(1)/usr/lib/lua/
endef

define Package/rrdtool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/rrdtool $(1)/usr/bin/
	# $(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/rrdupdate $(1)/usr/bin/
endef

$(eval $(call BuildPackage,librrd))
$(eval $(call BuildPackage,rrdtool))

