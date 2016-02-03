#ifndef FIMC_IS_DEVICE_CSI_H
#define FIMC_IS_DEVICE_CSI_H

#include <media/v4l2-device.h>
#include "fimc-is-type.h"

struct fimc_is_device_csi {
	/* channel information */
	u32				instance;
	u32 __iomem			*base_reg;

	/* for settle time */
	u32				sensor_cfgs;
	struct fimc_is_sensor_cfg	*sensor_cfg;

	/* for vci setting */
	u32				vcis;
	struct fimc_is_vci		*vci;

	/* image configuration */
	u32				mode;
	u32				lanes;
	struct fimc_is_image		image;
	/* pointer address from device sensor */
	struct v4l2_subdev		**subdev;
};

int __must_check fimc_is_csi_probe(void *parent, u32 instance);
int __must_check fimc_is_csi_open(struct v4l2_subdev *subdev);
int __must_check fimc_is_csi_close(struct v4l2_subdev *subdev);

#endif
