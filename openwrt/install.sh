#!/bin/sh

REPO_PATH=$(pwd)
TARGET="$1"
WRT_PATH="$2"

if [ -z "$WRT_PATH" ] ; then
  echo "Usage: ./install.sh <target> <wrt path>"
  echo "    Target: BCM47XX or BCM2708 (case sensitive)"
  echo "BCM47XX - svn://svn.openwrt.org/openwrt/trunk@29665"
  echo "BCM2708 - git://github.com/openwrt/openwrt.git@2839ee70d38eeea18f3423806bfa2fad6c597c25"
  echo ""
  echo "Be sure both your WRT_PATH and WRT_PATH/feeds/luci are 'git reset --hard HEAD'"
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
src-git packages https://github.com/openwrt/packages.git
src-git luci https://github.com/openwrt/luci.git
#src-git routing https://github.com/openwrt-routing/packages.git
#src-git telephony https://github.com/openwrt/telephony.git
#src-git management https://github.com/openwrt-management/packages.git
#src-git targets https://github.com/openwrt/targets.git
#src-git oldpackages http://git.openwrt.org/packages.git
#src-link custom /usr/src/openwrt/custom-feed
src-link linkmeter $REPO_PATH/package
EOFEEDS
fi

$WRT_PATH/scripts/feeds update

LUCIP=$WRT_PATH/feeds/luci
for X in patches/2??-luci-* ; do
  patch -N -p1 -d $LUCIP < $X
done

LMPACKS="parted rrdtool linkmeter liblmfit-lua luci-theme-material luci-theme-openwrt luci-app-msmtp"
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

