/*
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
