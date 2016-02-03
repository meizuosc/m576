/*
 * mq100_sensorhub.h --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MQ100_SENSORHUB_H
#define _MQ100_SENSORHUB_H
#if defined(CONFIG_MQ100_DEMO)
#include <linux/input.h>
#include <linux/osp-sensors.h>
#include "spi-sensor-hub-priv.h"
#endif

enum {
	MQ100_MIC_CONFIG,
	MQ100_AEC_MODE,
	MQ100_VEQ_ENABLE,
	MQ100_DEREVERB_ENABLE,
	MQ100_DEREVERB_GAIN,
	MQ100_BWE_ENABLE,
	MQ100_BWE_HIGH_BAND_GAIN,
	MQ100_BWE_MAX_SNR,
	MQ100_BWE_POST_EQ_ENABLE,
	MQ100_SLIMBUS_LINK_MULTI_CHANNEL,
	MQ100_POWER_STATE,
	MQ100_FE_STREAMING,
	MQ100_PRESET,
	MQ100_ALGO_PROCESSING,
	MQ100_ALGO_SAMPLE_RATE,
	MQ100_CHANGE_STATUS,
	MQ100_MIX_SAMPLE_RATE,
	MQ100_FW_FIRST_CHAR,
	MQ100_FW_NEXT_CHAR,
	MQ100_EVENT_RESPONSE,
	MQ100_VOICE_SENSE_ENABLE,
	MQ100_VOICE_SENSE_SET_KEYWORD,
	MQ100_VOICE_SENSE_EVENT,
	MQ100_VOICE_SENSE_TRAINING_MODE,
	MQ100_VOICE_SENSE_DETECTION_SENSITIVITY,
	MQ100_VOICE_ACTIVITY_DETECTION_SENSITIVITY,
	MQ100_VOICE_SENSE_TRAINING_RECORD,
	MQ100_VOICE_SENSE_TRAINING_STATUS,
	MQ100_VOICE_SENSE_DEMO_ENABLE,
	MQ100_VS_STORED_KEYWORD,
	MQ100_VS_INT_OSC_MEASURE_START,
	MQ100_VS_INT_OSC_MEASURE_STATUS,
	MQ100_CVS_PRESET,
	MQ100_RX_ENABLE,
	MQ100_API_ADDR_MAX,
};

/*
 * Device parameter command codes
 */
#define MQ100_DEV_PARAM_OFFSET		0x2000
#define MQ100_GET_DEV_PARAM		0x800b
#define MQ100_SET_DEV_PARAM_ID		0x900c
#define MQ100_SET_DEV_PARAM		0x900d

/*
 * Algoithm parameter command codes
 */
#define MQ100_ALGO_PARAM_OFFSET		0x0000
#define MQ100_GET_ALGO_PARAM		0x8016
#define MQ100_SET_ALGO_PARAM_ID		0x9017
#define MQ100_SET_ALGO_PARAM		0x9018
#define ES_GET_EVENT				0x806D

/* MQ100 states */
#define MQ100_STATE_RESET (0)
#define MQ100_STATE_NORMAL (1)

/* data structures */
#if defined(CONFIG_MQ100_DEMO)
struct sensorhub_data {
		struct work_struct work;
		struct input_dev *acc_input_dev;
		struct input_dev *gyro_input_dev;
		struct input_dev *mag_input_dev;
		struct device *acc_device;
		struct device *gyro_device;
		struct device *mag_device;
};


#define SENSOR_DATA(sensorId) \
	(sensor->sensor_data[sensorId - OSP_SH_SENSOR_ID_FIRST])
#define LAST_SENSOR_NODE(sensorId)            \
	(SENSOR_DATA(sensorId).last_node.data.sensorData)
#endif

extern void mq100_indicate_state_change(u8 val);
int mq100_bootup(struct escore_priv *mq100);
irqreturn_t mq100_irq_work(int irq, void *data);
int mq100_core_probe(struct device *dev);
#endif
