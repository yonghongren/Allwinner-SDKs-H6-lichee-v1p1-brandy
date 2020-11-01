#!/bin/bash
set -e

PLATFORM="sun8iw3p1"
MODE=""

localpath=$(cd $(dirname $0) && pwd)
argnum=$#

show_help()
{
	printf "\nbuild.sh - Top level build scritps\n"
	echo "Valid Options:"
	echo "  -h  Show help message"
	echo "  -p <platform> platform, e.g. sun5i, sun6i, sun8iw1p1, sun8iw3p1, sun9iw1p1"
	echo "  -m <mode> mode, e.g. ota_test"
	echo "  -t install gcc tools chain"
	printf "\n\n"
}

prepare_toolchain()
{
        local ARCH="arm";
        local GCC="";
        local GCC_PREFIX="";
        local toolchain_archive_aarch64="./toolchain/gcc-linaro-aarch64.tar.xz";
        local toolchain_archive_arm="./toolchain/gcc-linaro-arm.tar.xz";
        local tooldir_aarch64="./toolchain/gcc-aarch64";
        local tooldir_arm="./toolchain/gcc-arm";

        echo "Prepare toolchain ..."

        if [ ! -d "${tooldir_aarch64}" ]; then
                mkdir -p ${tooldir_aarch64} || exit 1
                tar --strip-components=1 -xf ${toolchain_archive_aarch64} -C ${tooldir_aarch64} || exit 1
        fi

        if [ ! -d "${tooldir_arm}" ]; then
                mkdir -p ${tooldir_arm} || exit 1
                tar --strip-components=1 -xf ${toolchain_archive_arm} -C ${tooldir_arm} || exit 1
        fi
}

select_uboot()
{
	if [ "x${PLATFORM}" = "xsun50iw1p1" ] || \
	[ "x${PLATFORM}" = "xsun50iw2p1" ] || \
	[ "x${PLATFORM}" = "xsun50iw6p1" ] || \
	[ "x${PLATFORM}" = "xsun50iw3p1" ] || \
	[ "x${PLATFORM}" = "xsun3iw1p1"  ] || \
	[ "x${PLATFORM}" = "xsun8iw12p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw10p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw11p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw12p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw6p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw15p1" ] || \
	[ "x${PLATFORM}" = "xsun8iw15p1_axp2231" ] || \
	[ "x${PLATFORM}" = "xsun8iw17p1" ];then
		echo "u-boot-2014.07"
	else
		echo "u-boot-2011.09"
	fi
}

get_platform()
{
	if [ $argnum -eq 0 ] && [ -f $localpath/../.buildconfig ]; then
		local LICHEE_CHIP=$(grep LICHEE_CHIP $localpath/../.buildconfig | sed 's/[^=]*= *//g')
		local LICHEE_BUSSINESS=$(grep LICHEE_BUSSINESS $localpath/../.buildconfig | sed 's/[^=]*= *//g')
		if [ -n "$LICHEE_CHIP" ]; then
			PLATFORM=$LICHEE_CHIP
			if [ -n "$LICHEE_BUSSINESS" ]; then
				local boardcfg=$localpath/$(select_uboot)/boards.cfg
				local parten=1
				[ $(select_uboot) == "u-boot-2014.07" ] && parten=7
				[ -n "$(sed -e '/^#/d' $boardcfg | awk '{print $'"$parten"'}' | \
					grep ${PLATFORM}_${LICHEE_BUSSINESS})" ] && \
				PLATFORM=${PLATFORM}_${LICHEE_BUSSINESS}
			fi
		fi
		echo "Get PLATFORM from lichee buildconfig: $PLATFORM"
	fi
}

build_uboot()
{
	get_platform

	cd $(select_uboot)

	make distclean
	if [ "x$MODE" = "xota_test" ] ; then
		export "SUNXI_MODE=ota_test"
	fi
	make ${PLATFORM}_config
	make -j16

	cd - 1>/dev/null
}

while getopts p:m:t OPTION
do
	case $OPTION in
	p)
		PLATFORM=$OPTARG
		;;
	m)
		MODE=$OPTARG
		;;
	t)
		prepare_toolchain
		exit
		;;
	*) show_help
		exit
		;;
esac
done


build_uboot




