# Copyright (c) 2011 Bryan Mayland

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_RELEASE)

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+uhttpd +rrdtool +avrdude
        PKG_BUILD_DEPENDS:=+libdaemon
	TITLE:=LinkMeter BBQ Controller
	URL:=http://tvwbb.com/eve/forums/a/tpc/f/9270072103/m/7691098906
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/linkmeter/description
	Arduino (AVR) based automatic temperature controller for BBQ
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(CP) ./luasrc/* $(PKG_BUILD_DIR)/
endef

define Package/linkmeter/conffiles
/etc/config/linkmeter
endef

define Package/linkmeter/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./config/linkmeter.init $(1)/etc/init.d/linkmeter
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./config/linkmeter.uci $(1)/etc/config/linkmeter

	$(CP) ./root/* $(1)/

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/luad $(1)/usr/sbin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/linkmeterd.lua $(1)/usr/sbin/
	$(LN) -s /usr/sbin/luad $(1)/usr/sbin/linkmeterd

	$(LN) /tmp/json $(1)/www/json
endef

define Package/linkmeter/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
    /etc/init.d/linkmeter enabled || /etc/init.d/linkmeter enable
    /etc/init.d/linkmeter start
    exit 0
}
endef

define Package/linkmeter/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
    /etc/init.d/linkmeter stop
    /etc/init.d/linkmeter enabled && /etc/init.d/linkmeter disable
    if [ -f "/tmp/hm.rrd" ] ; then rm "/tmp/hm.rrd" ; fi
    if [ -f "/tmp/json" ] ; then rm "/tmp/json" ; fi
}
endef

$(eval $(call BuildPackage,linkmeter))
