#pragma once

#include "Main.hpp"

#define ITEMFL_NONE        0x00
#define ITEMFL_PROGRESSION 0x01
#define ITEMFL_IMPORTANT   0x02
#define ITEMFL_TRAP        0x04

bool process_remote_deaths();
bool do_ap_death(string cause);
void do_ap_disconnect();
void do_ap_connect(string const& ip, string const& port,
	string const& slot, string const& pwd, optional<int> deathlink);
bool ap_connected();
bool ap_deathlink();

void grant_hint();
string ap_get_playername(int playerid);
string ap_get_itemname(int playerid, int itemid);
optional<string> ap_get_itemflagstr(int itemflags);
string ap_get_locationname(int locid);
