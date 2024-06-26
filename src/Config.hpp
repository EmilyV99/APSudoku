#pragma once

#include "Main.hpp"
#include "Util.hpp"

enum Config
{
	CFG_ROOT,
	CFG_THEME,
	NUM_CFGS
};
extern ALLEGRO_CONFIG* configs[NUM_CFGS];
void set_cfg(Config c);

void load_cfg();
void load_cfg(Config c);
void save_cfg();
void save_cfg(Config c);

struct ConfigStash
{
	Config val;
	ConfigStash();
	~ConfigStash();
};

void set_config_bool(char const* sec, char const* key, bool val);
optional<bool> get_config_bool(char const* sec, char const* key);

void set_config_int(char const* sec, char const* key, int val);
optional<int> get_config_int(char const* sec, char const* key);

void set_config_dbl(char const* sec, char const* key, double val);
optional<double> get_config_dbl(char const* sec, char const* key);

void set_config_str(char const* sec, char const* key, char const* val);
void set_config_str(char const* sec, char const* key, string const& val);
optional<string> get_config_str(char const* sec, char const* key);

void set_config_hex(char const* sec, char const* key, u32 val);
optional<u32> get_config_hex(char const* sec, char const* key);

void set_config_col(char const* sec, char const* key, ALLEGRO_COLOR const& val);
optional<ALLEGRO_COLOR> get_config_col(char const* sec, char const* key);

void add_config_comment(char const* sec, char const* comment);
void add_config_section(char const* sec);
