#!/usr/bin/env bash

export PROJ=$HOME/work/minos-qemu

export MINOS_SRC=$PROJ/minos
export UBOOT_SRC=$PROJ/u-boot

# Existing kernel folder.
# Check whether this is really Raspberry Pi kernel source.
export RPI_KERNEL_SRC=$PROJ/linux-4.19.238

# If linux-4.19.238 does not contain bcm2711_defconfig,
# use this alternative Raspberry Pi kernel source later.
export RPI_KERNEL_ALT=$PROJ/linux-rpi-4.19.238

export MINOS_MISC=$PROJ/minos-misc

export SDBOOT=$PROJ/sdboot
export SDROOT=$PROJ/sdroot

echo "PROJ=$PROJ"
echo "MINOS_SRC=$MINOS_SRC"
echo "UBOOT_SRC=$UBOOT_SRC"
echo "RPI_KERNEL_SRC=$RPI_KERNEL_SRC"
echo "RPI_KERNEL_ALT=$RPI_KERNEL_ALT"
echo "MINOS_MISC=$MINOS_MISC"
echo "SDBOOT=$SDBOOT"
echo "SDROOT=$SDROOT"
