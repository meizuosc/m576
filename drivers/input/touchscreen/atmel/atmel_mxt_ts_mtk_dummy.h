#ifndef __LINUX_ATMEL_MXT_TS_DUMMY_H
#define __LINUX_ATMEL_MXT_TS_DUMMY_H

#include <linux/types.h>

#define I2C_RS_FLAG (1<<0)
#define I2C_ENEXT_FLAG (1<<0)
#define I2C_DMA_FLAG (1<<0)

static int tpd_load_status;
static int tpd_type_cap;

struct tpd_driver_t{
    char *tpd_device_name;
    int (*tpd_local_init)(void);
#if defined(CONFIG_HAS_EARLYSUSPEND)    
    void (*suspend)(struct early_suspend *);
    void (*resume)(struct early_suspend *);
#endif
    int tpd_have_button;
};

inline int tpd_driver_add(struct tpd_driver_t *t)
{
    return 0;
}

inline void tpd_driver_remove(struct tpd_driver_t *t)
{
}


#endif /* __LINUX_ATMEL_MXT_TS_DUMMY_H */
