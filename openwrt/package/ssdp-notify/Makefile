include $(TOPDIR)/rules.mk

PKG_NAME:=ssdp-notify
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/ssdp-notify
	SECTION:=net
	CATEGORY:=Network
	TITLE:=SSDP notify service
	URL:=https://github.com/zlargon/lssdp
	MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
	DEPENDS:=+uuidgen
endef

define Package/ssdp-notify/description
	A simple SSDP notify daemon based on Leon Huang's light weight SSDP library
endef

define Package/ssdp-notify/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ssdp-notify $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ssdp_notify.init $(1)/etc/init.d/ssdp_notify

	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(CP) ./luasrc/* $(1)/usr/lib/lua/
endef

$(eval $(call BuildPackage,ssdp-notify))
