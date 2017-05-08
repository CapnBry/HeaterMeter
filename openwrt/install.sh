#!/bin/sh

REPO_PATH=$(pwd)
TARGET="$1"
WRT_PATH="$2"

if [ -z "$WRT_PATH" ] ; then
  echo "Usage: ./install.sh <target> <wrt path>"
  echo "    Target: BCM47XX or BCM2708 (case sensitive)"
  echo "BCM47XX - svn://svn.openwrt.org/openwrt/trunk@29665"
  echo "BCM2708 (tag/v17.01.1) - https://github.com/lede-project/source.git@29fabe26399fbaecf9231e24f9ac1ee5773cafa6"
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
src-git packages https://git.lede-project.org/feed/packages.git^f9e99848182fc7bc554e541ca133c22079d4041b
src-git luci https://git.lede-project.org/project/luci.git^29fabe26399fbaecf9231e24f9ac1ee5773cafa6
#src-git routing https://git.lede-project.org/feed/routing.git^04a37ef4309c2b67c64901eb8fbf3800b4c7bb35
#src-git telephony https://git.lede-project.org/feed/telephony.git^1f0fb2538ba6fc306198fe2a9a4b976d63adb304
src-link linkmeter $REPO_PATH/package
EOFEEDS
fi

$WRT_PATH/scripts/feeds update

LUCIP=$WRT_PATH/feeds/luci
for X in patches/2??-luci-* ; do
  patch -N -p1 -d $LUCIP < $X
done

# BRY: Want luci-ssl-openssl, but that adds luci -> luci-app-firewall -> iptables -> kmods-*
# Think we can get away with linkmeter -> luci-mod-admin-full and then just enabling uhttpd / openssl-util
LMPACKS="parted rrdtool linkmeter liblmfit-lua luci-theme-material luci-theme-openwrt luci-app-msmtp"
for PACK in $LMPACKS ; do
  $WRT_PATH/scripts/feeds install -p linkmeter $PACK
done


if [ "$TARGET" = "BCM47XX" ] ; then
  cp config.$TARGET $WRT_PATH/.config
  LMPACKS="kmod-broadcom-sdhc26"
  for PACK in $LMPACKS ; do
    $WRT_PATH/scripts/feeds install -p linkmeter $PACK
  done
  patch -N -p0 -d $WRT_PATH/package < patches/100-dhcp_add_hostname.patch
fi

if [ "$TARGET" = "BCM2708" ] ; then
  cp diffconfig.$TARGET $WRT_PATH/.config
  patch -N -p0 -d $WRT_PATH < patches/0700-bcm2708-tweaks.patch
  patch -N -p0 -d $WRT_PATH < patches/229-netifd-add-hostname.patch
fi

