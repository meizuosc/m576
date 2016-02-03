/*
 * Copyright (C) 2012 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINK_PM_H__
#define __LINK_PM_H__

struct link_pm_svc {
	void *owner;

	/*
	The spin-lock of the owner (mandatory)
	*/
	spinlock_t *lock;

	/*
	Whether or not PM operations (suspend, resume, etc.) are performed by
	a physical link driver
	*/
	bool dev_pm_ops;

	/* Call-back functions registered by a modem interface driver */
	void (*reset_cb)(void *owner);
	void (*mount_cb)(void *owner);
	void (*unmount_cb)(void *owner);
	void (*suspend_cb)(void *owner);
	void (*resume_cb)(void *owner);
	void (*error_cb)(void *owner);

	/*
	Optional service functions provided by a physical link driver
	- If $lock exists, these methods must be called in the critical region
	  with the $lock
	*/
	void (*lock_link)(void *owner);
	void (*unlock_link)(void *owner);
};

static inline void pm_svc_set_owner(struct link_pm_svc *svc, void *owner)
{
	svc->owner = owner;
}

static inline void pm_svc_import_lock(struct link_pm_svc *svc,
				      spinlock_t *lock)
{
	svc->lock = lock;
}

#endif
