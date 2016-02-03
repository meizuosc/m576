/*
 * drivers/media/m2m1shot-helper.c
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * Contact: Cho KyongHo <pullip.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <linux/kernel.h>
#include <linux/exynos_iovmm.h>
#include <linux/exynos_ion.h>

#include <media/m2m1shot-helper.h>

int m2m1shot_map_dma_buf(struct device *dev,
			struct m2m1shot_buffer_plane_dma *plane,
			enum dma_data_direction dir)
{
	if (plane->dmabuf) {
		plane->sgt = dma_buf_map_attachment(plane->attachment, dir);
		if (IS_ERR(plane->sgt)) {
			dev_err(dev, "%s: failed to map attacment of dma_buf\n",
					__func__);
			return PTR_ERR(plane->sgt);
		}

		exynos_ion_sync_dmabuf_for_device(dev, plane->dmabuf,
							plane->bytes_used, dir);
	} else { /* userptr */
		exynos_ion_sync_sg_for_device(dev, plane->bytes_used,
							plane->sgt, dir);
	}

	return 0;
}
EXPORT_SYMBOL(m2m1shot_map_dma_buf);

void m2m1shot_unmap_dma_buf(struct device *dev,
			struct m2m1shot_buffer_plane_dma *plane,
			enum dma_data_direction dir)
{
	if (plane->dmabuf) {
		exynos_ion_sync_dmabuf_for_cpu(dev, plane->dmabuf,
							plane->bytes_used, dir);
		dma_buf_unmap_attachment(plane->attachment, plane->sgt, dir);
	} else {
		exynos_ion_sync_sg_for_cpu(dev, plane->bytes_used,
							plane->sgt, dir);
	}
}
EXPORT_SYMBOL(m2m1shot_unmap_dma_buf);

int m2m1shot_dma_addr_map(struct device *dev,
			struct m2m1shot_buffer_dma *buf,
			int plane_idx, enum dma_data_direction dir)
{
	struct m2m1shot_buffer_plane_dma *plane = &buf->plane[plane_idx];
	dma_addr_t iova;

	if (plane->dmabuf) {
		iova = ion_iovmm_map(plane->attachment, 0,
					plane->bytes_used, dir, plane_idx);
	} else {
		iova = iovmm_map(dev, plane->sgt->sgl, 0,
					plane->bytes_used, dir, plane_idx);
	}

	if (IS_ERR_VALUE(iova))
		return (int)iova;

	buf->plane[plane_idx].dma_addr = iova + plane->offset;;

	return 0;
}

void m2m1shot_dma_addr_unmap(struct device *dev,
			struct m2m1shot_buffer_dma *buf, int plane_idx)
{
	struct m2m1shot_buffer_plane_dma *plane = &buf->plane[plane_idx];
	
	plane->dma_addr = plane->dma_addr-plane->offset;
	if (plane->dmabuf)
		ion_iovmm_unmap(plane->attachment, plane->dma_addr);
	else
		iovmm_unmap(dev, plane->dma_addr);

	plane->dma_addr = 0;
}
