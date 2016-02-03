/*
 * include/linux/muic/muic.h
 *
 * header file supporting MUIC common information
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seoyoung Jeong <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __MUIC_H__
#define __MUIC_H__

/* Status of IF PMIC chip (suspend and resume) */
enum {
	MUIC_SUSPEND		= 0,
	MUIC_RESUME,
};

/* MUIC Interrupt */
enum {
	MUIC_INTR_DETACH	= 0,
	MUIC_INTR_ATTACH
};

/* MUIC Dock Observer Callback parameter */
enum {
	MUIC_DOCK_DETACHED	= 0,
	MUIC_DOCK_DESKDOCK	= 1,
	MUIC_DOCK_CARDOCK	= 2,
	MUIC_DOCK_AUDIODOCK	= 7,
	MUIC_DOCK_SMARTDOCK	= 8,
	MUIC_DOCK_HMT		= 11,
};

/* MUIC Path */
enum {
	MUIC_PATH_USB_AP	= 0,
	MUIC_PATH_USB_CP,
	MUIC_PATH_UART_AP,
	MUIC_PATH_UART_CP,
	MUIC_PATH_OPEN,
	MUIC_PATH_AUDIO,
};

/* bootparam SWITCH_SEL */
enum {
	SWITCH_SEL_USB_MASK	= 0x1,
	SWITCH_SEL_UART_MASK	= 0x2,
	SWITCH_SEL_RUSTPROOF_MASK	= 0x8,
	SWITCH_SEL_AFC_DISABLE_MASK	= 0x100,
};

/* MUIC ADC table */
#ifdef CONFIG_MUIC_MAX77833	//CIS
typedef enum {
        ADC_GND                 = 0x00,
	ADC_1K			= 0x10, /* 0x010000 1K ohm */
        ADC_SEND_END            = 0x11, /* 0x010001 2K ohm */
	ADC_2_604K		= 0x12, /* 0x010010 2.604K ohm */
	ADC_3_208K		= 0x13, /* 0x010011 3.208K ohm */
	ADC_4_014K		= 0x14, /* 0x010100 4.014K ohm */
	ADC_4_820K		= 0x15, /* 0x010101 4.820K ohm */
	ADC_6_030K		= 0x16, /* 0x010110 6.030K ohm */
	ADC_8_030K		= 0x17, /* 0x010111 8.030K ohm */
	ADC_10_030K		= 0x18, /* 0x011000 10.030K ohm */
	ADC_12_030K		= 0x19, /* 0x011001 12.030K ohm */
	ADC_14_460K		= 0x1a, /* 0x011010 14.460K ohm */
	ADC_17_260K		= 0x1b, /* 0x011011 17.260K ohm */
        ADC_REMOTE_S11          = 0x1c, /* 0x011100 20.5K ohm */
        ADC_REMOTE_S12          = 0x1d, /* 0x011101 24.07K ohm */
        ADC_RESERVED_VZW        = 0x1e, /* 0x011110 28.7K ohm */
        ADC_INCOMPATIBLE_VZW    = 0x1f, /* 0x011111 34K ohm */
        ADC_SMARTDOCK           = 0x20, /* 0x100000 40.2K ohm */
        ADC_HMT                 = 0x21, /* 0x100001 49.9K ohm */
        ADC_AUDIODOCK           = 0x22, /* 0x100010 64.9K ohm */
        ADC_USB_LANHUB          = 0x23, /* 0x100011 80.07K ohm */
        ADC_CHARGING_CABLE      = 0x24, /* 0x100100 102K ohm */
        ADC_UNIVERSAL_MMDOCK    = 0x25, /* 0x100101 121K ohm */
        ADC_UART_CABLE          = 0x26, /* 0x100110 150K ohm */
        ADC_CEA936ATYPE1_CHG    = 0x27, /* 0x100111 200K ohm */
        ADC_JIG_USB_OFF         = 0x28, /* 0x101000 255K ohm */
        ADC_JIG_USB_ON          = 0x29, /* 0x101001 301K ohm */
        ADC_DESKDOCK            = 0x2a, /* 0x101010 365K ohm */
        ADC_CEA936ATYPE2_CHG    = 0x2b, /* 0x101011 442K ohm */
        ADC_JIG_UART_OFF        = 0x2c, /* 0x101100 523K ohm */
        ADC_JIG_UART_ON         = 0x2d, /* 0x101101 619K ohm */
        ADC_AUDIOMODE_W_REMOTE  = 0x2e, /* 0x101110 1000K ohm */
	ADC_1200K		= 0x2f, /* 0x101111 1200K ohm */

	/// MAX77843 ///
        ADC_OPEN                = 0x1f,
        ADC_OPEN_219            = 0xfb, /* ADC open or 219.3K ohm */
        ADC_219                 = 0xfc, /* ADC open or 219.3K ohm */

        ADC_UNDEFINED           = 0xfd, /* Undefied range */
        ADC_DONTCARE            = 0xfe, /* ADC don't care for MHL */
        ADC_ERROR               = 0xff, /* ADC value read error */

	ADC_JIG_UART_OFF_WA	= 0xfa, /* ADC temp for JIG UART cable */

} muic_adc_t;
#else
typedef enum {
	ADC_GND			= 0x00,
	ADC_SEND_END		= 0x01, /* 0x00001 2K ohm */
	ADC_REMOTE_S11		= 0x0c, /* 0x01100 20.5K ohm */
	ADC_REMOTE_S12		= 0x0d, /* 0x01101 24.07K ohm */
	ADC_RESERVED_VZW	= 0x0e, /* 0x01110 28.7K ohm */
	ADC_INCOMPATIBLE_VZW	= 0x0f, /* 0x01111 34K ohm */
	ADC_SMARTDOCK		= 0x10, /* 0x10000 40.2K ohm */
	ADC_HMT			= 0x11, /* 0x10001 49.9K ohm */
	ADC_AUDIODOCK		= 0x12, /* 0x10010 64.9K ohm */
	ADC_USB_LANHUB		= 0x13, /* 0x10011 80.07K ohm */
	ADC_CHARGING_CABLE	= 0x14,	/* 0x10100 102K ohm */
	ADC_UNIVERSAL_MMDOCK	= 0x15, /* 0x10101 121K ohm */
	ADC_UART_CABLE		= 0x16, /* 0x10110 150K ohm */
	ADC_CEA936ATYPE1_CHG	= 0x17,	/* 0x10111 200K ohm */
	ADC_JIG_USB_OFF		= 0x18, /* 0x11000 255K ohm */
	ADC_JIG_USB_ON		= 0x19, /* 0x11001 301K ohm */
	ADC_DESKDOCK		= 0x1a, /* 0x11010 365K ohm */
	ADC_CEA936ATYPE2_CHG	= 0x1b, /* 0x11011 442K ohm */
	ADC_JIG_UART_OFF	= 0x1c, /* 0x11100 523K ohm */
	ADC_JIG_UART_ON		= 0x1d, /* 0x11101 619K ohm */
	ADC_AUDIOMODE_W_REMOTE	= 0x1e, /* 0x11110 1000K ohm */
	ADC_OPEN		= 0x1f,
	ADC_OPEN_219		= 0xfb, /* ADC open or 219.3K ohm */
	ADC_219			= 0xfc, /* ADC open or 219.3K ohm */

	ADC_UNDEFINED		= 0xfd, /* Undefied range */
	ADC_DONTCARE		= 0xfe, /* ADC don't care for MHL */
	ADC_ERROR		= 0xff, /* ADC value read error */
} muic_adc_t;
#endif

