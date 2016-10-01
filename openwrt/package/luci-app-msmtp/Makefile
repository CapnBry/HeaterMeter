include $(TOPDIR)/rules.mk

PKG_VERSION:=1.6.2
PKG_RELEASE:=1
PKG_MAINTAINER:=Bryan Mayland <capnbry@gmail.com>

LUCI_TITLE:=MSMTP email client configuration
LUCI_DEPENDS:=+msmtp
LUCI_PKGARCH:=all
LUCI_DESCRIPTION:=Simple editor for the msmtp SMTP client's configuration file \
  /etc/msmtprc. Published to LuCI under Services -> SMTP

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
