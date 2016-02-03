#!/system/bin/sh

sleep 2

#insmod /data/temp/mxt.ko

cd /sys/bus/i2c/devices/10-004a

#chown root root *
#chmod 666 t19
#chmod 666 update_cfg
#chmod 666 update_fw

#format: [family id]_[variant id]_[version]_[build].fw
#format: [xxx].raw

#you could write a default zero config before update firmware
#e.g: for 540s from 3.0_AA to 5.0AA
echo "82_2C_3.0_AA.RAW" > update_cfg
#echo "82_39_5.0_AA.fw" > update_fw

#use GPIO 19 for diffferent hw identify
#format: [xxx].raw.[hex_i2c_address].[hex_gpio_t19].cfg
#echo "1" > t19

#update new config
#echo "82_39_5.0_AA.raw" > update_cfg
#for gpio 19 alternative(i2c address 0x4b, gpio=01)
#echo "82_39_5.0_AA.raw.4B.01.cfg" > update_cfg

sleep 1

#send self tune command for 540s
#0 : no backup
#1 : backup
echo 0 > self_tune
#dmesg > /cache/atmel_ts.log

sleep 1

#enable plugin
#
#format: pl enable [hex]
#[0] : CAL
#[1] : AC
#[2] : PI
#[3] : PLUG PAUSE
echo "pl enable 3" > plugin

#set gesture list
#format: <name> <val>;<name> <val>;...
#you could run command "cat gesture_list" for current config list
#<val>: bit[0]: enable
#	bit[1]: disable, 
#	bit[3]  status (1: excuted)
#echo "LEFT 1;e 1;" > gesture_list

#enable gesture feature
#echo 1 > en_gesture

#chmod 440 t19
#chmod 440 update_cfg
#chmod 440 update_fw

