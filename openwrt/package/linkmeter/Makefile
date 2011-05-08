# Copyright (c) 2011 Bryan Mayland

include $(TOPDIR)/rules.mk

PKG_NAME:=linkmeter
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/linkmeter
	SECTION:=utils
	CATEGORY:=utilities
	DEPENDS:=+uhttpd +rrdtool
	TITLE:=LinkMeter BBQ Controller
	URL:=http://tvwbb.com/eve/forums/a/tpc/f/9270072103/m/7691098906
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
endef

define Package/linkmeter/description
	Arduino (AVR) based automatic temperature controller for BBQ
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./files $(PKG_BUILD_DIR)/
endef

define Package/linkmeter/install
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/etc/heatermeter.conf

	$(INSTALL_DIR) $(1)/www
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/www
endef

$(eval $(call BuildPackage,linkmeter))
