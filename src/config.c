#include <string.h>
#include <stdio.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "config.h"
#include "console.h"

const uint8_t *flash_config_contents = (const uint8_t *)(XIP_BASE + FLASH_CONFIG_OFFSET);

config_t cfg;

void config_init()
{
    memcpy(&cfg, flash_config_contents, sizeof(config_t));

    if (cfg.magic != CONFIG_MAGIC || cfg.version != CONFIG_VERSION)
    {
        cprintf("[TC] Config invalid, loading defaults\n");

        memset(&cfg, 0, sizeof(config_t));
        cfg.magic = CONFIG_MAGIC;
        cfg.version = CONFIG_VERSION;

        strcpy(cfg.ssid, "SMSdoor");
        strcpy(cfg.pass, "12345678");

        strcpy(cfg.sim_pin, "0000");
        cfg.close_time = CLOSE_DISABLED;

        cfg.duration_shutter = 180;
        cfg.duration_overhead = 1;

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
}

void config_set_ssid(const char *ssid)
{
    strncpy(cfg.ssid, ssid, WIFI_SSID_SIZE - 1);
    cfg.ssid[WIFI_SSID_SIZE - 1] = 0;
}

void config_set_pass(const char *pass)
{
    strncpy(cfg.pass, pass, WIFI_PASS_SIZE - 1);
    cfg.pass[WIFI_PASS_SIZE - 1] = 0;
}
