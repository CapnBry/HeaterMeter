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
src-svn packages svn://svn.openwrt.org/openwrt/branches/packages_12.09
src-git luci http://github.com/openwrt/luci.git;0.11.1
src-link linkmeter $REPO_PATH/package
EOFEEDS
fi

$WRT_PATH/scripts/feeds update

LUCIP=$WRT_PATH/feeds/luci/patches
rm -fR $LUCIP
mkdir $LUCIP
cp patches/200-luci-inreq-fix.patch $LUCIP
cp patches/215-luci-adminfull-inreq.patch $LUCIP
cp patches/217-luci-login-urltok.patch $LUCIP
cp patches/218-lucid-cacheloader.patch $LUCIP
cp patches/219-luci-sysupgrade-url.patch $LUCIP
cp patches/226-lucid-no-redirect.patch $LUCIP
cp patches/227-lucid-keepalive-fix.patch $LUCIP
cp patches/228-luci-no-dhcp-dhcpc.patch $LUCIP

LMPACKS="rrdtool kmod-broadcom-sdhc26 linkmeter kmod-8192cu kmod-spi-bcm2708 luci-app-msmtp parted avahi-daemon kmod-incompat-wireless ca-certificates"
for PACK in $LMPACKS ; do
  $WRT_PATH/scripts/feeds install -p linkmeter $PACK
done

cp config.$TARGET $WRT_PATH/.config

if [ "$TARGET" = "BCM47XX" ] ; then
  patch -N -p0 -d $WRT_PATH/package < patches/100-dhcp_add_hostname.patch
fi

if [ "$TARGET" = "BCM2708" ] ; then
  patch -N -p0 -d $WRT_PATH < patches/0700-bcm2708-tweaks.patch
  patch -N -p0 -d $WRT_PATH < patches/0710-brcm2708-firmware.patch
  patch -N -p0 -d $WRT_PATH < patches/110-dnsmasq_add_hostname.patch
  patch -N -p0 -d $WRT_PATH < patches/220-iwinfo-nl80211-over-wext.patch
  patch -N -p0 -d $WRT_PATH < patches/225-iwinfo-scan-wo-vintf.patch
  patch -N -p0 -d $WRT_PATH < patches/229-netifd-add-hostname.patch
  patch -N -p0 -d $WRT_PATH < patches/230-hotplug2-source.patch
  cp patches/06??-rpi-patches-*.patch \
    $WRT_PATH/target/linux/brcm2708/patches-3.3/.
fi

