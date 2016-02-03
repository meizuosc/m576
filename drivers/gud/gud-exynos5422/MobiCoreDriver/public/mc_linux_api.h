/*
 * Interface to be used by other Linux drivers.
 *
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_LINUX_API_H_
#define _MC_LINUX_API_H_

/*
 * Switch tbase active core to core_num, defined as linux
 * core id
 */
int mc_switch_core(uint32_t core_num);

/*
 * Return tbase active core as Linux core id
 */
uint32_t mc_active_core(void);

#endif /* _MC_LINUX_API_H_ */
