#!/bin/sh

REPO_PATH=$(pwd)
WRT_PATH="~/openwrt"

cat << EOFEEDS > $WRT_PATH/feeds.conf
src-svn packages svn://svn.openwrt.org/openwrt/packages
src-svn luci http://svn.luci.subsignal.org/luci/trunk/contrib/package
src-link linkmeter $REPO_PATH/package
EOFEEDS

$WRT_PATH/scripts/feeds update

$WRT_PATH/scripts/feeds install libdaemon
$WRT_PATH/scripts/feeds install -p linkmeter avrdude
$WRT_PATH/scripts/feeds install -p linkmeter rrdtool
$WRT_PATH/scripts/feeds install -p linkmeter linkmeter

cp .config $WRT_PATH
