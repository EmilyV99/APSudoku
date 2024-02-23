#include "Main.hpp"
#include "GUI.hpp"
#include "Network.hpp"
#include "Config.hpp"
#include "../Archipelago.h"
#include <thread>

static Json::Reader reader;
deque<string> pending_deaths;

extern shared_ptr<Label> connect_error, hints_left;
std::jthread* hint_thread = nullptr;

map<int,AP_NetworkItem> missing_progressive, missing_basic;

static bool wait_conn()
{
	return !ap_connected();
}
static void ignore_loc_callback(vector<AP_NetworkItem> vec)
{
	AP_SetLocationInfoCallback(nullptr);
}


static size_t hint_count()
{
	return missing_basic.size()+missing_progressive.size();
}
static void update_hint_str()
{
	hints_left->text = std::format("Hints Remaining: {}",hint_count());
}
static void check_location(int loc)
{
	missing_progressive.erase(loc);
	missing_basic.erase(loc);
}
static void on_location_checked(int loc)
{
	if(hint_count() == 0)
		return;
	check_location(loc);
	update_hint_str();
}
static void read_hint_data(bool popup)
{
	log("Loading hint data from server...", true);
	AP_GetServerDataRequest req;
	req.key = format("_read_hints_{}_{}", AP_GetPlayerTeam(), AP_GetPlayerID());
	req.type = AP_DataType::Raw;
	req.value = new string();
	AP_GetServerData(&req);
	if(popup)
	{
		optional<u8> ret;
		bool running = true;
		Dialog popup;
		popups.emplace_back(&popup);
		generate_popup(popup, ret, running, "Please Wait", "Loading hint data...", {});
		std::function<bool()> req_proc = [&req]()
			{
				if(req.status == AP_RequestStatus::Pending && ap_connected())
					return true;
				return false;
			};
		popup.run_proc = req_proc;
		popup.run_loop();
		popups.pop_back();
	}
	else
	{
		while(req.status == AP_RequestStatus::Pending && ap_connected())
			al_rest(1);
	}
	if(!ap_connected())
	{
		log("...cancelled.", true);
		return;
	}
	log("...loaded, parsing...", true);

	string slotgame = AP_GetSlotGame();
	string const& val = *((string*)(req.value));
	Json::Value data;
	reader.parse(val, data);
	for(Json::Value& v : data)
		check_location(v["location"].asInt64());
	log("...parsed, deleting req.value...", true);
	delete (string*)req.value;
}
static void threaded_hint_data()
{
	read_hint_data(false);
	update_hint_str();
}
static void on_ap_log(string const& str)
{
	clog("APCpp", str, build_ccode(LOG_FG_CYAN));
}
static void on_ap_err(string const& str)
{
	error("APCpp", str);
}
static void on_item_clear()
{
	missing_progressive.clear();
	missing_basic.clear();
}
static void on_connect_err(string err)
{
	connect_error->text = err;
}
static void on_connected()
{
	set<i64> const& locs = AP_GetMissingLocations();
	AP_SetLocationInfoCallback([](vector<AP_NetworkItem> vec)
		{
			for(AP_NetworkItem const& itm : vec)
			{
				auto* dest = &missing_basic;
				if(itm.flags & ITEMFL_PROGRESSION)
					dest = &missing_progressive;
				(*dest)[itm.location] = itm;
			}
			AP_SetLocationInfoCallback(nullptr);
			if(hint_thread)
				delete hint_thread;
			hint_thread = new std::jthread(threaded_hint_data);
		});
	AP_SendLocationScouts(locs,0);
}
static void on_item_receive(i64,bool){}
static void on_death_recv(string src, string cause)
{
	string death_str = format("{} died to {}, bringing you down with them!",src,cause);
	log("DeathLink",death_str);
	pending_deaths.push_back(death_str);
}

bool process_remote_deaths()
{
	static bool mid_processing = false;
	if(mid_processing)
		return false;
	if(!ap_deathlink())
	{
		pending_deaths.clear();
		return false;
	}
	if(pending_deaths.empty())
		return false;
	mid_processing = true;
	while(!pending_deaths.empty())
	{
		string const& str = pending_deaths.front();
		pop_inf("DeathLink", str);
		pending_deaths.pop_front();
	}
	mid_processing = false;
	return true;
}

