# Copyright 2011 Bryan Mayland <bmayland@capnbry.net>

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+rrdtool +luci-lib-lucid-http +avrdude
	TITLE:=LinkMeter BBQ Controller
	URL:=http://tvwbb.com/eve/forums/a/tpc/f/9270072103/m/7691098906
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/linkmeter/description
	Arduino (AVR) based automatic temperature controller for BBQ
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./luasrc/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	# Nothing to compile
endef

define Package/linkmeter/conffiles
/etc/config/linkmeter
endef

define Package/linkmeter/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./config/linkmeter.uci $(1)/etc/config/linkmeter

	$(CP) ./root/* $(1)/

	$(INSTALL_DIR) $(1)/www
	$(CP) ./htdocs/* $(1)/www/

	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lmclient.lua $(1)/usr/lib/lua/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/lucid
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lucid/linkmeterd.lua $(1)/usr/lib/lua/luci/lucid/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/controller/linkmeter/* $(1)/usr/lib/lua/luci/controller/linkmeter/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/view/linkmeter/* $(1)/usr/lib/lua/luci/view/linkmeter/
endef

define Package/linkmeter/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
    rm -f /tmp/luci-indexcache
    /etc/init.d/lucid restart
    exit 0
}
endef

define Package/linkmeter/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
    /etc/init.d/lucid stop
    if [ -f "/tmp/hm.rrd" ] ; then rm "/tmp/hm.rrd" ; fi
    rm -f /tmp/luci-indexcache
}
endef

define Package/linkmeter/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
    /etc/init.d/lucid start
}
endef

$(eval $(call BuildPackage,linkmeter))
