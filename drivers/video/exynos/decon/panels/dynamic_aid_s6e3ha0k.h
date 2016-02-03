#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_VT,
	IV_3,
	IV_11,
	IV_23,
	IV_35,
	IV_51,
	IV_87,
	IV_151,
	IV_203,
	IV_255,
	IV_MAX,
};

enum {
	IBRIGHTNESS_2NT,
	IBRIGHTNESS_3NT,
	IBRIGHTNESS_4NT,
	IBRIGHTNESS_5NT,
	IBRIGHTNESS_6NT,
	IBRIGHTNESS_7NT,
	IBRIGHTNESS_8NT,
	IBRIGHTNESS_9NT,
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_11NT,
	IBRIGHTNESS_12NT,
	IBRIGHTNESS_13NT,
	IBRIGHTNESS_14NT,
	IBRIGHTNESS_15NT,
	IBRIGHTNESS_16NT,
	IBRIGHTNESS_17NT,
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_24NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_34NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_350NT,
	IBRIGHTNESS_500NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6200	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	0,	/* IV_VT */
	3,	/* IV_3 */
	11,	/* IV_11 */
	23,	/* IV_23 */
	35,	/* IV_35 */
	51,	/* IV_51 */
	87,	/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255	/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] = {
	2,	/* IBRIGHTNESS_2NT */
	3,	/* IBRIGHTNESS_3NT */
	4,	/* IBRIGHTNESS_4NT */
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,	/* IBRIGHTNESS_10NT */
	11,	/* IBRIGHTNESS_11NT */
	12,	/* IBRIGHTNESS_12NT */
	13,	/* IBRIGHTNESS_13NT */
	14,	/* IBRIGHTNESS_14NT */
	15,	/* IBRIGHTNESS_15NT */
	16,	/* IBRIGHTNESS_16NT */
	17,	/* IBRIGHTNESS_17NT */
	19,	/* IBRIGHTNESS_19NT */
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	24,	/* IBRIGHTNESS_24NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	30,	/* IBRIGHTNESS_30NT */
	32,	/* IBRIGHTNESS_32NT */
	34,	/* IBRIGHTNESS_34NT */
	37,	/* IBRIGHTNESS_37NT */
	39,	/* IBRIGHTNESS_39NT */
	41,	/* IBRIGHTNESS_41NT */
	44,	/* IBRIGHTNESS_44NT */
	47,	/* IBRIGHTNESS_47NT */
	50,	/* IBRIGHTNESS_50NT */
	53,	/* IBRIGHTNESS_53NT */
	56,	/* IBRIGHTNESS_56NT */
	60,	/* IBRIGHTNESS_60NT */
	64,	/* IBRIGHTNESS_64NT */
	68,	/* IBRIGHTNESS_68NT */
	72,	/* IBRIGHTNESS_72NT */
	77,	/* IBRIGHTNESS_77NT */
	82,	/* IBRIGHTNESS_82NT */
	87,	/* IBRIGHTNESS_87NT */
	93,	/* IBRIGHTNESS_93NT */
	98,	/* IBRIGHTNESS_98NT */
	105,	/* IBRIGHTNESS_105NT */
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_500NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] = {
	0x00, 0x00, 0x00,	/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100,	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860},	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int brightness_base_table[IBRIGHTNESS_MAX] = {
	33,	/* IBRIGHTNESS_2NT */
	33,	/* IBRIGHTNESS_3NT */
	33,	/* IBRIGHTNESS_4NT */
	110,	/* IBRIGHTNESS_5NT */
	110,	/* IBRIGHTNESS_6NT */
	110,	/* IBRIGHTNESS_7NT */
	110,	/* IBRIGHTNESS_8NT */
	110,	/* IBRIGHTNESS_9NT */
	110,	/* IBRIGHTNESS_10NT */
	110,	/* IBRIGHTNESS_11NT */
	110,	/* IBRIGHTNESS_12NT */
	110,	/* IBRIGHTNESS_13NT */
	110,	/* IBRIGHTNESS_14NT */
	110,	/* IBRIGHTNESS_15NT */
	110,	/* IBRIGHTNESS_16NT */
	110,	/* IBRIGHTNESS_17NT */
	110,	/* IBRIGHTNESS_19NT */
	110,	/* IBRIGHTNESS_20NT */
	110,	/* IBRIGHTNESS_21NT */
	110,	/* IBRIGHTNESS_22NT */
	110,	/* IBRIGHTNESS_24NT */
	110,	/* IBRIGHTNESS_25NT */
	110,	/* IBRIGHTNESS_27NT */
	110,	/* IBRIGHTNESS_29NT */
	110,	/* IBRIGHTNESS_30NT */
	110,	/* IBRIGHTNESS_32NT */
	110,	/* IBRIGHTNESS_34NT */
	110,	/* IBRIGHTNESS_37NT */
	110,	/* IBRIGHTNESS_39NT */
	110,	/* IBRIGHTNESS_41NT */
	110,	/* IBRIGHTNESS_44NT */
	110,	/* IBRIGHTNESS_47NT */
	110,	/* IBRIGHTNESS_50NT */
	110,	/* IBRIGHTNESS_53NT */
	110,	/* IBRIGHTNESS_56NT */
	110,	/* IBRIGHTNESS_60NT */
	114,	/* IBRIGHTNESS_64NT */
	121,	/* IBRIGHTNESS_68NT */
	127,	/* IBRIGHTNESS_72NT */
	137,	/* IBRIGHTNESS_77NT */
	145,	/* IBRIGHTNESS_82NT */
	151,	/* IBRIGHTNESS_87NT */
	161,	/* IBRIGHTNESS_93NT */
	169,	/* IBRIGHTNESS_98NT */
	180,	/* IBRIGHTNESS_105NT */
	188,	/* IBRIGHTNESS_111NT */
	200,	/* IBRIGHTNESS_119NT */
	210,	/* IBRIGHTNESS_126NT */
	221,	/* IBRIGHTNESS_134NT */
	235,	/* IBRIGHTNESS_143NT */
	249,	/* IBRIGHTNESS_152NT */
	249,	/* IBRIGHTNESS_162NT */
	249,	/* IBRIGHTNESS_172NT */
	249,	/* IBRIGHTNESS_183NT */
	249,	/* IBRIGHTNESS_195NT */
	249,	/* IBRIGHTNESS_207NT */
	249,	/* IBRIGHTNESS_220NT */
	249,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table,	/* IBRIGHTNESS_2NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_3NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_4NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_34NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_282NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_300NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_350NT */
	gamma_curve_2p20_table	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0x09, 0x1A},	/* IBRIGHTNESS_2NT */
	{0x09, 0x1A},	/* IBRIGHTNESS_3NT */
	{0x09, 0x1A},	/* IBRIGHTNESS_4NT */
	{0x09, 0x8F},	/* IBRIGHTNESS_5NT */
	{0x09, 0x78},	/* IBRIGHTNESS_6NT */
	{0x09, 0x60},	/* IBRIGHTNESS_7NT */
	{0x09, 0x4A},	/* IBRIGHTNESS_8NT */
	{0x09, 0x33},	/* IBRIGHTNESS_9NT */
	{0x09, 0x1A},	/* IBRIGHTNESS_10NT */
	{0x09, 0x02},	/* IBRIGHTNESS_11NT */
	{0x08, 0xEC},	/* IBRIGHTNESS_12NT */
	{0x08, 0xD5},	/* IBRIGHTNESS_13NT */
	{0x08, 0xBB},	/* IBRIGHTNESS_14NT */
	{0x08, 0xA4},	/* IBRIGHTNESS_15NT */
	{0x08, 0x8C},	/* IBRIGHTNESS_16NT */
	{0x08, 0x74},	/* IBRIGHTNESS_17NT */
	{0x08, 0x44},	/* IBRIGHTNESS_19NT */
	{0x08, 0x28},	/* IBRIGHTNESS_20NT */
	{0x08, 0x14},	/* IBRIGHTNESS_21NT */
	{0x07, 0xFC},	/* IBRIGHTNESS_22NT */
	{0x07, 0xD8},	/* IBRIGHTNESS_24NT */
	{0x07, 0xB2},	/* IBRIGHTNESS_25NT */
	{0x07, 0x82},	/* IBRIGHTNESS_27NT */
	{0x07, 0x4D},	/* IBRIGHTNESS_29NT */
	{0x07, 0x32},	/* IBRIGHTNESS_30NT */
	{0x07, 0x02},	/* IBRIGHTNESS_32NT */
	{0x06, 0xD5},	/* IBRIGHTNESS_34NT */
	{0x06, 0x85},	/* IBRIGHTNESS_37NT */
	{0x06, 0x55},	/* IBRIGHTNESS_39NT */
	{0x06, 0x23},	/* IBRIGHTNESS_41NT */
	{0x05, 0xD6},	/* IBRIGHTNESS_44NT */
	{0x05, 0x8A},	/* IBRIGHTNESS_47NT */
	{0x05, 0x45},	/* IBRIGHTNESS_50NT */
	{0x04, 0xF1},	/* IBRIGHTNESS_53NT */
	{0x04, 0xA0},	/* IBRIGHTNESS_56NT */
	{0x04, 0x40},	/* IBRIGHTNESS_60NT */
	{0x04, 0x11},	/* IBRIGHTNESS_64NT */
	{0x04, 0x11},	/* IBRIGHTNESS_68NT */
	{0x04, 0x11},	/* IBRIGHTNESS_72NT */
	{0x04, 0x11},	/* IBRIGHTNESS_77NT */
	{0x04, 0x11},	/* IBRIGHTNESS_82NT */
	{0x04, 0x11},	/* IBRIGHTNESS_87NT */
	{0x04, 0x11},	/* IBRIGHTNESS_93NT */
	{0x04, 0x11},	/* IBRIGHTNESS_98NT */
	{0x04, 0x11},	/* IBRIGHTNESS_105NT */
	{0x04, 0x11},	/* IBRIGHTNESS_111NT */
	{0x04, 0x11},	/* IBRIGHTNESS_119NT */
	{0x04, 0x11},	/* IBRIGHTNESS_126NT */
	{0x04, 0x11},	/* IBRIGHTNESS_134NT */
	{0x04, 0x11},	/* IBRIGHTNESS_143NT */
	{0x04, 0x11},	/* IBRIGHTNESS_152NT */
	{0x03, 0xB1},	/* IBRIGHTNESS_162NT */
	{0x03, 0x3D},	/* IBRIGHTNESS_172NT */
	{0x02, 0xB8},	/* IBRIGHTNESS_183NT */
	{0x02, 0x29},	/* IBRIGHTNESS_195NT */
	{0x01, 0x97},	/* IBRIGHTNESS_207NT */
	{0x00, 0xF6},	/* IBRIGHTNESS_220NT */
	{0x00, 0x48},	/* IBRIGHTNESS_234NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_249NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_265NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_282NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_300NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_316NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_333NT */
	{0x00, 0x0A},	/* IBRIGHTNESS_350NT */
	{0x00, 0x0A}	/* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 24, 25, 23, 21, 20, 17, 12, 6, 0},
	{0, 27, 26, 25, 24, 23, 20, 17, 13, 10},
	{0, 23, 26, 25, 24, 24, 24, 24, 22, 22},
	{0, 34, 33, 31, 28, 27, 23, 16, 9, 0},
	{0, 33, 33, 31, 28, 27, 22, 16, 8, 0},
	{0, 32, 31, 29, 26, 25, 21, 15, 8, 0},
	{0, 30, 29, 28, 25, 24, 20, 14, 8, 0},
	{0, 28, 27, 26, 23, 21, 18, 13, 8, 0},
	{0, 25, 25, 23, 21, 20, 17, 12, 7, 0},
	{0, 25, 24, 22, 20, 18, 16, 12, 7, 0},
	{0, 21, 23, 21, 20, 18, 15, 12, 7, 0},
	{0, 22, 22, 20, 18, 17, 15, 11, 7, 0},
	{0, 20, 21, 19, 17, 16, 14, 11, 6, 0},
	{0, 20, 20, 19, 17, 16, 14, 10, 6, 0},
	{0, 20, 20, 18, 17, 15, 13, 10, 6, 0},
	{0, 20, 18, 17, 15, 14, 12, 9, 6, 0},
	{0, 18, 17, 16, 14, 13, 12, 9, 6, 0},
	{0, 15, 15, 14, 12, 12, 11, 9, 6, 0},
	{0, 17, 15, 15, 13, 12, 11, 9, 5, 0},
	{0, 19, 15, 14, 12, 12, 10, 8, 5, 0},
	{0, 17, 15, 14, 12, 12, 11, 9, 7, 1},
	{0, 14, 14, 13, 11, 10, 9, 7, 5, 0},
	{0, 13, 13, 12, 10, 10, 9, 7, 5, 0},
	{0, 13, 13, 11, 10, 9, 8, 6, 5, 0},
	{0, 14, 12, 11, 9, 9, 8, 7, 5, 0},
	{0, 11, 12, 11, 9, 9, 7, 6, 5, 0},
	{0, 14, 11, 10, 8, 8, 8, 6, 5, 0},
	{0, 9, 11, 9, 8, 7, 7, 6, 4, 0},
	{0, 9, 10, 8, 7, 7, 7, 6, 4, 0},
	{0, 10, 9, 8, 7, 7, 7, 6, 5, 0},
	{0, 9, 9, 8, 7, 7, 6, 6, 4, 0},
	{0, 7, 8, 7, 6, 6, 6, 6, 4, 0},
	{0, 9, 7, 7, 6, 6, 6, 6, 4, 0},
	{0, 6, 7, 6, 5, 5, 5, 5, 4, 0},
	{0, 5, 7, 6, 5, 5, 5, 5, 4, 0},
	{0, 5, 6, 6, 4, 5, 4, 5, 4, 0},
	{0, 5, 6, 4, 4, 4, 5, 4 , 3 , 0},
	{0, 6, 6, 5, 4, 4, 5, 5 , 3 , 0},
	{0, 7, 6, 5, 5, 5, 6, 5 , 3 , 0},
	{0, 7, 5, 4, 4, 4, 5, 4 , 3 , 0},
	{0, 5, 6, 5, 4, 5, 5, 6 , 3 , 0},
	{0, 6, 5, 4, 4, 4, 5, 5 , 3 , 0},
	{0, 7, 5, 5, 4, 4, 4, 5 , 3 , 0},
	{0, 7, 5, 5, 4, 4, 5, 4 , 1 , 0},
	{0, 7, 4, 4, 4, 4, 4, 4 , 1 , 0},
	{0, 4, 5, 4, 3, 4, 5, 5 , 2 , 0},
	{0, 2, 5, 4, 4, 4, 5, 6 , 2 , 0},
	{0, 3, 5, 4, 4, 4, 5, 5 , 2 , 0},
	{0, 2, 4, 3, 3, 3, 5, 5 , 2 , 0},
	{0, 5, 4, 3, 3, 4, 5, 6 , 4 , 0},
	{0, 2, 4, 3, 3, 4, 5, 5, 2, 0},
	{0, 3, 3, 2, 3, 3, 4, 4, 2, 0},
	{0, 6, 3, 2, 3, 3, 4, 4, 2, 0},
	{0, 4, 2, 1, 2, 2, 3, 3, 2, 0},
	{0, 1, 2, 1, 2, 2, 3, 3, 1, 0},
	{0, 3, 1, 1, 1, 2, 2, 3, 1, 0},
	{0, 0, -1, -1, 0, 0, 0, 1, 0, 0},
	{0, 0, -1, -1, 0, 0, 0, 1, 1, 0},
	{0, 3, 1, 1, 2, 2, 3, 4, 3, 3},
	{0, 3, 1, 1, 1, 1, 2, 3, 2, 1},
	{0, 0, 1, 1, 1, 1, 2, 2, 1, 1},
	{0, 0, 1, 0, 1, 1, 1, 2, 2, 1},
	{0, 4, 0, 0, 0, 0, 0, 0, 1, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -2, 2, -9, -4, 4, -8, -6, 6, -17, -15, 7, -19, -17, 3, -13, -6, 1, -4, -3, 1, -3, -7, -2, -13},
	{0, 0, 0, 0, 0, 0, -8, 2, -13, -9, 4, -16, -13, 2, -18, -19, 3, -17, -15, 2, -11, -5, 0, -6, -4, 0, -5, -2, 0, -7},
	{0, 0, 0, 0, 0, 0, -8, 3, -16, -9, 4, -16, -14, 4, -16, -18, 4, -16, -11, 2, -10, -3, 0, -4, -4, 0, -4, -2, 0, -6},
	{0, 0, 0, 0, 0, 0, -7, 6, -18, -9, 5, -15, -11, 5, -14, -18, 4, -13, -9, 3, -9, -3, 1, -5, -3, 0, -3, -2, 1, -5},
	{0, 0, 0, 0, 0, 0, -10, 4, -20, -13, 3, -18, -15, 3, -16, -17, 3, -12, -9, 2, -9, -4, 0, -5, -2, 1, -3, -2, 0, -6},
	{0, 0, 0, 0, 0, 0, -10, 4, -22, -15, 3, -19, -16, 3, -17, -17, 2, -12, -8, 1, -10, -4, 0, -5, -2, 0, -3, -2, 0, -5},
	{0, 0, 0, 0, 0, 0, -11, 6, -25, -14, 3, -17, -16, 3, -15, -15, 1, -11, -10, 0, -10, -3, 0, -4, -3, 0, -4, -1, 0, -4},
	{0, 0, 0, 0, 0, 0, -7, 8, -21, -24, -2, -25, -13, 3, -12, -14, 1, -10, -7, 1, -8, -3, 0, -4, -2, 0, -3, -1, 0, -4},
	{0, 0, 0, 0, 0, 0, -10, 6, -25, -16, 4, -17, -16, 3, -14, -12, 2, -9, -8, 0, -9, -3, 0, -4, -2, 0, -2, -1, 0, -4},
	{0, 0, 0, 0, 0, 0, -10, 6, -25, -17, 3, -18, -17, 2, -14, -11, 2, -9, -8, 0, -9, -3, 0, -3, -1, 0, -3, -1, 0, -3},
	{0, 0, 0, 0, 0, 0, -12, 6, -27, -16, 4, -16, -15, 2, -11, -11, 2, -9, -6, 0, -7, -3, 0, -4, -2, 0, -2, 0, 0, -3},
	{0, 0, 0, 0, 0, 0, -11, 6, -26, -18, 3, -16, -15, 2, -13, -11, 1, -9, -6, 0, -7, -2, 0, -4, -2, 0, -2, 0, 0, -3},
	{0, 0, 0, 0, 0, 0, -15, 5, -27, -17, 3, -16, -16, 2, -12, -8, 2, -7, -5, 0, -6, -2, 0, -3, -2, 0, -3, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -16, 7, -30, -16, 3, -14, -15, 2, -11, -8, 1, -8, -6, 0, -6, -2, 0, -3, -2, 0, -3, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -13, 5, -29, -20, 3, -17, -14, 0, -9, -10, 0, -10, -6, 0, -6, -2, 0, -4, -1, 0, -2, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -16, 7, -31, -16, 3, -14, -14, 1, -10, -8, 1, -8, -4, 0, -5, -2, 0, -3, -1, 0, -2, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -17, 6, -32, -18, 2, -13, -11, 1, -9, -8, 1, -7, -3, 0, -4, -2, 0, -3, -1, 0, -2, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -11, 9, -28, -14, 4, -10, -10, 3, -8, -6, 2, -5, -2, 1, -4, -2, 0, -2, -1, 0, -1, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -11, 11, -29, -18, 2, -13, -11, 1, -9, -7, 1, -6, -4, 0, -4, -1, 0, -3, -1, 0, -1, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -16, 7, -33, -17, 2, -12, -12, 1, -10, -6, 1, -6, -4, 0, -4, -2, 0, -3, 0, 0, -1, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, -21, 5, -35, -15, 2, -10, -10, 1, -8, -6, 0, -6, -3, 0, -4, -1, 0, -2, -2, 0, -2, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -16, 7, -32, -15, 2, -10, -10, 1, -8, -6, 0, -6, -3, 0, -3, -1, 0, -2, -1, 1, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -11, 9, -30, -16, 1, -10, -10, 1, -8, -6, 0, -6, -2, 0, -2, -1, 0, -2, -2, 0, -2, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -16, 5, -34, -17, 2, -11, -9, 0, -7, -5, 0, -5, -2, 0, -3, -2, 0, -2, 0, 0, -2, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 8, -35, -16, 1, -10, -8, 1, -7, -4, 0, -5, -3, 0, -2, -1, 0, -2, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -14, 8, -32, -14, 1, -9, -8, 0, -7, -5, 0, -5, -2, 0, -3, -1, 0, -1, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -14, 7, -37, -14, 1, -9, -8, 0, -7, -4, 0, -4, -2, 0, -3, -1, 0, -1, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -18, 4, -35, -14, 1, -7, -5, 1, -5, -3, 0, -4, -2, 0, -2, -1, 0, -1, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -14, 6, -33, -11, 2, -8, -7, 1, -5, -2, 0, -4, -2, 0, -2, -1, 0, -1, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -12, 8, -34, -10, 2, -7, -6, 1, -4, -2, 0, -4, -1, 0, -1, -1, 0, -1, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -13, 7, -33, -10, 1, -7, -6, 0, -5, -3, 0, -4, -1, 0, -2, -1, 0, 0, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -7, 10, -28, -8, 1, -8, -5, 0, -4, -2, 0, -4, -1, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -9, 10, -34, -5, 2, -6, -4, 0, -4, -2, 0, -3, -1, 0, -1, -1, 0, 0, -1, 0, -1, 1, 0, -1},
	{0, 0, 0, 0, 0, 0, -9, 8, -29, -5, 2, -7, -4, 0, -3, -2, 0, -3, -1, 0, 0, -1, 0, -2, -1, 0, -1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -9, 6, -27, -3, 2, -7, -4, 0, -3, -2, 0, -3, 0, 0, 1, -1, 0, -2, -1, 0, -1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 8, -28, -3, 1, -6, -3, 0, -3, -1, 0, -2, -1, 0, -1, 0, 0, -1, -1, 0, -1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 6, -27, -1, 2, -6, -2, 0, -2, -2, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -8, 6, -29, -2, 1, -7, -2, 0, -2, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 7, -28, -2, 1, -6, -2, 0, -2, -1, 0, 0, -2, 0, -1, 0, 0, -1, -1, 0, -1, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -6, 7, -27, -2, 1, -6, -2, 0, -2, -1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 5, -25, -4, 1, -5, -2, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, -2, -1, 0, -1, 2, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 6, -26, -3, 1, -5, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 7, -25, -4, 0, -4, -2, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 0, -1, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 8, -23, -4, 0, -4, -3, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, 1, 7, -22, -3, 0, -3, -2, 0, -3, 0, 0, 0, -1, 0, -1, 0, 0, -1, 1, 0, 1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 4, -20, -4, 0, -4, -2, 0, -2, 1, 0, 1, 0, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 5, -18, -4, 0, -2, -2, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, -1, -1, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -19, -5, 0, -1, -1, 0, -2, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 4, -16, -4, 0, -2, -1, 0, -1, 1, 0, 1, -1, 0, -1, 0, 0, -1, 1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 4, -17, -4, 0, -1, -2, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 4, -15, -2, 0, -1, -2, 0, -1, 1, 0, 2, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 2, 4, -15, -3, 0, 0, -1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 5, 3, -13, -3, 0, 0, 0, 0, -1, 0, 0, 2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 4, 3, -15, 0, 0, -1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 7, 3, -13, 2, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 10, 3, -13, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 27, 0, 0, 7, 0, 0, 1, 0, -2, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 27, 0, 0, 6, 0, -1, 3, 0, -1, 0, 0, 2, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#ifdef CONFIG_LCD_HMT
enum {
	HMT_SCAN_SINGLE,
	HMT_SCAN_DUAL,
	HMT_SCAN_MAX
};

enum {
	HMT_IBRIGHTNESS_80NT,
	HMT_IBRIGHTNESS_95NT,
	HMT_IBRIGHTNESS_115NT,
	HMT_IBRIGHTNESS_130NT,
	HMT_IBRIGHTNESS_MAX
};

static const int hmt_index_brightness_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] = {
	{
		80,	/* IBRIGHTNESS_80NT */
		95,	/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		350,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	}, {
		80,	/* IBRIGHTNESS_80NT */
		95,	/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		350,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	}
};