bool do_ap_death(string cause)
{
	bool ret = false;
	if(AP_DeathLinkSend(cause))
	{
		log("DeathLink", format("Sent death from: You {}", cause));
		pop_inf("You Died!", format("You perished because:\nYou {}", cause));
		ret = true;
	}
	return ret;
}
void do_ap_disconnect()
{
	AP_Disconnect();
}
void do_ap_connect(string const& _ip, string const& _port,
	string const& slot, string const& pwd, optional<int> deathlink)
{
	if(get_config_bool("Archipelago", "do_cache_login").value_or(false))
	{
		set_config_str("Archipelago", "cached_ip", _ip);
		set_config_str("Archipelago", "cached_port", _port);
		set_config_str("Archipelago", "cached_slot", slot);
		if(get_config_bool("Archipelago", "do_cache_pwd").value_or(false))
			set_config_str("Archipelago", "cached_pwd", pwd);
		save_cfg(CFG_ROOT);
	}
	string& errtxt = connect_error->text;
	string port = _port.empty() ? "38281" : _port;
	string ip = _ip.empty() ? "archipelago.gg" : _ip;
	if(port.size() != 5 || port.find_first_not_of("0123456789") != string::npos)
	{
		errtxt = format("Port '{}' is invalid!", port);
		return;
	}
	if(slot.empty())
	{
		errtxt = "Slot is required!";
		return;
	}

	log(format("Connecting: '{}:{}', '{}', '{}'", ip,port,slot,pwd));
	AP_SetDeathLinkAlias(slot + "_APSudoku");
	AP_SetLoggingCallback(on_ap_log);
	AP_SetLoggingErrorCallback(on_ap_err);

	string ip_port = format("{}:{}",ip,port);
	AP_Init(ip_port.c_str(), "", slot.c_str(), pwd.c_str());
	AP_SetDeathLinkForced(deathlink.has_value());
	AP_SetDeathAmnestyForced(deathlink.value_or(0));
	set<string> tags = {"Tracker","HintGame"};
	AP_SetTags(tags);

	log(format("\t with tags: {}", set_string(AP_GetTags())));

	AP_SetItemClearCallback(on_item_clear);
	AP_SetItemRecvCallback(on_item_receive);
	AP_SetLocationCheckedCallback(on_location_checked);
	AP_SetDeathLinkRecvCallback(on_death_recv);
	AP_SetLocationInfoCallback(nullptr);
	AP_SetConnectedCallback(on_connected);
	AP_OnConnectError(on_connect_err);
	AP_Start();
	errtxt.clear();
	hints_left->text = "Calculating remaining hint count...";
}

bool ap_connected()
{
	return AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated;
}
bool ap_deathlink()
{
	return ap_connected() && AP_GetTags().contains("DeathLink");
}

void grant_hint()
{
	if(hint_count() == 0)
	{
		log("No hints left to earn!");
		pop_inf("Hinted Out!","Nothing left to hint for this slot!");
		return;
	}
	read_hint_data(true); //check for any `!hint` uses or etc to prevent duplicate hinting
	if(hint_count() == 0)
	{
		log("No hints left to earn!");
		pop_inf("Hinted Out!","Nothing left to hint for this slot!");
		return;
	}

	log("attempting to unlock hint...", true);
	int basic = 0, prog = 0;
	switch(diff)
	{
		case DIFF_EASY:
			basic = 90;
			prog = 10;
			break;
		case DIFF_NORMAL:
			basic = 60;
			prog = 40;
			break;
		case DIFF_HARD:
			basic = 20;
			prog = 80;
			break;
	}
	if(missing_basic.empty())
		basic = 0;
	if(missing_progressive.empty())
		prog = 0;
	u64 val = rand(basic+prog);
	map<int,AP_NetworkItem>& valid_locations =
		val < basic ? missing_basic : missing_progressive;
	u64 indx = rand(valid_locations.size());
	auto it = valid_locations.begin();
	std::advance(it, indx);
	auto [loc,item] = *it;
	valid_locations.erase(loc);
	AP_SetLocationInfoCallback(ignore_loc_callback);
	AP_SendLocationScouts({loc},2);
	Hint h(item);
	clog(h,build_ccode(LOG_FG_YELLOW));
	pop_inf("Hint Earned:",
		std::format("{}\n{} hints remaining to earn.", string(h), hint_count()),
		CANVAS_W*0.8);
	update_hint_str();
	log("Exiting 'grant_hint()'", true);
}


string ap_get_playername(int playerid)
{
	if(playerid == AP_GetPlayerID())
		return "You";
	auto& pmap = AP_GetPlayerMap();
	auto it = pmap.find(playerid);
	if(it != pmap.end())
		return it->second.name_or_alias();
	return "";
}
string ap_get_itemname(int itemid)
{
	for(auto [p,id] : AP_GetItemMap())
	{
		auto [s,v] = p;
		if(v != itemid)
			continue;
		return id;
	}
	error(std::format("Itemname lookup failed for item ID {}",itemid));
	return "";
}
optional<string> ap_get_itemflagstr(int itemflags)
{
	if(itemflags)
	{
		stringstream ret;
		ret << "(";
		if(itemflags & ITEMFL_PROGRESSION)
			ret << "Progression,";
		if(itemflags & ITEMFL_IMPORTANT)
			ret << "Useful,";
		if(itemflags & ITEMFL_TRAP)
			ret << "Trap,";
		ret.seekp(-1,ret.cur);
		ret << ")";
		return ret.str();
	}
	else return nullopt;
}
string ap_get_locationname(int locid)
{
	string slotgame = AP_GetSlotGame();
	for(auto [p,id] : AP_GetLocationMap())
	{
		auto [s,v] = p;
		if(v != locid)
			continue;
		if(s != slotgame)
			continue;
		return id;
	}
	return "";
}

