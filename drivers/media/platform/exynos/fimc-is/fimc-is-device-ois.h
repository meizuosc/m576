/*
 * Samsung Exynos5 SoC series FIMC-IS OIS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct fimc_is_device_ois {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	unsigned long					state;
	struct exynos_platform_fimc_is_sensor		*pdata;
	struct i2c_client			*client;
	int ois_en;
	bool ois_hsi2c_status;
};

struct ois_i2c_platform_data {
	int ois_en;
};

struct fimc_is_ois_exif {
	int error_data;
	int status_data;
};

int fimc_is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data);
int fimc_is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data);
void fimc_is_ois_enable(struct fimc_is_core *core);
void fimc_is_ois_offset_test(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y);
int fimc_is_ois_self_test(struct fimc_is_core *core);
int fimc_is_ois_gpio_on(struct fimc_is_device_companion *device);
int fimc_is_ois_gpio_off(struct fimc_is_device_companion *device);
void fimc_is_ois_fw_update(struct fimc_is_core *core);
void fimc_is_ois_fw_version(struct fimc_is_core *core);
int fimc_is_ois_get_module_version(struct fimc_is_ois_info **minfo);
int fimc_is_ois_get_phone_version(struct fimc_is_ois_info **minfo);
int fimc_is_ois_get_user_version(struct fimc_is_ois_info **uinfo);
void fimc_is_ois_get_offset_data(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y);
void fimc_is_ois_check_fw(struct fimc_is_core *core);
bool fimc_is_ois_diff_test(struct fimc_is_core *core, int *x_diff, int *y_diff);
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
void fimc_is_ois_init_thread(struct fimc_is_core *core);
#endif
bool fimc_is_ois_read_userdata(struct fimc_is_core *core);
void fimc_is_ois_exif_data(struct fimc_is_core *core);
int fimc_is_ois_get_exif_data(struct fimc_is_ois_exif **exif_info);