/* MUIC attached device type */
typedef enum {
	ATTACHED_DEV_NONE_MUIC = 0,
	ATTACHED_DEV_USB_MUIC,
	ATTACHED_DEV_CDP_MUIC,
	ATTACHED_DEV_OTG_MUIC,
	ATTACHED_DEV_TA_MUIC,
	ATTACHED_DEV_UNOFFICIAL_MUIC,
	ATTACHED_DEV_UNOFFICIAL_TA_MUIC,
	ATTACHED_DEV_UNOFFICIAL_ID_MUIC,
	ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC,
	ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC,
	ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC,
	ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC,
	ATTACHED_DEV_UNDEFINED_CHARGING_MUIC,
	ATTACHED_DEV_DESKDOCK_MUIC,
	ATTACHED_DEV_DESKDOCK_VB_MUIC,
	ATTACHED_DEV_CARDOCK_MUIC,
	ATTACHED_DEV_JIG_UART_OFF_MUIC,
	ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,	/* VBUS enabled */
	ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC,	/* for otg test */
	ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC,	/* for fuelgauge test */
	ATTACHED_DEV_JIG_UART_ON_MUIC,
	ATTACHED_DEV_JIG_USB_OFF_MUIC,
	ATTACHED_DEV_JIG_USB_ON_MUIC,
	ATTACHED_DEV_SMARTDOCK_MUIC,
	ATTACHED_DEV_SMARTDOCK_VB_MUIC,
	ATTACHED_DEV_SMARTDOCK_TA_MUIC,
	ATTACHED_DEV_SMARTDOCK_USB_MUIC,
	ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC,
	ATTACHED_DEV_AUDIODOCK_MUIC,
	ATTACHED_DEV_MHL_MUIC,
	ATTACHED_DEV_CHARGING_CABLE_MUIC,
	ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC,
	ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC,
	ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC,
	ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC,
	ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC,
	ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC,
	ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC,
	ATTACHED_DEV_HMT_MUIC,

	ATTACHED_DEV_VZW_ACC_MUIC,
	ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC,
	ATTACHED_DEV_USB_LANHUB_MUIC,
	ATTACHED_DEV_TYPE2_CHG_MUIC,

	ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC,
	ATTACHED_DEV_UNKNOWN_MUIC,

	ATTACHED_DEV_NUM,
} muic_attached_dev_t;

/* muic common callback driver internal data structure
 * that setted at muic-core.c file
 */
struct muic_platform_data {
	int irq_gpio;

	/* muic current USB/UART path */
	int usb_path;
	int uart_path;

	int gpio_uart_sel;

	bool rustproof_on;
	bool afc_disable;

	/* muic switch dev register function for DockObserver */
	void (*init_switch_dev_cb) (void);
	void (*cleanup_switch_dev_cb) (void);

	/* muic GPIO control function */
	int (*init_gpio_cb) (int switch_sel);
	int (*set_gpio_usb_sel) (int usb_path);
	int (*set_gpio_uart_sel) (int uart_path);
	int (*set_safeout) (int safeout_path);

	/* muic path switch function for waterproof */
	void (*set_path_switch_suspend) (struct device *dev);
	void (*set_path_switch_resume) (struct device *dev);
};

#endif /* __MUIC_H__ */
