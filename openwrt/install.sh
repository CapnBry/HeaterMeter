#!/bin/sh

REPO_PATH=$(pwd)
TARGET="$1"
WRT_PATH="$2"

if [ -z "$WRT_PATH" ] ; then
  echo "Usage: ./install.sh <target> <wrt path>"
  echo "    Target: BCM47XX or BCM2708 (case sensitive)"
  echo "BCM47XX - svn://svn.openwrt.org/openwrt/trunk@29665"
  echo "BCM2708 - svn://svn.openwrt.org/openwrt/tags/attitude_adjustment_12.09@36422"
  exit 1
fi

if [ "$TARGET" = "BCM47XX" ] ; then
  cat << EOFEEDS > $WRT_PATH/feeds.conf
src-svn packages svn://svn.openwrt.org/openwrt/packages@29665
src-svn luci http://svn.luci.subsignal.org/luci/trunk/contrib/package@8686
src-link linkmeter $REPO_PATH/package
EOFEEDS
fi

if [ "$TARGET" = "BCM2708" ] ; then
  cat << EOFEEDS > $WRT_PATH/feeds.conf
src-git packages git://git.openwrt.org/12.09/packages.git
src-git luci http://github.com/openwrt/luci.git;0.11.1
src-link linkmeter $REPO_PATH/package
EOFEEDS
fi

$WRT_PATH/scripts/feeds update

LUCIP=$WRT_PATH/feeds/luci
for X in patches/2??-luci-* ; do
  patch -N -p1 -d $LUCIP < $X
done

LMPACKS="luci luci-theme-material luci-theme-openwrt parted rrdtool linkmeter liblmfit-lua luci-app-msmtp"
for PACK in $LMPACKS ; do
  $WRT_PATH/scripts/feeds install -p linkmeter $PACK
done

cp config.$TARGET $WRT_PATH/.config

if [ "$TARGET" = "BCM47XX" ] ; then
  LMPACKS="kmod-broadcom-sdhc26"
  for PACK in $LMPACKS ; do
    $WRT_PATH/scripts/feeds install -p linkmeter $PACK
  done
  patch -N -p0 -d $WRT_PATH/package < patches/100-dhcp_add_hostname.patch
fi

if [ "$TARGET" = "BCM2708" ] ; then
  patch -N -p0 -d $WRT_PATH < patches/0700-bcm2708-tweaks.patch
  patch -N -p0 -d $WRT_PATH < patches/229-netifd-add-hostname.patch
fi

