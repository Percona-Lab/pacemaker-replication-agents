/*
 * Copyright (C) 2010-2011 Red Hat, Inc.  All rights reserved.
 * (This code is borrowed from the sanlock project which is hosted on 
 * fedorahosted.org.)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LOG_H
#define _LOG_H

#include <heartbeat/glue_config.h>
#include <clplumbing/cl_log.h>

#define log_debug(fmt, args...)		cl_log(LOG_DEBUG, fmt, ##args)
#define log_info(fmt, args...)		cl_log(LOG_INFO, fmt, ##args)
#define log_error(fmt, args...)		cl_log(LOG_ERR, fmt, ##args)

#endif /* _LOG_H */
