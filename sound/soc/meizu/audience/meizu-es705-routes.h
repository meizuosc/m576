/*
 * meizu-es705-routes.h  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/** Meizu-m76 usercase:
Route	Route	Algo
1    CT (2-Mic NB, WB)                        	1332	554/555
2    FT (3-Mic NB/WB W AEC)                   	1363	925/926
3    3-segment headset NB/WB                 	1313	592/593
4    BT/WHS (NB/WB)                           	1315	590/591(NS-ON,AEC-ON), 594/595(NS-OFF,AEC-OFF)
5    Stereo recording with UI/music PB       	2621	653(NS-ON)/654(NS-OFF)/655(NS-ON,DRC-ON)/656(NS-OFF,DRC-ON)
6    2Mic ASRA Omni W UI/music PB, AEC        	2648	946
7    2Mic ASRA CT W UI/music BP to SPK, AEC    	2650	600
8    1-Mic Rec with Top Mic (PDM3)            	1151
9    1-Mic Rec with Front Mic (PDM2)          	1152
10   1-Mic Rec with Bottom Mic (PDM0)         	1153
11   CVQ w/DHWPT
12   CT Voip (2-Mic W UI)                     	1051	554
13   FT Voip (3-Mic W UI, AEC ref)           	1101	925
14   3 Segment headset Voip                  	1005	592
15   BT/WHS VOIP                             	1006	590
16   Voice message recording                  	1155
17   CVQ                                     	7001/7002/7100/7101
18   Mono recording with UI/music PB            1159
19   Modem loopback                             1156
20   Fake route to stop prevous active route	1154
 */

#ifndef _MEIZU_ES705_ROUTES_H
#define _MEIZU_ES705_ROUTES_H

struct esxxx_route_config {
	const u32 *route;
	const u32 *nb;
	const u32 *wb;
	const u32 *swb;
	const u32 *fb;
	const u32 *pnb;
	const u32 *pwb;
	const u32 *pswb;
	const u32 *pfb;
};

enum {
	RATE_NB,
	RATE_WB,
	RATE_SWB,
	RATE_FB,
	RATE_MAX
};

enum {
	ROUTE_OFF = 0,

	// playback
	ROUTE_DHWPT = 1,

	// capture
	ROUTE_BOARD_MIC_RECORD = 2,
	ROUTE_HEADSET_MIC_RECORD = 3,
	ROUTE_BOARD_MIC_RECORD_NR_OFF = 4,
	ROUTE_HEADSET_MIC_RECORD_NR_OFF = 5,

	// voice message record
	ROUTE_VOICE_MESSAGE_RECORD = 10,

	// voice sense
	ROUTE_VOICE_ASR = 17,
	ROUTE_VOICE_SENSE_PDM = 22,

	// voice call (NB)
	ROUTE_VOICE_CALL_CT = 30,
	ROUTE_VOICE_CALL_FT = 31,
	ROUTE_VOICE_CALL_HEADPHONE = 32,
	ROUTE_VOICE_CALL_HEADSET = 33,
	ROUTE_VOICE_CALL_BT_NREC_ON = 34,
	ROUTE_VOICE_CALL_BT_NREC_OFF = 35,

	// voice call (WB)
	ROUTE_VOICE_CALL_WB_CT = 40,
	ROUTE_VOICE_CALL_WB_FT = 41,
	ROUTE_VOICE_CALL_WB_HEADPHONE = 42,
	ROUTE_VOICE_CALL_WB_HEADSET = 43,
	ROUTE_VOICE_CALL_WB_BT_NREC_ON = 44,
	ROUTE_VOICE_CALL_WB_BT_NREC_OFF = 45,

	// VoIP
	ROUTE_VOIP_CT = 50,
	ROUTE_VOIP_FT = 51,
	ROUTE_VOIP_HEADPHONE = 52,
	ROUTE_VOIP_HEADSET = 53,

