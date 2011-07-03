#!/bin/sh

REPO_PATH=$(pwd)
WRT_PATH="$1"

if [ -z "$WRT_PATH" ] ; then
  echo "Usage: ./install.sh <wrt path>"
  exit 1
fi

cat << EOFEEDS > $WRT_PATH/feeds.conf
src-svn packages svn://svn.openwrt.org/openwrt/packages
src-svn luci http://svn.luci.subsignal.org/luci/trunk/contrib/package
src-link linkmeter $REPO_PATH/package
EOFEEDS

$WRT_PATH/scripts/feeds update

$WRT_PATH/scripts/feeds install libdaemon
$WRT_PATH/scripts/feeds install -p linkmeter avrdude
$WRT_PATH/scripts/feeds install -p linkmeter rrdtool
$WRT_PATH/scripts/feeds install -p linkmeter kmod-broadcom-sdhc26
$WRT_PATH/scripts/feeds install -p linkmeter linkmeter

cp .config $WRT_PATH

patch -p0 -d $WRT_PATH/package < patches/100-dhcp_add_hostname.patch
