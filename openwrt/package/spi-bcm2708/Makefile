#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=spi-bcm2708
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/spi-bcm2708
  SUBMENU:=SPI Support
  TITLE:=Broadcom 2708 SPI
  FILES:= $(LINUX_DIR)/drivers/spi/spi-bcm2708.ko
  MAINTAINER:=Bryan Mayland <capnbry@gmail.com>
  DEPENDS:=+kmod-spi-dev
  KCONFIG:=CONFIG_SPI_BCM2708
  #AUTOLOAD:=$(call AutoLoad,94,spi-bcm2708)
endef

define KernelPackage/spi-bcm2708/description
  Driver for user mode SPI for the bcm2708 controller (SPI0) used
  on the RaspberryPi.
endef

$(eval $(call KernelPackage,spi-bcm2708))

