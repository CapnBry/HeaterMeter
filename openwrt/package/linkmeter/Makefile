# Copyright 2011 Bryan Mayland <bmayland@capnbry.net>

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_RELEASE:=9

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+rrdtool +luci-lib-lucid-http +libuci
	TITLE:=LinkMeter BBQ Controller
	URL:=http://github.com/CapnBry/HeaterMeter
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/linkmeter/description
	Arduino (AVR) based automatic temperature controller for BBQ
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) src/* $(PKG_BUILD_DIR)/
	$(CP) luasrc/* $(PKG_BUILD_DIR)/
endef

#define Build/Compile
	# Nothing to compile
#endef

define Package/linkmeter/install
	$(CP) ./root/* $(1)/
	if [ "$(CONFIG_TARGET_brcm2708)" = "y" ] ; then \
	  $(CP) ./targets/bcm2708/* $(1)/ ; \
	fi

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./config/linkmeter.uci-defaults $(1)/etc/uci-defaults/linkmeter

	$(INSTALL_DIR) $(1)/www
	$(CP) htdocs/* $(1)/www/

	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lmclient.lua $(1)/usr/lib/lua/

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hmdude $(1)/usr/bin
	$(LN) -sf ../../usr/lib/lua/lmclient.lua $(1)/usr/bin/lmclient

	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/lucid
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lucid/linkmeterd.lua $(1)/usr/lib/lua/luci/lucid/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/controller/linkmeter/* $(1)/usr/lib/lua/luci/controller/linkmeter/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/view/linkmeter/* $(1)/usr/lib/lua/luci/view/linkmeter/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/model/cbi/linkmeter/* $(1)/usr/lib/lua/luci/model/cbi/linkmeter/
endef

define Package/linkmeter/postinst
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit
	/etc/uci-defaults/linkmeter update
	rm /etc/uci-defaults/linkmeter
	rm -f /tmp/luci-indexcache
	rm -f /tmp/luci-modulecache/*
	/etc/init.d/lucid restart
endef


define Package/linkmeter/postrm
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit

	/etc/init.d/lucid stop
	[ -f "/tmp/hm.rrd" ] && rm "/tmp/hm.rrd"
	uci del lucid.lmserver
	uci commit lucid
	rm -f /tmp/luci-indexcache
	rm -f /tmp/luci-modulecache/*
	/etc/init.d/lucid start
endef

$(eval $(call BuildPackage,linkmeter))
