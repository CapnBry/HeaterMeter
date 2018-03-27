include $(TOPDIR)/rules.mk

PKG_NAME:=firmware-brcmfmac43430
PKG_VERSION:=1
PKG_MAINTAINER:=Bryan Mayland <capnbry@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/firmware-brcmfmac43430
	SECTION:=firmware
	CATEGORY:=Firmware
	TITLE:=Out of tree updates for firmware-brcmfmac43430
endef

define Package/firmware-brcmfmac43430/install
	$(INSTALL_DIR) $(1)/lib/firmware/brcm
	$(INSTALL_DATA) ./root/brcm/brcmfmac43430-sdio.bin $(1)/lib/firmware/brcm/
	$(INSTALL_DATA) ./root/brcm/brcmfmac43430-sdio.txt $(1)/lib/firmware/brcm/
endef

define Build/Compile
endef

$(eval $(call BuildPackage,firmware-brcmfmac43430))