static const int hmt_brightness_base_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] = {
	{
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	}, {
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	}
};

static const int *hmt_gamma_curve_tables[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] = {
	{
		gamma_curve_2p15_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_130NT */
	}, {
		gamma_curve_2p15_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_130NT */
	}
};

static const unsigned char hmt_aor_cmd[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX][15] = {
	{
		{0xB2, 0x03, 0x8C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_80NT */
		{0xB2, 0x03, 0x8C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_95NT */
		{0xB2, 0x03, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_115NT */
		{0xB2, 0x03, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_130NT */
	}, {
		{0xB2, 0x07, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_80NT */
		{0xB2, 0x07, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_95NT */
		{0xB2, 0x06, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_115NT */
		{0xB2, 0x06, 0x0C, 0x00, 0x06, 0x00, 0x01, 0x06, 0x06, 0x06, 0x00, 0x18, 0xFF, 0xFF, 0xFF}, /* IBRIGHTNESS_130NT */
	}
};

static const int hmt_offset_gradation[HMT_SCAN_MAX][IBRIGHTNESS_MAX][IV_MAX] = {
	/* V0 ~ V255 */
	{
		{0, 4, 9, 8, 9, 9, 8, 10, 8, 0},
		{0, 4, 9, 7, 7, 7, 8, 8, 6, 0},
		{0, 3, 7, 6, 7, 6, 6, 8, 6, 0},
		{0, 3, 6, 5, 5, 7, 7, 8, 5, 0}
	}, {
		{0, 6, 16, 13, 14, 14, 11, 11, 7, 0},
		{0, 5, 14, 11, 10, 11, 11, 11, 7, 0},
		{0, 5, 11, 9, 9, 9, 9, 11, 6, 0},
		{0, 5, 9, 8, 7, 9, 9, 9, 4, 0}
	}
};

static const int hmt_offset_color[HMT_SCAN_MAX][IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* V0 ~ V255 */
	{
		{0, 0, 0, 0, 0, 0, -8, 2, -25, -3, 1, -1, -3, -1, -6, -2, -1, -4, 0, 1, -1, 0, 1, -1, 0, -1, 1, -9, -8, -11},
		{0, 0, 0, 0, 0, 0, -11, 0, -24, -3, 1, -3, -2, -1, -5, 1, 1, -2, -1, 0, -2, -1, 1, -1, 2, 0, 1, -3, -1, -2},
		{0, 0, 0, 0, 0, 0, -13, -3, -24, 1, 4, 1, -5, -4, -7, -2, 0, -3, -1, 0, -2, 1, 1, 0, 2, -1, 1, -5, -4, -6},
		{0, 0, 0, 0, 0, 0, -14, -4, -6, 1, 3, -1, 1, 2, -1, 0, 1, -2, -1, -1, -2, 0, 1, 0, 1, -1, 1, -4, -3, -4}
	}, {
		{0, 0, 0, 0, 0, 0, -12, 2, -20, 3, 3, 0, -3, -1, -7, -4, -2, -9, 0, 0, -3, -1, 1, -2, 1, -1, 1, -10, -8, -12},
		{0, 0, 0, 0, 0, 0, 2, 2, -20, 0, 0, -4, 2, 3, -3, 1, 1, -5, 0, 0, -2, -1, 0, -2, 1, 0, 2, -5, -3, -6},
		{0, 0, 0, 0, 0, 0, -1, 0, -23, 4, 4, 0, -1, 0, -5, 1, 1, -3, 0, 1, -2, -1, -1, -2, 1, -1, 1, -5, -4, -7},
		{0, 0, 0, 0, 0, 0, 3, 6, -15, 3, 3, -2, 3, 4, -1, 2, 2, -3, 0, 0, -2, -1, 0, -2, 1, 0, 2, -4, -3, -5}
	}
};
#endif

#endif /* __DYNAMIC_AID_XXXX_H */
