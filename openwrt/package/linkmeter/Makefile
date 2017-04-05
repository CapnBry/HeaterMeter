# Copyright 2016 Bryan Mayland <bmayland@capnbry.net>

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_RELEASE:=14

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+rrdtool +liblmfit-lua +libuci +luci-base +luci-lib-nixio +luci-lib-jsonc
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

	mkdir -p $(PKG_BUILD_DIR)/jssrc
	$(CP) jssrc/* $(PKG_BUILD_DIR)/jssrc
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(FIND) $(PKG_BUILD_DIR)/jssrc/ -type f -name "1??-*" | \
		sort | $(XARGS) cat > $(PKG_BUILD_DIR)/jssrc/jquery-all.js
endef

define Package/linkmeter/install
	$(CP) ./root/* $(1)/
	if [ "$(CONFIG_TARGET_brcm2708)" = "y" ] ; then \
	  $(CP) ./targets/bcm2708/* $(1)/ ; \
	  $(LN) -sf wifi-client $(1)/usr/bin/wifi-ap ; \
	fi

	$(INSTALL_DIR) $(1)/etc/uci-defaults
        # Don't clobber files
	cp -n ./config/email.txt $(1)/usr/share/linkmeter/

	$(INSTALL_DIR) $(1)/www/luci-static/resouces/js
	$(CP) htdocs/* $(1)/www/
	$(CP) $(PKG_BUILD_DIR)/jssrc/jquery-all.js $(1)/www/luci-static/resources/js

	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(CP) ./luasrc/* $(1)/usr/lib/lua/

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hmdude $(1)/usr/bin
	$(LN) -sf ../../usr/lib/lua/lmclient.lua $(1)/usr/bin/lmclient
	# uci-defaults is saved so it can be used again after config-restore
	$(INSTALL_BIN) ./config/linkmeter.uci-defaults $(1)/usr/bin/
	$(LN) -sf ../../usr/bin/linkmeter.uci-defaults $(1)/etc/uci-defaults/90_linkmeter
endef

define Package/linkmeter/postinst
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit
	/etc/uci-defaults/linkmeter update
	rm /etc/uci-defaults/linkmeter
	rm -f /tmp/luci-indexcache
	rm -f /tmp/luci-modulecache/*
endef

define Package/linkmeter/postrm
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit
	[ -f "/tmp/hm.rrd" ] && rm "/tmp/hm.rrd"
	rm -f /tmp/luci-indexcache
	rm -f /tmp/luci-modulecache/*
endef

define Package/linkmeter/conffiles
/etc/config/linkmeter
endef

$(eval $(call BuildPackage,linkmeter))
