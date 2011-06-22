#!/bin/sh

start_broadcom_sdhc26() {
	local preinit_run="$1"

	config_load "broadcom_sdhc26"

	local section="broadcom_sdhc26"
	
	config_get "din" "$section" "din"
	config_get "dout" "$section" "dout"
	config_get "clk" "$section" "clk"
	config_get "cs" "$section" "cs"

	config_get "maxsec" "$section" "maxsec"
	config_get "major" "$section" "major"

	config_get "gpio_input" "$section" "gpio_input"
	config_get "gpio_output" "$section" "gpio_output"
	config_get "gpio_enable" "$section" "gpio_enable"
	config_get "gpio_control" "$section" "gpio_control"

	config_get "dbg" "$section" "dbg"

	config_get_bool "preinit" "$section" "preinit" '1'
	config_get_bool "enabled" "$section" "enabled" '1'
	config_get_bool "mask_diag" "$section" "mask_diag" '1'

	if [ "$preinit_run" = "1" ] && [ "$preinit" -ne 1 ]; then
		return 0;
	fi

	if [ "$preinit_run" = "0" ] && [ "$preinit" -ne 0 ]; then
		return 0;
	fi
	
	mkdir -p /tmp/broadcom_sdhc26
	ln -sf /lib/modules/*/* /tmp/overlay/lib/modules/*/* /tmp/broadcom_sdhc26/

	if [ "$enabled" -gt 0 ]; then
		if [ -f /proc/diag/gpiomask ] && [ "$mask_diag" -gt 0 ]; then
			mask=`printf '0x%x' $((2**$din + 2**$dout + 2**$clk + 2**$cs))`
			echo "$mask" > /proc/diag/gpiomask
		fi
		[ ! -z "$din" ] && DIN="din=$din"
		[ ! -z "$dout" ] && DOUT="dout=$dout"
		[ ! -z "$clk" ] && CLK="clk=$clk"
		[ ! -z "$cs" ] && CS="cs=$cs"

		[ ! -z "$maxsec" ] && MAXSEC="maxsec=$maxsec"
		[ ! -z "$major" ] && MAJOR="major=$major"

		[ ! -z "$gpio_input" ] && GPIOINPUT="gpio_input=$gpio_input"
		[ ! -z "$gpio_output" ] && GPIOOUTPUT="gpio_output=$gpio_output"
		[ ! -z "$gpio_enable" ] && GPIOENABLE="gpio_enable=$gpio_enable"
		[ ! -z "$gpio_control" ] && GPIOCONTROL="gpio_control=$gpio_control"

		[ ! -z "$dbg" ] && DBG="dbg=$dbg"

		insmod /tmp/broadcom_sdhc26/bcm_sdhc.ko $DIN $DOUT $CLK $CS $MAXSEC $MAJOR $GPIOINPUT $GPIOOUTPUT $GPIOENABLE $GPIOCONTROL $DBG
		if [ $? -ne 0 ]; then
			echo "Start failed."
			return $?
		fi
	fi
	
	rm -rf /tmp/broadcom_sdhc26
}
