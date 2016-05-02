# Turbo
This tree is used to build boot.img which include ubuntu kernel and initrd for Turbo (Pro5 Ubuntu Edition)

Environment
-----------------------------
$ sudo apt-get install make lzop abootimg wget libc6-i386 gcc git ccache bc

Build Steps for boot.img
-----------------------------
$ make -f ubuntu.mk bootimage

Note
-----------------------------
 * The mkbootimg and cross compiler in prebuilt folder came from AOSP Android-5.1.0_r1
 * The Ubuntu ramdisk is commit 9a919 of https://code-review.phablet.ubuntu.com/p/ubuntu/initrd/ubuntu_prebuilt_initrd_debs
 * flash image by "fastboot flash bootimg boot.img"
