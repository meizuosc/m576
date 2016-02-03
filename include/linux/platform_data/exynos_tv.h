#ifndef _EXYNOS_TVOUT_H
#define _EXYNOS_TVOUT_H

/*
 * This IP version is used for TVOUT drivers.
 * There is no driver of pre-IP_VER_TV_5S version
 * in kernel 3.10, however I left for maintenance.
 */
enum tv_ip_version {
	IP_VER_TV_5G_1,
	IP_VER_TV_5A_0,
	IP_VER_TV_5A_1,
	IP_VER_TV_5S,
	IP_VER_TV_5H,
	IP_VER_TV_5S2,
	IP_VER_TV_5HP,
	IP_VER_TV_7I,
};

#endif /* _EXYNOS_TVOUT_H */
