/*
 * NOAA Weather decoder Allstar Repeater Bridge
 * Copyright (C) 2026 Scott Gillins W2KP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * config.h - INI configuration file parser
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

/*
 * Parse configuration file into config structure.
 * Returns 0 on success, -1 on error.
 */
int config_load(config_t *cfg, const char *path);

/*
 * Print configuration to stdout (for debugging).
 */
void config_dump(const config_t *cfg);

#endif /* CONFIG_H */
