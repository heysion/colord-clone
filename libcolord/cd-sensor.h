/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_SENSOR_H
#define __CD_SENSOR_H

#include <glib-object.h>
#include <gio/gio.h>

#include <libcolord/cd-enum.h>
#include <libcolord/cd-color.h>

G_BEGIN_DECLS

#define CD_TYPE_SENSOR		(cd_sensor_get_type ())
#define CD_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_SENSOR, CdSensor))
#define CD_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_SENSOR, CdSensorClass))
#define CD_IS_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_SENSOR))
#define CD_IS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_SENSOR))
#define CD_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_SENSOR, CdSensorClass))
#define CD_SENSOR_ERROR		(cd_sensor_error_quark ())
#define CD_SENSOR_TYPE_ERROR	(cd_sensor_error_get_type ())

typedef struct _CdSensorPrivate CdSensorPrivate;

typedef struct
{
	 GObject		 parent;
	 CdSensorPrivate	*priv;
} CdSensor;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*button_pressed)	(CdSensor	*sensor);
	/*< private >*/
	/* Padding for future expansion */
	void (*_cd_sensor_reserved1) (void);
	void (*_cd_sensor_reserved2) (void);
	void (*_cd_sensor_reserved3) (void);
	void (*_cd_sensor_reserved4) (void);
	void (*_cd_sensor_reserved5) (void);
	void (*_cd_sensor_reserved6) (void);
	void (*_cd_sensor_reserved7) (void);
	void (*_cd_sensor_reserved8) (void);
} CdSensorClass;

/**
 * CdSensorError:
 * @CD_SENSOR_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	CD_SENSOR_ERROR_FAILED,
	CD_SENSOR_ERROR_LAST
} CdSensorError;

GType		 cd_sensor_get_type			(void);
GQuark		 cd_sensor_error_quark			(void);
CdSensor	*cd_sensor_new				(void);
gchar		*cd_sensor_to_string			(CdSensor	*sensor);
gboolean	 cd_sensor_set_object_path_sync		(CdSensor	*sensor,
							 const gchar	*object_path,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_sensor_lock_sync			(CdSensor	*sensor,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_sensor_unlock_sync			(CdSensor	*sensor,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_sensor_get_sample_sync		(CdSensor	*sensor,
							 CdSensorCap	 cap,
							 CdColorXYZ	*values,
							 gdouble	*ambient,
							 GCancellable	*cancellable,
							 GError		**error);
CdSensorKind	 cd_sensor_get_kind			(CdSensor	*sensor);
CdSensorState	 cd_sensor_get_state			(CdSensor	*sensor);
const gchar	*cd_sensor_get_serial			(CdSensor	*sensor);
const gchar	*cd_sensor_get_model			(CdSensor	*sensor);
const gchar	*cd_sensor_get_vendor			(CdSensor	*sensor);
gboolean	 cd_sensor_get_native			(CdSensor	*sensor);
gboolean	 cd_sensor_get_locked			(CdSensor	*sensor);
guint		 cd_sensor_get_caps			(CdSensor	*sensor);
gboolean	 cd_sensor_has_cap			(CdSensor	*sensor,
							 CdSensorCap	 cap);
const gchar	*cd_sensor_get_object_path		(CdSensor	*sensor);

G_END_DECLS

#endif /* __CD_SENSOR_H */
