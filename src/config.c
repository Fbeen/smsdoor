#include <string.h>
#include <stdio.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "config.h"

const uint8_t *flash_config_contents = (const uint8_t *)(XIP_BASE + FLASH_CONFIG_OFFSET);

config_t cfg;

void config_init()
{
    memcpy(&cfg, flash_config_contents, sizeof(config_t));

    if (cfg.magic != CONFIG_MAGIC || cfg.version != CONFIG_VERSION)
    {
        printf("Config invalid, loading defaults\n");

        memset(&cfg, 0, sizeof(config_t));
        cfg.magic = CONFIG_MAGIC;
        cfg.version = CONFIG_VERSION;

        strcpy(cfg.sim_pin, "0000");
        cfg.close_time = CLOSE_DISABLED;

        config_save(cfg);
    }
}

void config_save()
{
    uint32_t ints = save_and_disable_interrupts();

    flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_OFFSET,
                        (uint8_t *)&cfg,
                        sizeof(config_t));

    restore_interrupts(ints);
}

void config_set_pin(const char *pin)
{
    strncpy(cfg.sim_pin, pin, SIM_PIN_SIZE - 1);
    cfg.sim_pin[SIM_PIN_SIZE - 1] = 0;

    config_save();
}
