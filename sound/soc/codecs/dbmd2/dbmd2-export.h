/*
 * DSPG DBMD2 codec driver
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DBMD2_EXPORT_H
#define DBMD2_EXPORT_H

int dbmd2_remote_add_codec_controls(struct snd_soc_codec *codec);

typedef void (*event_cb)(int);
void dbmd2_remote_register_event_callback(event_cb func);

#endif
