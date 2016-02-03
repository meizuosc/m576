#ifndef __DECON_MIC_H__
#define __DECON_MIC_H__

struct mic_config {
	u32 sysreg1;
	u32 sysreg2;
};

enum mic_on_off {
	DECON_MIC_OFF = 0,
	DECON_MIC_ON = 1
};

struct decon_mic {
	struct device *dev;
	void __iomem *reg_base;
	struct decon_lcd *lcd;
	struct mic_config *mic_config;
	bool decon_mic_on;
};

#endif
