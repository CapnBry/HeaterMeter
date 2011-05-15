# Copyright (c) 2011 Bryan Mayland

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+uhttpd +rrdtool1 +avrdude
	TITLE:=LinkMeter BBQ Controller
	URL:=http://tvwbb.com/eve/forums/a/tpc/f/9270072103/m/7691098906
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/linkmeter/description
	Arduino (AVR) based automatic temperature controller for BBQ
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./root/* $(PKG_BUILD_DIR)/
	$(CP) ./luasrc/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
# nothing to compile
endef

define Package/linkmeter/install
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/etc/heatermeter.conf $(1)/etc/

	$(INSTALL_DIR) $(1)/www
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www/*.html $(1)/www/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www/*.png $(1)/www/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www/*.gif $(1)/www/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www/*.ico $(1)/www/
	$(INSTALL_DIR) $(1)/www/js
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www/js/* $(1)/www/js
	$(INSTALL_DIR) $(1)/www/cgi-bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/www/cgi-bin/* $(1)/www/cgi-bin

	$(INSTALL_DIR) $(1)/usr/lib/lua/linkmeter
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/rrdpipe.lua $(1)/usr/lib/lua/linkmeter/

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/linkmeterd.lua $(1)/usr/sbin/

	$(LN) /tmp/json $(1)/www/json
endef

$(eval $(call BuildPackage,linkmeter))