	// factory test
	ROUTE_FACTORY_TEST_TOP_MIC = 90,
	ROUTE_FACTORY_TEST_FRONT_MIC = 91,
	ROUTE_FACTORY_TEST_BOTTOM_MIC = 92,
	ROUTE_FACTORY_TEST_MODEM_LOOPBACK_EARPHONE = 93,
	ROUTE_FACTORY_TEST_MODEM_LOOPBACK_HEADPHONE = 94,

	ROUTE_MAX,
};

static const u32 route_off[] = {
	0x904e0000, /* #smooth rate to 0   */
	0x903101f7, /* #503  : Port A slave */
	0x90310200, /* #512  : Port D slave */
	0x90310482, /* #1154 : inactive route*/
	0x80620008, /* Stop voice call Algo */
	0x805200f3, /* #DHWPT: Port D to A, clock from Port D */
	0xffffffff, /* terminate */
};

/* media preset defination */
static const u32 route_dhwpt[] = {
	0x904e0000, /* #smooth rate to 0   */
	0x903101f7, /* #503  : Port A slave */
	0x90310200, /* #512  : Port D slave */
	0x90310482, /* #1154 : inactive route*/
	0x80620008, /* Stop voice call Algo */
	0x805200f3, /* #DHWPT: Port D to A, clock from Port D */
	0xffffffff, /* terminate */
};
static const u32 route_stereo_record_with_48khz_uitone[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x90310a3d, /* #2621 : Stereo recording with UI/music PB */
	0x8031028d, /* #653 : Avalon Algo Preset Record 24K */
	0xffffffff, /* terminate */
};
static const u32 route_headset_mic_record_with_48khz_uitone[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x90310487, /* #1159 : Mono recording with UI/music PB */
	0x8031035f, /* #863 : Passthrough Algo Preset 1 Ch Audio PT FB */
	0xffffffff, /* terminate */
};
static const u32 route_stereo_record_with_48khz_uitone_nr_off[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x90310a3d, /* #2621 : Stereo recording with UI/music PB */
	0x8031028e, /* #654 : Avalon Algo Preset Record 24K (NR-OFF) */
	0xffffffff, /* terminate */
};
static const u32 route_headset_mic_record_with_48khz_uitone_nr_off[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x90310487, /* #1159 : Mono recording with UI/music PB */
	0x8031035f, /* #863 : Passthrough Algo Preset 1 Ch Audio PT FB */
	0xffffffff, /* terminate */
};

/* voice message record preset defination */
static const u32 route_voice_message_record[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	0x80310483, /* #1155  : Voice message recording */
	0xffffffff, /* terminate */
};

/* voice sense preset defination */
static const u32 route_voice_asr[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	0x80311bbc, /* #7100 : ASRA route preset */
	0xffffffff, /* terminate */
};
static const u32 route_voice_sense_pdm[] = {
	0x80310566, /* #1382 */
	0xffffffff, /* terminate */
};

