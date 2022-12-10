#pragma once

#define CONFIG_VER 0x00000003

#define CONFIG_SYSCLOCK 114.0 /* MHz */

#define CONFIG_SERIAL_BAUD 111816

extern char wifi_ssid[64];
extern char wifi_psk[64];
extern uint32_t wifi_auth;

extern char tftp_server_ip[64];

extern int configured;

extern void writeConfig();
extern void readConfig();
extern void configWizard();