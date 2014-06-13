include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=incompat-wireless
PKG_RELEASE:=1

# I need the firmware files from compat-wireless proper
COMPAT_FIRMWARE_DIR:=$(KERNEL_BUILD_DIR)/compat-wireless-2012-09-07/linux-firmware-375e9548bf75a5664256d47a2f9e14b79181e02d

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/incompat-wireless
  SUBMENU:=Wireless Drivers
  TITLE:=Incompat Wireless
  FILES:= \
    $(LINUX_DIR)/net/wireless/cfg80211.ko \
    $(LINUX_DIR)/net/mac80211/mac80211.ko
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  DEPENDS:=+@DRIVER_11N_SUPPORT
  KCONFIG:=CONFIG_CFG80211=m \
    CONFIG_NL80211_TESTMODE=n CONFIG_CFG80211_REG_DEBUG=n \
    CONFIG_CFG80211_DEVELOPER_WARNINGS=n \
    CONFIG_CFG80211_DEFAULT_PS=n \
    CONFIG_CFG80211_DEBUGFS=y CONFIG_CFG80211_INTERNAL_REGDB=n \
    CONFIG_CFG80211_WEXT=y \
    CONFIG_ATH_COMMON=n CONFIG_IWM=n CONFIG_MWIFIEX=n \
    CONFIG_MAC80211=m \
    CONFIG_MAC80211_RC_PID=n CONFIG_MAC80211_RC_MINSTREL=y \
    CONFIG_MAC80211_RC_MINSTREL_HT=y \
    CONFIG_MAC80211_MESH=n CONFIG_MAC80211_LEDS=n \
    CONFIG_MAC80211_DEBUGFS=n CONFIG_MAC80211_DEBUG_MENU=n \
    CONFIG_MAC80211_HWSIM=n \
    CONFIG_RTL8192CU=n CONFIG_WL1251=n \
    CONFIG_WL12XX_MENU=n
  AUTOLOAD:=$(call AutoLoad,20, cfg80211 mac80211)
endef

MAKE_OPTS:= \
        ARCH="$(LINUX_KARCH)" \
        CROSS_COMPILE="$(TARGET_CROSS)" \
        USER_EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN" \
        KSRC=$(LINUX_DIR)

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/incompat-wireless/description
  This package includes a non 'compat-wireless' version
  of cfg80211.ko and mac80211.ko (i.e. the stock kernel version)
  so do not build with the openwrt
  kmod-cfg80211 or kmod-mac80211 packages built into the image
  (they'll still have to be compiled as modules though).
  Enabling kmod-mac80211 as a module is required to
  enable the nl80211 support in all the wireless tools.
endef

define KernelPackage/incompat-wireless/install
	mkdir -p $(1)/lib/wifi
	# compat-wireless, I hate your bastard kernel module but I need your script
	$(INSTALL_DATA) $(TOPDIR)/package/mac80211/files/lib/wifi/mac80211.sh $(1)/lib/wifi/.
endef

WMENU:=Wireless Drivers

define KernelPackage/mac80211-incompat/Default
  SUBMENU:=$(WMENU)
  URL:=http://linuxwireless.org/
  MAINTAINER:=Bryan Mayland <bmaylad@capnbry.net>
  DEPENDS:=@(!(TARGET_avr32||TARGET_ps3||TARGET_pxcab)||BROKEN)
endef

define KernelPackage/rt2x00-incompat/Default
$(call KernelPackage/mac80211-incompat/Default)
  TITLE:=Incompat Ralink Drivers for RT2x00 cards
endef

define KernelPackage/rt2x00-lib-incompat
$(call KernelPackage/rt2x00-incompat/Default)
  DEPENDS+= @(PCI_SUPPORT||USB_SUPPORT||TARGET_ramips) +kmod-incompat-wireless +kmod-lib-crc-itu-t
  TITLE+= (LIB)
  FILES:=$(LINUX_DIR)/drivers/net/wireless/rt2x00/rt2x00lib.ko
  AUTOLOAD:=$(call AutoLoad,25,rt2x00lib)
  MENU:=1
  KCONFIG:=CONFIG_RT2X00=m  CONFIG_RT2X00_LIB=m \
    CONFIG_RT2X00_LIB_FIRMWARE=y CONFIG_RT2X00_LIB_CRYPTO=y \
    CONFIG_RT2X00_LIB_LEDS=y CONFIG_RT2X00_DEBUG=n \
    CONFIG_RT2500USB=n CONFIG_RT73USB=n
endef

define KernelPackage/rt2x00-usb-incompat
$(call KernelPackage/rt2x00-incompat/Default)
  DEPENDS+= @USB_SUPPORT +kmod-rt2x00-lib-incompat +kmod-usb-core
  TITLE+= (USB)
  FILES:=$(LINUX_DIR)/drivers/net/wireless/rt2x00/rt2x00usb.ko
  AUTOLOAD:=$(call AutoLoad,26,rt2x00usb)
  KCONFIG:=CONFIG_RT2X00_LIB_USB=m
endef

define KernelPackage/rt2800-lib-incompat
$(call KernelPackage/rt2x00-incompat/Default)
  DEPENDS+= @(PCI_SUPPORT||USB_SUPPORT||TARGET_ramips) +kmod-rt2x00-lib-incompat +TARGET_ramips:kmod-rt2x00-soc +@DRIVER_11N_SUPPORT
  TITLE+= (rt2800 LIB)
  FILES:=$(LINUX_DIR)/drivers/net/wireless/rt2x00/rt2800lib.ko
  AUTOLOAD:=$(call AutoLoad,27,rt2800lib)
  KCONFIG:=CONFIG_RT2800_LIB=m
endef

define KernelPackage/rt2800-usb-incompat
$(call KernelPackage/rt2x00-incompat/Default)
  DEPENDS+= @USB_SUPPORT +kmod-rt2x00-usb-incompat +kmod-rt2800-lib-incompat +kmod-lib-crc-ccitt
  TITLE+= (RT2870 USB)
  FILES:=$(LINUX_DIR)/drivers/net/wireless/rt2x00/rt2800usb.ko
  AUTOLOAD:=$(call AutoLoad,28,rt2800usb)
  KCONFIG:=CONFIG_RT2800USB=m CONFIG_RT2800USB_RT33XX=y \
    CONFIG_RT2800USB_RT35XX=y CONFIG_RT2800USB_RT53XX=y \
    CONFIG_RT2800USB_UNKNOWN=n
endef

define KernelPackage/rt2800-usb-incompat/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(COMPAT_FIRMWARE_DIR)/rt2870.bin $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,incompat-wireless))
$(eval $(call KernelPackage,rt2x00-lib-incompat))
$(eval $(call KernelPackage,rt2x00-usb-incompat))
$(eval $(call KernelPackage,rt2800-lib-incompat))
$(eval $(call KernelPackage,rt2800-usb-incompat))
