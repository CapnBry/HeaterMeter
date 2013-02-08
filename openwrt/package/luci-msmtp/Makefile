include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-msmtp
PKG_VERSION:=1.4.27
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
PKG_INSTALL:=1

define Package/luci-app-msmtp
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  TITLE:=MSMTP email client configuration
  DEPENDS:=luci-lib-web msmtp
endef

define Package/luci-app-msmtp/description
  Simple editor for the msmtp SMTP client's configuration file
  /etc/msmtprc. Published to LuCI under Services -> SMTP
endef

define Build/Compile
endef

define Build/Install
endef

define Package/luci-app-msmtp/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci
	$(CP) ./luasrc/* $(1)/usr/lib/lua/luci
endef

$(eval $(call BuildPackage,luci-app-msmtp))
