#ifndef ADNC_SENSORHUB_API_H
#define ADNC_SENSORHUB_API_H


struct mq100_extension_cb;

/**
 * Register rdb callbacks
 * @param  callbacks - structure containing
 * callbacks from es705 rdb/wdb driver
 * @return
 */
int mq100_register_extensions\
	(struct mq100_extension_cb *callbacks, void *priv);

/**
 * Note: can we expect all of these
 * functions to be executed in process context?
 */
struct mq100_extension_cb {
	/**
	 * cookie using for callbacks
	 */
	void *priv;

	/**
	 * Callback when firmware has been downloaded, device has been
	 * initialized and is ready for rdb/wdb
	 *
	 * @param  es705_priv - es705 private data. this cookie will be
	 * returned with all calls to es705_wdb
	 * @return on success, a pointer to the callee's private data,
	 *         this cookie must be returned with every other callback
	 *         on failure, return NULL
	 */
	void * (*probe)(void *es705);

	/**
	 * This function is called when audience driver
	 * has detected the an interrupt from the device
	 * @param priv - cookie returned from probe()
	 */
	void (*intr)(void *priv);

	/**
	 * Callback whenever the device state changes.
	 * e.g. when firmware has been downloaded
	 * Use MQ100_STATE_XXX values for state param.
	 * @param priv - cookie returned from probe()
	 */
	void (*status)(void *priv, u8 state);
};

/*
 * Writes buf to es705 using wdb
 * this function will prepend 0x802F 0xffff
 * @param: buf - wdb data
 * @param: len - length
 * @return: no. of bytes written
 */
int mq100_wdb(const void *buf, int len);

/* Max Size = PAGE_SIZE * 2 */
/*
 * Reads buf from es705 using rdb
 * @param:
 * buf - rdb data Max Size supported - 2*PAGE_SIZE
 * Ensure buffer allocated has enough space for rdb
 *
 * buf - buffer pointer
 * id - type specifier
 * len - max. buf size
 *
 * @return: no. of bytes read
 */
int mq100_rdb(void *buf, int len, int id);

#endif