/* voice call (NB) preset defination */
static const u32 route_cs_voice_2mic_ct[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310534, /* #1332 : CT (2-Mic NB, WB) */
	0x8031022a, /* #554 : CT (2-Mic NB, WB) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_2mic_ft[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310553, /* #1363 : FT (2-Mic NB, WB W AEC) */
	0x8031039d, /* #925 : FT (2-Mic NB, WB W AEC) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_headphone[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310521, /* #1313 : 3-segment headphone NB/WB */
	0x80310250, /* #592 : 3-segment headphone NB/WB */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_headset[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310523, /* #1315 : BT/WHS (NB/WB) */
	0x8031024e, /* #590 : BT/WHS (NB/WB) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_bt_nrec_on[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310523, /* #1315 : BT/WHS (NB/WB) */
	0x8031024e, /* #590 : BT/WHS (NB/WB) nrec on*/
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_bt_nrec_off[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x90310523, /* #1315 : BT/WHS (NB/WB) */
	0x80310252, /* #594 : BT/WHS (NB/WB) nrec off*/
	0xffffffff, /* terminate */
};

/* voice call (WB) preset defination */
static const u32 route_cs_voice_wb_2mic_ct[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310534, /* #1332 : CT (2-Mic NB, WB) */
	0x9031022b, /* #555 : CT (2-Mic NB, WB) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_wb_2mic_ft[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310553, /* #1363 : FT (2-Mic NB, WB W AEC) */
	0x9031039e, /* #926 : FT (2-Mic NB, WB W AEC) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_wb_headphone[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310521, /* #1313 : 3-segment headphone NB/WB */
	0x90310251, /* #593 : 3-segment headphone NB/WB */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_wb_headset[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310523, /* #1315 : BT/WHS (NB/WB) */
	0x9031024f, /* #591 : BT/WHS (NB/WB) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_wb_bt_nrec_on[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310523, /* #1315 : BT/WHS (NB/WB) */
	0x9031024f, /* #591 : BT/WHS (NB/WB) */
	0xffffffff, /* terminate */
};
static const u32 route_cs_voice_wb_bt_nrec_off[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f6, /* #502  : Port A master-16k */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x903101ff, /* #511  : Port D master-16k */
	0x80310523, /* #1315 : BT/WHS (NB/WB) */
	0x90310253, /* #595 : BT/WHS (NB/WB) nrec off*/
	0xffffffff, /* terminate */
};

/* voip preset defination */
static const u32 route_voip_2mic_ct[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x9031041b, /* #1051 : CT Voip (2-Mic W UI) */
	0x8031022a, /* #554 : CT Voip (2-Mic W UI) */
	0xffffffff, /* terminate */
};
static const u32 route_voip_2mic_ft[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x9031044d, /* #1101 : FT Voip (3-Mic W UI, AEC ref) */
	0x8031039d, /* #925 : FT Voip (3-Mic W UI, AEC ref) */
	0xffffffff, /* terminate */
};
static const u32 route_voip_headphone[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x903103ed, /* #1005 : 3 Segment headset Voip */
	0x80310250, /* #592 : 3 Segment headset Voip */
	0xffffffff, /* terminate */
};
static const u32 route_voip_headset[] = {
	0x90520000, /* #DHWPT : disable */
	0x903101f5, /* #501  : Port A master-48k */
	//0x903101fe, /* #510  : Port D master-48k */
	0x903103ee, /* #1006 : BT/WHS VOIP */
	0x8031024e, /* #590 : BT/WHS VOIP */
	0xffffffff, /* terminate */
};

/* factory test */
static const u32 route_factory_test_top_mic[] = {
	0x80520000, /* #DHWPT : disable */
	//0x803101fe, /* #510  : Port D master-48k */
	0x8031047f, /* #1151 : 1-Mic Rec with Top Mic (PDM3) */
	0xffffffff, /* terminate */
};
static const u32 route_factory_test_front_mic[] = {
	0x80520000, /* #DHWPT : disable */
	//0x803101fe, /* #510  : Port D master-48k */
	0x80310480, /* #1152 : 1-Mic Rec with front Mic (PDM2) */
	0xffffffff, /* terminate */
};
static const u32 route_factory_test_bottom_mic[] = {
	0x80520000, /* #DHWPT : disable */
	//0x803101fe, /* #510  : Port D master-48k */
	0x80310481, /* #1153 : 1-Mic Rec with bottom Mic (PDM0) */
	0xffffffff, /* terminate */
};
static const u32 route_factory_test_modem_loopback_earphone[] = {
	0x80520000, /* #DHWPT : disable */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x803101fe, /* #510  : Port D master-48k */
	0x80310484, /* #1156 : modem looback */
	0xffffffff, /* terminate */
};
static const u32 route_factory_test_modem_loopback_headphone[] = {
	0x80520000, /* #DHWPT : disable */
	//0x903101f9, /* #505  : Port B-PCM-Master-16k */
	//0x803101fe, /* #510  : Port D master-48k */
	0x80310485, /* #1157 : modem looback */
	0xffffffff, /* terminate */
};

static const struct esxxx_route_config es705_route_config[ROUTE_MAX] = {
	[ROUTE_OFF] = {
		.route = route_off,
	},

	/* media route */
	[ROUTE_DHWPT] = {
		.route = route_dhwpt,
	},
	[ROUTE_BOARD_MIC_RECORD] = {
		.route = route_stereo_record_with_48khz_uitone,
	},
	[ROUTE_HEADSET_MIC_RECORD] = {
		.route = route_headset_mic_record_with_48khz_uitone,
	},
	[ROUTE_BOARD_MIC_RECORD_NR_OFF] = {
		.route = route_stereo_record_with_48khz_uitone_nr_off,
	},
	[ROUTE_HEADSET_MIC_RECORD_NR_OFF] = {
		.route = route_headset_mic_record_with_48khz_uitone_nr_off,
	},

	/* voice sense route */
	[ROUTE_VOICE_ASR] = {
		.route = route_voice_asr,
	},
	[ROUTE_VOICE_SENSE_PDM] = {
		.route = route_voice_sense_pdm,
	},

	/* voice call (NB) route */
	[ROUTE_VOICE_CALL_CT] = {
		.route = route_cs_voice_2mic_ct,
	},
	[ROUTE_VOICE_CALL_FT] = {
		.route = route_cs_voice_2mic_ft,
	},
	[ROUTE_VOICE_CALL_HEADPHONE] = {
		.route = route_cs_voice_headphone,
	},
	[ROUTE_VOICE_CALL_HEADSET] = {
		.route = route_cs_voice_headset,
	},
	[ROUTE_VOICE_CALL_BT_NREC_ON] = {
		.route = route_cs_voice_bt_nrec_on,
	},
	[ROUTE_VOICE_CALL_BT_NREC_OFF] = {
		.route = route_cs_voice_bt_nrec_off,
	},

	/* voice call (WB) route */
	[ROUTE_VOICE_CALL_WB_CT] = {
		.route = route_cs_voice_wb_2mic_ct,
	},
	[ROUTE_VOICE_CALL_WB_FT] = {
		.route = route_cs_voice_wb_2mic_ft,
	},
	[ROUTE_VOICE_CALL_WB_HEADPHONE] = {
		.route = route_cs_voice_wb_headphone,
	},
	[ROUTE_VOICE_CALL_WB_HEADSET] = {
		.route = route_cs_voice_wb_headset,
	},
	[ROUTE_VOICE_CALL_WB_BT_NREC_ON] = {
		.route = route_cs_voice_wb_bt_nrec_on,
	},
	[ROUTE_VOICE_CALL_WB_BT_NREC_OFF] = {
		.route = route_cs_voice_wb_bt_nrec_off,
	},

	/* voip route */
	[ROUTE_VOIP_CT] = {
		.route = route_voip_2mic_ct,
	},
	[ROUTE_VOIP_FT] = {
		.route = route_voip_2mic_ft,
	},
	[ROUTE_VOIP_HEADPHONE] = {
		.route = route_voip_headphone,
	},
	[ROUTE_VOIP_HEADSET] = {
		.route = route_voip_headset,
	},

	[ROUTE_VOICE_MESSAGE_RECORD] = {
		.route = route_voice_message_record,
	},

	/* factory test route */
	[ROUTE_FACTORY_TEST_TOP_MIC] = {
		.route = route_factory_test_top_mic,
	},
	[ROUTE_FACTORY_TEST_FRONT_MIC] = {
		.route = route_factory_test_front_mic,
	},
	[ROUTE_FACTORY_TEST_BOTTOM_MIC] = {
		.route = route_factory_test_bottom_mic,
	},
	[ROUTE_FACTORY_TEST_MODEM_LOOPBACK_EARPHONE] = {
		.route = route_factory_test_modem_loopback_earphone,
	},
	[ROUTE_FACTORY_TEST_MODEM_LOOPBACK_HEADPHONE] = {
		.route = route_factory_test_modem_loopback_headphone,
	},
};

#endif
