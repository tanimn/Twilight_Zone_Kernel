#!/bin/sh

# Defaults
BUILD_KERNEL=y
CLEAN=n
CROSS_COMPILE="/home/tanimn/x-tools/arm-2009q3/bin/arm-none-linux-gnueabi-"
# CROSS_COMPILE="~/x-tools/arm-2010.09/bin/arm-none-eabi-"
# CROSS_COMPILE="~/x-tools/arm-cortex_a8-linux-gnueabi/bin/arm-cortex_a8-linux-gnueabi-"
# CROSS_COMPILE="~/x-tools/arm-unknown-linux-gnueabi/bin/arm-unknown-linux-gnueabi-"
DEFCONFIG=y
MKZIP='7z -mx9 -mmt=1 a "$OUTFILE" .'
PRODUCE_TAR=n
PRODUCE_ZIP=n
TARGET="Twilight_Zone_01"
THREADS=4
VERSION=$(date +%Y%m%d%H%M)
TRACK=y
TRACK_OUT="$PWD/../../../out"
TRACK_STORE="$PWD/../../../../archives/builds"

SHOW_HELP()
{
	echo
	echo "Usage options for build_kernel.sh:"
	echo "-c : Run 'make clean'"
	echo "-d : Use specified config."
	echo "     For example, use -d myconfig to 'make myconfig_defconfig'"
	echo "-h : Print this help."
	echo "-j : Use a specified number of threads to build."
	echo "     For example, use -j4 to make with 4 threads."
	echo "-t : Produce tar file suitable for flashing with Odin."
	echo "-z : Produce zip file suitable for flashing via Recovery."
	echo
	exit 1
}

# Get values from Args
set -- $(getopt cd:hj:tz "$@")
while [ $# -gt 0 ]
do
	case "$1" in
	(-c) CLEAN=y;;
	(-d) DEFCONFIG=y; TARGET="$2"; shift;;
	(-h) SHOW_HELP;;
	(-j) THREADS=$2; shift;;
	(-t) PRODUCE_TAR=y;;
	(-z) PRODUCE_ZIP=y;;
	(--) shift; break;;
	(-*) echo "$0: error - unrecognized option $1" 1>&2; exit 1;;
	(*) break;;
	esac
	shift
done

echo "make clean    == "$CLEAN
echo "use defconfig == "$DEFCONFIG
echo "build target  == "$TARGET
echo "make threads  == "$THREADS
echo "build kernel  == "$BUILD_KERNEL
echo "create tar    == "$PRODUCE_TAR
echo "create zip    == "$PRODUCE_ZIP
echo "track builds  == "$TRACK

if [ "$CLEAN" = "y" ] ; then
	echo "Cleaning source directory." && echo ""
	make -j"$THREADS" ARCH=arm clean
fi

if [ "$DEFCONFIG" = "y" -o ! -f ".config" ] ; then
	echo "Using default configuration for $TARGET" && echo ""
	make -j"$THREADS" ARCH=arm ${TARGET}_defconfig
fi

if [ "$BUILD_KERNEL" = "y" ] ; then
	T1=$(date +%s)
	echo "Beginning zImage compilation..." && echo ""
	make -j"$THREADS" ARCH=arm CROSS_COMPILE="$CROSS_COMPILE"
	T2=$(date +%s)
	echo "" && echo "Compilation took $(($T2 - $T1)) seconds." && echo ""
fi

if [ "$PRODUCE_TAR" = y ] ; then
	echo "Generating $TARGET-$VERSION.tar for flashing with Odin" && echo ""
	tar c -C arch/arm/boot zImage >"$TARGET-$VERSION.tar"
fi

if [ "$PRODUCE_ZIP" = y ] ; then
	echo "Generating $TARGET-$VERSION.zip for flashing as update.zip" && echo ""
	rm -fr "$TARGET-$VERSION.zip"
	rm -f update/kernel_update/zImage
	cp arch/arm/boot/zImage update/kernel_update
	OUTFILE="$PWD/$TARGET-$VERSION.zip"
	cd update
	eval "$MKZIP" >/dev/null 2>&1
	cd ..
	mv $"OUTFILE"-signed "$OUTFILE"
fi

if [ "$TRACK" = "y" ] ; then
	echo "copying zImage to ${TRACK_OUT}"
	cp arch/arm/boot/zImage ${TRACK_OUT}
	echo "storing zImage as ${TRACK_STORE}/zImage.${TARGET}.${VERSION}"
	cp arch/arm/boot/zImage ${TRACK_STORE}/zImage.${TARGET}.${VERSION}
fi
