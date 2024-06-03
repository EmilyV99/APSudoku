#pragma once

#ifdef _WIN32
#include <format>
using std::format;
#else
#include <fmt/format.h>
using fmt::format;
#endif

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <map>
#include <vector>
#include <deque>
#include <set>
#include <string>
#include <optional>
#include <functional>
#include <memory>
#include <tuple>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int uint;
using std::pair;
using std::tuple;
using std::string;
using std::stringstream;
using std::set;
using std::map;
using std::vector;
using std::deque;
using std::to_string;
using std::optional;
using std::nullopt;
using std::shared_ptr;
using std::make_shared;

extern std::mt19937 rng;
u64 rand(u64 range);
u64 rand(u64 min, u64 max);

struct FontDef;
struct GUIObject;
struct InputObject;
struct DrawContainer;
struct AP_NetworkItem;

extern ALLEGRO_DISPLAY* display;

extern vector<DrawContainer*> popups;
extern u64 cur_frame;
extern bool shape_mode, thicker_borders, show_invalid, verbose_log;

void dlg_draw();
void dlg_render();
void run_events(bool& redraw);
bool events_empty();
void on_resize();

extern volatile bool program_running;

struct Hint
{
	string entrance;
	int finding_player, receiving_player;
	bool found;
	int item, location;
	int item_flags;
	operator string() const;
	Hint(Json::Value const& v);
	Hint(AP_NetworkItem const& itm);
};

enum Screen
{
	SCR_SUDOKU,
	SCR_CONNECT,
	SCR_SETTINGS,
	NUM_SCRS
};
enum EntryMode
{
	ENT_ANSWER,
	ENT_CENTER,
	ENT_CORNER,
	NUM_ENT
};
extern EntryMode mode;
EntryMode get_mode();
bool mode_mod();

enum Difficulty
{
	DIFF_EASY,
	DIFF_NORMAL,
	DIFF_HARD,
	DIFF_KILLER,
	NUM_DIFF
};
extern Difficulty diff;

#define CANVAS_W 640
#define CANVAS_H 352

enum direction
{
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
	DIR_UPLEFT,
	DIR_UPRIGHT,
	DIR_DOWNLEFT,
	DIR_DOWNRIGHT,
	NUM_DIRS
};

enum CCFG
{
	LOG_FG_BLACK = 30,
	LOG_FG_RED,
	LOG_FG_GREEN,
	LOG_FG_YELLOW,
	LOG_FG_BLUE,
	LOG_FG_PURPLE,
	LOG_FG_CYAN,
	LOG_FG_WHITE,
	LOG_FG_B_BLACK = 90,
	LOG_FG_B_RED,
	LOG_FG_B_GREEN,
	LOG_FG_B_YELLOW,
	LOG_FG_B_BLUE,
	LOG_FG_B_PURPLE,
	LOG_FG_B_CYAN,
	LOG_FG_B_WHITE,
};
enum CCBG
{
	LOG_BG_NONE,
	LOG_BG_BLACK = 40,
	LOG_BG_RED,
	LOG_BG_GREEN,
	LOG_BG_YELLOW,
	LOG_BG_BLUE,
	LOG_BG_PURPLE,
	LOG_BG_CYAN,
	LOG_BG_WHITE,
	LOG_BG_B_BLACK = 100,
	LOG_BG_B_RED,
	LOG_BG_B_GREEN,
	LOG_BG_B_YELLOW,
	LOG_BG_B_BLUE,
	LOG_BG_B_PURPLE,
	LOG_BG_B_CYAN,
	LOG_BG_B_WHITE,
};

string build_ccode(CCFG fg, CCBG bg = LOG_BG_NONE);
extern string default_ccode;
inline const string CCODE_REVERT = "\033[0m";
void clog(string const& hdr, string const& msg, string const& ccode, bool is_verbose = false);
void log(string const& hdr, string const& msg, bool is_verbose = false);
void error(string const& hdr, string const& msg, bool is_verbose = false);
void fail(string const& hdr, string const& msg);

void clog(string const& msg, string const& ccode, bool is_verbose = false);
void log(string const& msg, bool is_verbose = false);
void error(string const& msg, bool is_verbose = false);
void fail(string const& msg);

class sudoku_exception : public std::exception
{
public:
	virtual const char * what() const noexcept override
	{
		return msg.c_str();
	}
	sudoku_exception(string const& msg) : msg(msg)
	{}
private:
	string msg;
};
class ignore_exception : public sudoku_exception
{
public:
	ignore_exception()
		: sudoku_exception("IGNORE")
	{}
};

#include "Util.hpp"
