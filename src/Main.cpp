#include "../Archipelago.h"
#include <iostream>
#include <filesystem>
#include "GUI.hpp"
#include "Font.hpp"
#include "Config.hpp"
#include "SudokuGrid.hpp"
#include "Network.hpp"
#include "PuzzleGen.hpp"

string build_ccode(CCFG fg, CCBG bg)
{
	stringstream s;
	s << "\x1B[";
	if(bg != LOG_BG_NONE)
		s << bg << ';';
	s << fg << "m";
	return s.str();
}
string default_ccode = build_ccode(LOG_FG_B_PURPLE,LOG_BG_NONE);
void log(string const& hdr, string const& msg, bool is_verbose)
{
	if(verbose_log || !is_verbose)
		std::cout << default_ccode << "[LOG][" << hdr << "] " << msg << CCODE_REVERT << std::endl;
}
void clog(string const& hdr, string const& msg, string const& ccode, bool is_verbose)
{
	if(verbose_log || !is_verbose)
		std::cout << ccode << "[LOG][" << hdr << "] " << msg << CCODE_REVERT << std::endl;
}
void error(string const& hdr, string const& msg, bool is_verbose)
{
	if(verbose_log || !is_verbose)
		std::cerr << build_ccode(LOG_FG_RED) << "[ERROR][" << hdr << "] " << msg << CCODE_REVERT << std::endl;
}
void fail(string const& hdr, string const& msg)
{
	std::cerr << build_ccode(LOG_FG_RED) << "[FATAL][" << hdr << "] " << msg << CCODE_REVERT << std::endl;
	exit(1);
}
void log(string const& msg, bool is_verbose)
{
	log("APSudoku", msg, is_verbose);
}
void clog(string const& msg, string const& ccode, bool is_verbose)
{
	clog("APSudoku", msg, ccode, is_verbose);
}
void error(string const& msg, bool is_verbose)
{
	error("APSudoku", msg, is_verbose);
}
void fail(string const& msg)
{
	fail("APSudoku", msg);
}

Hint::operator string() const
{
	auto flagstr = ap_get_itemflagstr(item_flags);
	flagstr = flagstr ? (" "+*flagstr) : "";
	return format("{} {} '{}'{} for {} at '{}'",
		ap_get_playername(finding_player),
		found ? "found" : "will find",
		ap_get_itemname(item), *flagstr,
		ap_get_playername(receiving_player),
		ap_get_locationname(location));
}
Hint::Hint(Json::Value const& v)
	: entrance(v["entrance"].asString()),
	finding_player(v["finding_player"].asInt()),
	receiving_player(v["receiving_player"].asInt()),
	found(v["found"].asBool()),
	item(v["item"].asInt()),
	location(v["location"].asInt()),
	item_flags(v["item_flags"].asInt())
{}
Hint::Hint(AP_NetworkItem const& itm)
	: entrance(),
	finding_player(AP_GetPlayerID()),
	receiving_player(itm.player),
	found(false),
	item(itm.item),
	location(itm.location),
	item_flags(itm.flags)
{}

ALLEGRO_DISPLAY* display;
ALLEGRO_BITMAP* canvas;
ALLEGRO_TIMER* timer;
ALLEGRO_EVENT_QUEUE* events;
ALLEGRO_EVENT_SOURCE event_source;

std::mt19937 rng;
bool shift_center = false;

Difficulty diff = DIFF_NORMAL;
EntryMode mode = ENT_ANSWER;
EntryMode get_mode()
{
	EntryMode ret = mode;
	if(cur_input->shift())
		ret = shift_center ? ENT_CENTER : ENT_CORNER;
	if(cur_input->ctrl_cmd())
		ret = shift_center ? ENT_CORNER : ENT_CENTER;
	if(shape_mode && (ret == ENT_CENTER)) //shapes can't handle center!
		ret = ENT_CORNER;
	return ret;
}
bool mode_mod()
{
	return cur_input->shift() || cur_input->ctrl_cmd();
}

#define GRID_X (32)
#define GRID_Y (32)
shared_ptr<Sudoku::Grid> grid;
#define BUTTON_X (CANVAS_W-32-96)
#define BUTTON_Y (32)
shared_ptr<Button> swap_btns[NUM_SCRS];
shared_ptr<Button> connect_btn;
shared_ptr<RadioSet> difficulty, entry_mode;
shared_ptr<Label> connect_error, hints_left;
shared_ptr<CheckBox> deathlink_cbox;
shared_ptr<TextField> deathlink_amnesty;
shared_ptr<Column> entry_c_num;
shared_ptr<Column> entry_c_shape;
vector<shared_ptr<TextField>> ap_fields;

Screen curscr = SCR_SUDOKU;

map<Screen,DrawContainer> gui_objects;
vector<DrawContainer*> popups;

void swap_screen(Screen scr)
{
	if(curscr == scr)
		return;
	curscr = scr;
}

bool do_connect()
{
	connect_btn->flags |= FL_SELECTED;
	connect_btn->focus();
	if(grid->active())
	{
		if(!pop_yn("Forfeit", "Quit solving current puzzle?"))
		{
			connect_btn->flags &= ~FL_SELECTED;
			return false;
		}
		//do_ap_death("quit a sudoku puzzle!");//can't be connected here
		grid->clear();
	}
	string& errtxt = connect_error->text;
	do_ap_connect(ap_fields[0]->get_str(),
		ap_fields[1]->get_str(), ap_fields[2]->get_str(),
		ap_fields[3]->get_str(), deathlink_cbox->selected() ? optional<int>(deathlink_amnesty->get_int()) : nullopt);
	
	if(!errtxt.empty())
	{
		connect_btn->flags &= ~FL_SELECTED;
		return false; //failed, error handled
	}
	auto status = AP_GetConnectionStatus();
	
	optional<u8> ret;
	bool running = true;
	
	Dialog popup;
	popups.emplace_back(&popup);
	generate_popup(popup, ret, running, "Please Wait", "Connecting...", {"Cancel"});
	popup.run_proc = [&running]()
		{
			if(!running) //cancelled
			{
				do_ap_disconnect();
				return false;
			}
			switch(AP_GetConnectionStatus())
			{
				case AP_ConnectionStatus::Disconnected:
				case AP_ConnectionStatus::Connected:
					return true;
			}
			return false;
		};
	popup.run_loop();
	if(!running)
		errtxt = "Connection cancelled";
	else switch(AP_GetConnectionStatus())
	{
		case AP_ConnectionStatus::ConnectionRefused:
			errtxt = "Connection Refused: check your connection details";
			break;
	}
	popups.pop_back();
	connect_btn->flags &= ~FL_SELECTED;
	return errtxt.empty();
}

void init_grid() //for debug convenience only
{
	// for(int q = 0; q < 9; ++q)
	// {
		// grid->cells[(3+(q%3))+((3+(q/3))*9)].val = q+1;
	// }
}
void build_gui()
{
	using namespace Sudoku;
	const int GRID_SZ = CELL_SZ*9+2;
	const int GRID_X2 = GRID_X+GRID_SZ;
	const int GRID_Y2 = GRID_Y+GRID_SZ;
	const int RGRID_X = GRID_X2 + 8;
	FontDef font_l(-22, false, BOLD_NONE);
	FontDef font_m(-20, false, BOLD_NONE);
	FontDef font_s(-15, false,BOLD_NONE);
	{ // BG, to allow 'clicking off'
		shared_ptr<ShapeRect> bg = make_shared<ShapeRect>(0,0,CANVAS_W,CANVAS_H,C_BACKGROUND);
		bg->onMouse = mouse_killfocus;
		for(int q = 0; q < NUM_SCRS; ++q)
			gui_objects[static_cast<Screen>(q)].push_back(bg);
	}
	{ // Swap buttons
		#define ON_SWAP_BTN(b, scr) \
		b->sel_proc = [](GUIObject const& ref) -> bool {return curscr == scr;}; \
		b->onMouse = [](InputObject& ref,MouseEvent e) \
			{ \
				switch(e) \
				{ \
					case MOUSE_LCLICK: \
						if(curscr != scr) swap_screen(scr); \
						return MRET_TAKEFOCUS; \
				} \
				return ref.handle_ev(e); \
			}
		swap_btns[0] = make_shared<Button>("Sudoku", font_l);
		swap_btns[1] = make_shared<Button>("Archipelago", font_l);
		swap_btns[2] = make_shared<Button>("Settings", font_l);
		ON_SWAP_BTN(swap_btns[0], SCR_SUDOKU);
		ON_SWAP_BTN(swap_btns[1], SCR_CONNECT);
		ON_SWAP_BTN(swap_btns[2], SCR_SETTINGS);
		shared_ptr<Column> sb_column = make_shared<Column>(BUTTON_X, BUTTON_Y, 0, 2, ALLEGRO_ALIGN_CENTER);
		for(shared_ptr<Button> const& b : swap_btns)
			sb_column->add(b);
		
		for(int q = 0; q < NUM_SCRS; ++q)
			gui_objects[static_cast<Screen>(q)].push_back(sb_column);
	}
	{ // Grid
		grid = make_shared<Grid>(GRID_X, GRID_Y);
		grid->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LOSTFOCUS:
						if(cur_input->new_focus == entry_mode.get())
							return MRET_TAKEFOCUS;
						for(shared_ptr<GUIObject>& obj : entry_mode->cont)
							if(cur_input->new_focus == obj.get())
								return MRET_TAKEFOCUS;
						break;
				}
				return ref.handle_ev(e);
			};
		grid->onExit = [](Grid& ref)
			{
				for(auto row : entry_c_num->cont)
					for(auto b : static_pointer_cast<Row>(row)->cont)
					{
						auto btn = static_pointer_cast<Button>(b);
						btn->custom_flags &= ~0b1;
						btn->force_bg = nullopt;
						btn->force_fg = nullopt;
					}
				for(auto row : entry_c_shape->cont)
					for(auto b : static_pointer_cast<Row>(row)->cont)
					{
						auto btn = static_pointer_cast<BmpButton>(b);
						btn->custom_flags &= ~0b1;
						btn->force_bg = nullopt;
					}
			};
		gui_objects[SCR_SUDOKU].push_back(grid);
		const int HELPBTN_WID = 32, HELPBTN_HEI = 24;
		shared_ptr<Button> gridhelp = make_shared<Button>("?", font_l,
			GRID_X2-HELPBTN_WID-2, GRID_Y-HELPBTN_HEI, HELPBTN_WID, HELPBTN_HEI);
		gridhelp->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
					{
						ref.flags |= FL_SELECTED;
						pop_inf("Controls",
							format("LClick: Select (Hold, Shift, or Ctrl: Multi)"
							"\nDouble-LClick: Select Matching (Shift or Ctrl: Multi)"
							"\nRClick: Select (Shift or Ctrl: Deselect)"
							"\nWASD/Arrow Keys: Move (Shift or Ctrl: Multi)"
							"\nNumbers: Enter ({})"
							"\nTab: Cycle number entry mode"
							"\nDelete/Backspace: Clear Cell",
								shift_center ? "Shift: Center, Ctrl: Corner"
								: "Shift: Corner, Ctrl: Center"),
							CANVAS_W*0.75);
						ref.flags &= ~FL_SELECTED;
						break;
					}
				}
				return ref.handle_ev(e);
			};
		gui_objects[SCR_SUDOKU].push_back(gridhelp);
		
		shared_ptr<Label> lifecnt = make_shared<Label>("", font_l, ALLEGRO_ALIGN_LEFT);
		lifecnt->setx(GRID_X);
		lifecnt->sety(GRID_Y-lifecnt->height()-2);
		lifecnt->text_proc = [](Label& ref) -> string
			{
				return format("Lives: {}", AP_GetCurrentDeathAmnesty());
			};
		lifecnt->vis_proc = [](GUIObject const& ref) -> bool
			{
				return grid->active() && ap_deathlink();
			};
		gui_objects[SCR_SUDOKU].push_back(lifecnt);
		
		shared_ptr<Label> nogame = make_shared<Label>("", font_l, ALLEGRO_ALIGN_CENTER);
		nogame->setx(GRID_X + GRID_SZ/2);
		nogame->sety(GRID_Y-nogame->height()-2);
		nogame->text_proc = [](Label& ref) -> string
			{
				if(grid->active())
				{
					ref.type = TYPE_NORMAL;
					return format("Playing: {}", *difficulty->get_sel_text());
				}
				else
				{
					ref.type = TYPE_ERROR;
					return "No Active Game";
				}
			};
		gui_objects[SCR_SUDOKU].push_back(nogame);
		
		auto btn_clear_invalid = make_shared<Button>("Clear Invalid Marks", font_l);
		btn_clear_invalid->w = 8+btn_clear_invalid->stringw();
		btn_clear_invalid->h = 4+btn_clear_invalid->stringh();
		btn_clear_invalid->setx(GRID_X + (GRID_SZ-btn_clear_invalid->w)/2);
		btn_clear_invalid->sety(GRID_Y2);
		btn_clear_invalid->vis_proc = [](GUIObject const& ref) -> bool
			{
				return show_invalid && grid->has_invalid();
			};
		btn_clear_invalid->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
					{
						grid->clear_invalid();
						return MRET_OK;
					}
				}
				return ref.handle_ev(e);
			};
		gui_objects[SCR_SUDOKU].push_back(btn_clear_invalid);
	}
	{ // Difficulty / game buttons
		shared_ptr<Column> diff_column = make_shared<Column>(BUTTON_X,GRID_Y,0,2,ALLEGRO_ALIGN_LEFT);
		
		shared_ptr<Row> diff_row = make_shared<Row>(0,0,0,0,ALLEGRO_ALIGN_RIGHT);
		
		shared_ptr<Label> diff_lbl = make_shared<Label>("Difficulty:", font_s, ALLEGRO_ALIGN_LEFT);
		diff_row->add(diff_lbl);
		
		log(format("{}", font_s.height()));
		shared_ptr<Button> diffhelp = make_shared<Button>("?", font_l, 0, 0, CELL_SZ, font_s.pix_height());
		diffhelp->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
					{
						ref.flags |= FL_SELECTED;
						pop_inf("How to Play",
							"Fill every Row, Column, and outlined Region (3x3 box) with the digits 1-9, exactly once each."
							"\nName: # Givens, progression hint %"
							"\nEasy: 46, 10%"
							"\nNormal: 35, 40%"
							"\nHard: 26, 80%"
							"\nKiller*: 26, 60%"
							"\n* In 'Killer' mode, outlined Cages must sum to the indicated total.",
							CANVAS_W*0.75);
						ref.flags &= ~FL_SELECTED;
						break;
					}
				}
				return ref.handle_ev(e);
			};
		diff_row->add(diffhelp);
		
		diff_column->add(diff_row);
		
		difficulty = make_shared<RadioSet>(
			[](){return diff;},
			[](optional<u16> v)
			{
				if(v)
					diff = Difficulty(*v);
			},
			vector<string>({"Easy","Normal","Hard","Killer"}),
			FontDef(-15, false, BOLD_NONE));
		difficulty->select(1);
		difficulty->dis_proc = [](GUIObject const& ref) -> bool
			{
				return grid->active();
			};
		diff_column->add(difficulty);
		
		shared_ptr<Button> start_btn = make_shared<Button>("Start", font_l);
		diff_column->add(start_btn);
		start_btn->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
						if(!ap_connected())
						{
							if(!pop_yn("Start Unconnected?",
								"Start a puzzle while not connected to Archipelago?"
								"\nNo hints will be earnable."))
								return MRET_OK;
						}
						grid->exit();
						grid->generate(diff);
						break;
				}
				return ref.handle_ev(e);
			};
		start_btn->dis_proc = [](GUIObject const& ref) -> bool
			{
				return grid->active();
			};
		
		shared_ptr<Button> forfeit_btn = make_shared<Button>("Forfeit", font_l);
		diff_column->add(forfeit_btn);
		forfeit_btn->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
						if(pop_yn("Forfeit", "Quit solving this puzzle?"
								+ string(ap_deathlink()?"\nThis will count as a DeathLink death!":"")))
						{
							do_ap_death("quit a sudoku puzzle!");
							grid->clear();
						}
						break;
				}
				return ref.handle_ev(e);
			};
		forfeit_btn->dis_proc = [](GUIObject const& ref) -> bool
			{
				return !grid->active();
			};
		
		shared_ptr<Button> check_btn = make_shared<Button>("Check", font_l);
		diff_column->add(check_btn);
		check_btn->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
						if(!grid->filled())
							pop_inf("Unfinished","Not all cells are filled!");
						else if(grid->check())
						{
							if(ap_connected())
								grant_hint();
							else pop_inf("Solved","Correct!");
							grid->exit();
						}
						else
						{
							if(do_ap_death("solved a sudoku wrong!"))
								grid->exit();
							else pop_inf("Wrong", "Puzzle solution incorrect!");
						}
						break;
				}
				return ref.handle_ev(e);
			};
		check_btn->dis_proc = [](GUIObject const& ref) -> bool
			{
				return !grid->active();
			};
		diff_column->sety(GRID_Y2-diff_column->height());
		diff_column->realign();
		gui_objects[SCR_SUDOKU].push_back(diff_column);
	}
	{ // Entry mode toggle
		shared_ptr<Column> entry_col = make_shared<Column>(RGRID_X,GRID_Y,0,1,ALLEGRO_ALIGN_LEFT);
		
		shared_ptr<Label> entry_lbl = make_shared<Label>("Entry Mode:", font_s, ALLEGRO_ALIGN_LEFT);
		entry_col->add(entry_lbl);
		
		entry_mode = make_shared<RadioSet>(
			[]()
			{
				if(grid->focused())
					return get_mode();
				return mode;
			},
			[](optional<u16> v)
			{
				if(v)
					mode = EntryMode(*v);
			},
			vector<string>({"Answer","Center","Corner"}),
			font_m);
		entry_mode->select(ENT_ANSWER);
		entry_mode->dis_proc = [](GUIObject const& ref) -> bool {return grid->focused() && mode_mod();};
		entry_col->add(entry_mode);
		entry_mode->onMouse = [](InputObject& ref,MouseEvent e)
			{
				if(e == MOUSE_GOTFOCUS)
				{
					grid->focus();
					return MRET_OK;
				}
				return ref.handle_ev(e);
			};
		entry_mode->cont[1]->dis_proc = [](GUIObject const& ref) -> bool {return shape_mode;};
		
		auto shapes_check = make_shared<CheckBox>("Shapes Mode", font_s);
		if(shape_mode)
			shapes_check->flags |= FL_SELECTED;
		shapes_check->onMouse = [](InputObject& ref,MouseEvent e)
			{
				auto ret = ref.handle_ev(e);
				shape_mode = (ref.flags & FL_SELECTED);
				set_config_bool("GUI", "shape_mode", shape_mode);
				save_cfg(CFG_ROOT);
				return ret;
			};
		entry_col->add(shapes_check);
		
		auto cellborder_check = make_shared<CheckBox>("Thicker Borders", font_s);
		if(thicker_borders)
			cellborder_check->flags |= FL_SELECTED;
		cellborder_check->onMouse = [](InputObject& ref,MouseEvent e)
			{
				auto ret = ref.handle_ev(e);
				thicker_borders = (ref.flags & FL_SELECTED);
				set_config_bool("GUI", "thicker_borders", thicker_borders);
				save_cfg(CFG_ROOT);
				return ret;
			};
		entry_col->add(cellborder_check);
		
		auto showinvalid_check = make_shared<CheckBox>("Show Invalid on Check", font_s);
		if(show_invalid)
			showinvalid_check->flags |= FL_SELECTED;
		showinvalid_check->onMouse = [](InputObject& ref,MouseEvent e)
			{
				auto ret = ref.handle_ev(e);
				show_invalid = (ref.flags & FL_SELECTED);
				set_config_bool("Sudoku", "show_invalid", show_invalid);
				save_cfg(CFG_ROOT);
				return ret;
			};
		entry_col->add(showinvalid_check);
		
		gui_objects[SCR_SUDOKU].push_back(entry_col);
	}
	{ // Number Entry Buttons
		entry_c_num = make_shared<Column>(RGRID_X, GRID_Y, 0, 0, ALLEGRO_ALIGN_LEFT);
		entry_c_num->vis_proc = [](GUIObject const& ref) -> bool {return !shape_mode;};
		
		entry_c_shape = make_shared<Column>(RGRID_X, GRID_Y, 0, 0, ALLEGRO_ALIGN_LEFT);
		entry_c_shape->vis_proc = [](GUIObject const& ref) -> bool {return shape_mode;};
		
		for(int row = 0; row < 3; ++row)
		{
			shared_ptr<Row> r = make_shared<Row>(0,0,0,0,ALLEGRO_ALIGN_CENTER);
			shared_ptr<Row> rs = make_shared<Row>(0,0,0,0,ALLEGRO_ALIGN_CENTER);
			for(int col = 0; col < 3; ++col)
			{
				auto q = col+(row*3);
				shared_ptr<Button> btn = make_shared<Button>(to_string(q+1), FONT_ANSWER, 0, 0, CELL_SZ, CELL_SZ);
				shared_ptr<BmpButton> bmpbtn = make_shared<BmpButton>(shape_bmps[q], 0, 0, CELL_SZ, CELL_SZ);
				btn->onMouse = [q](InputObject& ref,MouseEvent e)
					{
						Button& btn = static_cast<Button&>(ref);
						bool marked_dis = btn.custom_flags&0b1;
						if(e == MOUSE_RCLICK)
						{
							btn.custom_flags ^= 0b1; //temp disable
							if(marked_dis)
							{
								btn.force_fg = nullopt;
								btn.force_bg = nullopt;
							}
							else
							{
								btn.force_fg = C_BUTTON_DISTXT;
								btn.force_bg = C_BUTTON_DISBG;
							}
							return MRET_OK;
						}
						
						if(e == MOUSE_LCLICK)
						{
							if(!marked_dis)
							{
								grid->enter(q+1);
								grid->focus();
							}
							return MRET_OK;
						}
						return ref.handle_ev(e);
					};
				bmpbtn->onMouse = [q](InputObject& ref,MouseEvent e)
					{
						BmpButton& btn = static_cast<BmpButton&>(ref);
						bool marked_dis = btn.custom_flags&0b1;
						if(e == MOUSE_RCLICK)
						{
							btn.custom_flags ^= 0b1; //temp disable
							if(marked_dis)
								btn.force_bg = nullopt;
							else
								btn.force_bg = C_BUTTON_DISBG;
							return MRET_OK;
						}
						
						if(e == MOUSE_LCLICK)
						{
							if(!marked_dis)
							{
								grid->enter(q+1);
								grid->focus();
							}
							return MRET_OK;
						}
						return ref.handle_ev(e);
					};
				r->add(btn);
				rs->add(bmpbtn);
			}
			entry_c_num->add(r);
			entry_c_shape->add(rs);
		}
		
		shared_ptr<Row> delrow = make_shared<Row>(0,0,0,0,ALLEGRO_ALIGN_LEFT);
		shared_ptr<Button> del = make_shared<Button>("Delete", FONT_ANSWER, 0, 0, CELL_SZ*3, CELL_SZ);
		del->onMouse = [](InputObject& ref,MouseEvent e)
			{
				if(e == MOUSE_LCLICK)
				{
					grid->enter(0);
					grid->focus();
					return MRET_OK;
				}
				return ref.handle_ev(e);
			};
		delrow->add(del);
		
		shared_ptr<Button> padhelp = make_shared<Button>("?", font_l, 0, 0, CELL_SZ, CELL_SZ);
		padhelp->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
					{
						ref.flags |= FL_SELECTED;
						pop_inf("Controls",
							"RClick: Toggle Button (useful for marking off finished numbers/shapes)"
							"\nLClick: Enter number/shape (into selected cells)"
							"\n'Delete' button: Clear contents (selected cells)",
							CANVAS_W*0.75);
						ref.flags &= ~FL_SELECTED;
						break;
					}
				}
				return ref.handle_ev(e);
			};
		delrow->add(padhelp);
		
		entry_c_num->add(delrow);
		entry_c_shape->add(delrow);
		
		entry_c_num->sety(GRID_Y2-entry_c_num->height());
		entry_c_shape->sety(entry_c_num->ypos());
		entry_c_num->realign();
		entry_c_shape->realign();
		
		gui_objects[SCR_SUDOKU].push_back(entry_c_num);
		gui_objects[SCR_SUDOKU].push_back(entry_c_shape);
	}
	{ // AP connection
		shared_ptr<Column> apc = make_shared<Column>(GRID_X, GRID_Y, 0, 8, ALLEGRO_ALIGN_RIGHT);
		apc->dis_proc = [](GUIObject const& ref) -> bool
			{
				return ap_connected();
			};
		
		vector<tuple<string,string,
			std::function<bool(string const&,string const&,char)>>> p = {
			{"IP:","archipelago.gg", [](string const& o, string const& n, char c)
				{
					return validate_alphanum(o,n,c) || c == '.';
				}},
			{"Port:","", [](string const& o, string const& n, char c)
				{
					return validate_numeric(o,n,c) && n.size() <= 5;
				}},
			{"Slot:","",nullptr},
			{"Passwd:","",nullptr}
		};
		
		if(get_config_bool("Archipelago", "do_cache_login").value_or(false))
		{
			if(auto str = get_config_str("Archipelago", "cached_ip"))
				std::get<1>(p[0]) = *str;
			if(auto str = get_config_str("Archipelago", "cached_port"))
				std::get<1>(p[1]) = *str;
			if(auto str = get_config_str("Archipelago", "cached_slot"))
				std::get<1>(p[2]) = *str;
			if(get_config_bool("Archipelago", "do_cache_pwd").value_or(false))
			{
				if(auto str = get_config_str("Archipelago", "cached_pwd"))
					std::get<1>(p[3]) = *str;
			}
		}
		
		ap_fields.clear();
		for(auto [label,defval,valproc] : p)
		{
			shared_ptr<Row> r = make_shared<Row>(0,0,0,4,ALLEGRO_ALIGN_CENTER);
			
			shared_ptr<Label> lbl = make_shared<Label>(label, font_l, ALLEGRO_ALIGN_LEFT);
			shared_ptr<TextField> field = make_shared<TextField>(
				GRID_X, GRID_Y, 256,
				defval, font_l);
			field->onValidate = valproc;
			field->onEnter = do_connect;
			r->add(lbl);
			r->add(field);
			ap_fields.push_back(field);
			apc->add(r);
		}
		if(!ap_fields.empty())
		{
			for(size_t q = 0; q < ap_fields.size()-1; ++q)
				ap_fields[q]->tab_target = ap_fields[q+1].get();
			ap_fields.back()->tab_target = ap_fields.front().get();
		}
		gui_objects[SCR_CONNECT].push_back(apc);
		
		deathlink_cbox = make_shared<CheckBox>("DeathLink", font_l);
		deathlink_cbox->setx(apc->xpos()+apc->width()+4);
		deathlink_cbox->sety(apc->ypos());
		deathlink_cbox->dis_proc = [](GUIObject const& ref) -> bool
			{
				return ap_connected();
			};
		gui_objects[SCR_CONNECT].push_back(deathlink_cbox);
		
		shared_ptr<Row> amnesty_row = make_shared<Row>();
		amnesty_row->setx(deathlink_cbox->xpos());
		amnesty_row->sety(deathlink_cbox->ypos()+deathlink_cbox->height()+4);
		
		shared_ptr<Label> amnesty_lbl = make_shared<Label>("Lives:",font_s);
		amnesty_row->add(amnesty_lbl);
		deathlink_amnesty = make_shared<TextField>("0", font_l);
		deathlink_amnesty->onValidate = validate_numeric;
		amnesty_row->dis_proc = [](GUIObject const& ref) -> bool
			{
				return ap_connected() || !(deathlink_cbox->selected());
			};
		amnesty_row->add(deathlink_amnesty);
		gui_objects[SCR_CONNECT].push_back(amnesty_row);
		
		shared_ptr<Switcher> sw = make_shared<Switcher>(
			[]()
			{
				return ap_connected() ? 1 : 0;
			},
			[](optional<u16> v){}
		);
		shared_ptr<DrawContainer> cont_uncon = make_shared<DrawContainer>();
		shared_ptr<DrawContainer> cont_con = make_shared<DrawContainer>();
		
		connect_btn = make_shared<Button>("Connect", font_l);
		shared_ptr<Button> disconnect_btn = make_shared<Button>("Disconnect", font_l);
		connect_btn->setx(apc->xpos());
		connect_btn->sety(apc->ypos() + apc->height() + 4);
		disconnect_btn->setx(connect_btn->xpos());
		disconnect_btn->sety(connect_btn->ypos());
		connect_btn->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
					{
						do_connect();
						return MRET_OK;
					}
				}
				return ref.handle_ev(e);
			};
		disconnect_btn->onMouse = [](InputObject& ref,MouseEvent e)
			{
				switch(e)
				{
					case MOUSE_LCLICK:
						if(grid->active())
						{
							if(!pop_yn("Forfeit", "Quit solving current puzzle?"
								+ string(ap_deathlink()?"\nThis will count as a DeathLink death!":"")))
								return MRET_OK;
							do_ap_death("quit a sudoku puzzle!");
							grid->clear();
						}
						do_ap_disconnect();
						return MRET_TAKEFOCUS;
				}
				return ref.handle_ev(e);
			};
		cont_uncon->push_back(connect_btn);
		cont_con->push_back(disconnect_btn);
		
		shared_ptr<Label> grid_status = make_shared<Label>(
			"Currently mid-puzzle! Changing connection requires forfeiting!",
			font_s, ALLEGRO_ALIGN_LEFT);
		grid_status->setx(connect_btn->xpos() + connect_btn->width() + 4);
		grid_status->sety(connect_btn->ypos() + (connect_btn->height() - grid_status->height()) / 2);
		grid_status->vis_proc = [](GUIObject const& ref) -> bool {return grid->active();};
		cont_uncon->push_back(grid_status);
		cont_con->push_back(grid_status);
		
		connect_error = make_shared<Label>("", font_s, ALLEGRO_ALIGN_LEFT);
		connect_error->setx(connect_btn->xpos());
		connect_error->sety(connect_btn->ypos() + connect_btn->height());
		connect_error->type = TYPE_ERROR;
		cont_uncon->push_back(connect_error);
		
		hints_left = make_shared<Label>("", font_s, ALLEGRO_ALIGN_CENTER);
		hints_left->setx(apc->xpos()+apc->width()/2);
		hints_left->sety(apc->ypos() - hints_left->height() - 2);
		cont_con->push_back(hints_left);
		
		sw->add(cont_uncon);
		sw->add(cont_con);
		gui_objects[SCR_CONNECT].push_back(sw);
	}
	{ // Settings
		//!TODO themes stuff - theme editor, loader, saver
		auto check_col = make_shared<Column>(GRID_X,GRID_Y,0,1,ALLEGRO_ALIGN_LEFT);
		
		{ //Checkbox settings
			auto cache_login_check = make_shared<CheckBox>("Cache Login Info", font_s);
			if(get_config_bool("Archipelago", "do_cache_login").value_or(false))
				cache_login_check->flags |= FL_SELECTED;
			cache_login_check->onMouse = [](InputObject& ref,MouseEvent e)
				{
					auto ret = ref.handle_ev(e);
					set_config_bool("Archipelago", "do_cache_login", (ref.flags & FL_SELECTED));
					save_cfg(CFG_ROOT);
					return ret;
				};
			check_col->add(cache_login_check);
			
			auto cache_pwd_check = make_shared<CheckBox>("Cache Login Password", font_s);
			if(get_config_bool("Archipelago", "do_cache_pwd").value_or(false))
				cache_pwd_check->flags |= FL_SELECTED;
			cache_pwd_check->onMouse = [](InputObject& ref,MouseEvent e)
				{
					auto ret = ref.handle_ev(e);
					set_config_bool("Archipelago", "do_cache_pwd", (ref.flags & FL_SELECTED));
					save_cfg(CFG_ROOT);
					return ret;
				};
			check_col->add(cache_pwd_check);
			
			auto shift_center_check = make_shared<CheckBox>("Shift=Center,Ctrl=Corner", font_s);
			if(shift_center)
				shift_center_check->flags |= FL_SELECTED;
			shift_center_check->onMouse = [](InputObject& ref,MouseEvent e)
				{
					auto ret = ref.handle_ev(e);
					shift_center = (ref.flags & FL_SELECTED);
					set_config_bool("Sudoku", "shift_center", shift_center);
					save_cfg(CFG_ROOT);
					return ret;
				};
			check_col->add(shift_center_check);
			
			auto verbose_log_check = make_shared<CheckBox>("Verbose Logging", font_s);
			if(verbose_log)
				verbose_log_check->flags |= FL_SELECTED;
			verbose_log_check->onMouse = [](InputObject& ref,MouseEvent e)
				{
					auto ret = ref.handle_ev(e);
					verbose_log = (ref.flags & FL_SELECTED);
					set_config_bool("GUI", "verbose_log", verbose_log);
					save_cfg(CFG_ROOT);
					return ret;
				};
			check_col->add(verbose_log_check);
		}
		gui_objects[SCR_SETTINGS].push_back(check_col);
		
		auto lblx = check_col->xpos()+check_col->width()+4;
		auto topy = check_col->ypos();
		auto lbl_wrap = make_shared<MiscDrawWrapper>();
		lbl_wrap->add(make_shared<Label>("Launch Res X:", font_l, ALLEGRO_ALIGN_LEFT));
		lbl_wrap->add(make_shared<Label>("Launch Res Y:", font_l, ALLEGRO_ALIGN_LEFT));
		static u16 lblw = 0;
		lblw = 0;
		for(auto ptr : lbl_wrap->cont)
			lblw = std::max(lblw,ptr->width());
		auto lbl_rx = lblx+lblw;
		auto tfw = BUTTON_X - 16 - lbl_rx;
		
		auto col_tf = make_shared<Column>(lbl_rx+2,topy,0,1,ALLEGRO_ALIGN_LEFT);
		{
			auto resx = get_config_dbl("GUI", "resx").value_or(CANVAS_W*2.0);
			auto tf = make_shared<TextField>(0, 0, tfw, format("{}",resx), font_l);
			tf->onValidate = [](string const& o, string const& n, char c)
				{
					return validate_numeric(o,n,c);
				};
			tf->onUpdate = [](TextField& tf)
				{
					if(tf.is_uint())
					{
						int v = tf.get_int();
						if(v < CANVAS_W)
							v = CANVAS_W;
						set_config_int("GUI", "resx", v);
						save_cfg(CFG_ROOT);
					}
				};
			col_tf->add(tf);
		}
		{
			auto resy = get_config_dbl("GUI", "resy").value_or(CANVAS_H*2.0);
			auto tf = make_shared<TextField>(0, 0, tfw, format("{}",resy), font_l);
			tf->onValidate = [](string const& o, string const& n, char c)
				{
					return validate_numeric(o,n,c);
				};
			tf->onUpdate = [](TextField& tf)
				{
					if(tf.is_uint())
					{
						int v = tf.get_int();
						if(v < CANVAS_H)
							v = CANVAS_H;
						set_config_int("GUI", "resy", v);
						save_cfg(CFG_ROOT);
					}
				};
			col_tf->add(tf);
		}
		
		MiscDrawWrapper* lbl_ptr = lbl_wrap.get();
		Column* col_tf_ptr = col_tf.get();
		Column* check_col_ptr = check_col.get();
		lbl_wrap->onResizeDisplay = [](GUIObject& ref)
			{
				ref.realign();
			};
		lbl_wrap->onRealign = [lbl_ptr,col_tf_ptr,check_col_ptr](size_t start)
			{
				lblw = 0;
				auto& lbls = lbl_ptr->cont;
				auto& refobjs = col_tf_ptr->cont;
				for(auto ptr : lbls)
					lblw = std::max(lblw,ptr->true_width());
				unscale_x(lblw);
				u16 ccwid = check_col_ptr->true_width();
				unscale_x(ccwid);
				auto lblx = check_col_ptr->xpos()+ccwid+4;;
				auto lbl_rx = lblx+lblw;
				auto tfw = BUTTON_X - 16 - lbl_rx;
				for(auto& ptr : col_tf_ptr->cont)
					ptr->setw(tfw);
				col_tf_ptr->setx(lbl_rx+2);
				col_tf_ptr->realign();
				for(size_t q = 0; q < lbls.size(); ++q)
				{
					auto& lbl = lbls[q];
					auto& refobj = refobjs[q];
					lbl->setx(lbl_rx-_unscale_x(lbl->true_width()));
					lbl->sety((refobj->ypos()+refobj->height()/2)-lbl->height()/2);
				}
			};
		lbl_wrap->realign();
		gui_objects[SCR_SETTINGS].push_back(lbl_wrap);
		gui_objects[SCR_SETTINGS].push_back(col_tf);
	}
}

void dlg_draw()
{
	ALLEGRO_STATE oldstate;
	al_store_state(&oldstate, ALLEGRO_STATE_TARGET_BITMAP);
	
	al_set_target_bitmap(canvas);
	clear_a5_bmp(C_BACKGROUND);
	
	gui_objects[curscr].draw();
	for(DrawContainer* p : popups)
		p->draw();
	
	al_restore_state(&oldstate);
}

void dlg_render()
{
	ALLEGRO_STATE oldstate;
	al_store_state(&oldstate, ALLEGRO_STATE_TARGET_BITMAP);
	
	al_set_target_backbuffer(display);
	clear_a5_bmp(C_BACKGROUND);
	
	al_draw_bitmap(canvas, 0, 0, 0);
	
	al_flip_display();
	
	update_scale();
	
	al_restore_state(&oldstate);
}

void setup_allegro();
void save_cfg();
volatile bool program_running = true;
u64 cur_frame = 0;
bool shape_mode = false, thicker_borders = false, show_invalid = false, verbose_log = false;
void run_events(bool& redraw)
{
	ALLEGRO_EVENT ev;
	al_wait_for_event(events, &ev);
	
	switch(ev.type)
	{
		case ALLEGRO_EVENT_TIMER:
			++cur_frame;
		[[fallthrough]];
		case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
			redraw = true;
			break;
		case ALLEGRO_EVENT_DISPLAY_CLOSE:
		{
			if(grid->active() && ap_deathlink())
			{
				if(!pop_yn("Forfeit", "Quit solving current puzzle?"
					"\nThis will count as a DeathLink death!"))
					break;
				do_ap_death("quit a sudoku puzzle!");
				grid->clear();
			}
			program_running = false;
			break;
		}
		case ALLEGRO_EVENT_DISPLAY_RESIZE:
			al_acknowledge_resize(display);
			on_resize();
			redraw = true;
			break;
		case ALLEGRO_EVENT_KEY_DOWN:
		case ALLEGRO_EVENT_KEY_UP:
		case ALLEGRO_EVENT_KEY_CHAR:
			if(cur_input && cur_input->focused)
				cur_input->focused->key_event(ev);
			break;
	}
	if(process_remote_deaths())
		grid->exit();
}
bool events_empty()
{
	return al_is_event_queue_empty(events);
}
int main(int argc, char **argv)
{
	try
	{
		string exepath(argv[0]);
		string wdir;
		auto ind = exepath.find_last_of("/\\");
		if(ind == string::npos)
			wdir = "";
		else wdir = exepath.substr(0,1+ind);
		std::filesystem::current_path(wdir);
		log("Running in dir: \"" + wdir + "\"");
		//
		setup_allegro();
		log("Allegro initialized successfully", true);
		rng = std::mt19937(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
		log("Building GUI...", true);
		build_gui();
		init_grid();
		log("...built!", true);
		PuzzleGen::init();
		
		InputState input_state;
		cur_input = &input_state;
		al_start_timer(timer);
		bool redraw = true;
		program_running = true;
		while(program_running)
		{
			if(redraw && events_empty())
			{
				gui_objects[curscr].run();
				dlg_draw();
				dlg_render();
				redraw = false;
			}
			run_events(redraw);
		}
		
		al_destroy_display(display);
		PuzzleGen::shutdown();
		return 0;
	}
	catch(ignore_exception&)
	{}
	catch(sudoku_exception& e)
	{
		fail(format("Sudoku Error: {}",e.what()));
	}
	catch(std::exception& e)
	{
		fail(format("Unknown Error: {}",e.what()));
	}
	catch(...)
	{
		fail("Unknown Error");
	}
	return 1;
}

void init_fonts()
{
	fonts[FONT_ANSWER] = FontDef(-32, false, BOLD_NONE);
	fonts[FONT_MARKING5] = FontDef(-12, true, BOLD_NONE);
	fonts[FONT_MARKING6] = FontDef(-11, true, BOLD_NONE);
	fonts[FONT_MARKING7] = FontDef(-10, true, BOLD_NONE);
	fonts[FONT_MARKING8] = FontDef(-9, true, BOLD_NONE);
	fonts[FONT_MARKING9] = FontDef(-7, true, BOLD_NONE);
}

void on_resize()
{
	int w = render_resx, h = render_resy;
	update_scale();
	if(w == render_resx && h == render_resy)
		return; // no resize
	al_destroy_bitmap(canvas);
	canvas = al_create_bitmap(render_resx, render_resy);
	if(!canvas)
		fail("Failed to recreate canvas bitmap!");
	scale_fonts();
	
	for(int scr = 0; scr < NUM_SCRS; ++scr)
		gui_objects[Screen(scr)].on_disp_resize();
	for(DrawContainer* popup : popups)
		popup->on_disp_resize();
}

void default_configs() // Resets configs to default
{
	ConfigStash stash;
	
	set_cfg(CFG_ROOT);
	add_config_section("Archipelago");
	add_config_comment("Archipelago", "Cache the Archipelago connection information, to remember it for next time");
	set_config_bool("Archipelago", "do_cache_login", true);
	add_config_comment("Archipelago", "Should the password also be cached? (does nothing if do_cache_login==false)");
	set_config_bool("Archipelago", "do_cache_pwd", false);
	add_config_comment("Archipelago", "The currently cached login info");
	set_config_str("Archipelago", "cached_ip", "archipelago.gg");
	set_config_str("Archipelago", "cached_port", "");
	set_config_str("Archipelago", "cached_slot", "");
	set_config_str("Archipelago", "cached_pwd", "");
	
	add_config_section("GUI");
	add_config_comment("GUI", "The window's starting size on launch");
	set_config_int("GUI", "resx", CANVAS_W*2.0);
	set_config_int("GUI", "resy", CANVAS_H*2.0);
	add_config_comment("GUI", "Use colored shapes instead of numbers");
	set_config_bool("GUI", "shape_mode", false);
	add_config_comment("GUI", "Thicken the borders of cells slightly");
	set_config_bool("GUI", "thicker_borders", false);
	add_config_comment("GUI", "Output extra info to the console");
	set_config_bool("GUI", "verbose_log", true);
	
	add_config_section("Sudoku");
	add_config_comment("Sudoku", "If 'shift' should do center-marks (true) or corner-marks (false)");
	set_config_bool("Sudoku", "shift_center", false);
	add_config_comment("Sudoku", "When 'Check'ing an invalid solution, highlight the errors");
	set_config_bool("Sudoku", "show_invalid", false);
	
	Theme::reset();
}
void refresh_configs() // Uses values in the loaded configs to change the program
{
	using namespace Sudoku;
	ConfigStash stash;
	
	bool wrote_any = false;
	bool b;
	#define DBL_BOUND(var, low, high, sec, key) \
	if(auto val = get_config_dbl(sec, key)) \
	{ \
		var = vbound(*val,low,high,b); \
		if(b) \
			set_config_dbl(sec, key, var); \
		wrote_any = wrote_any || b; \
	}
	#define INT_BOUND(var, low, high, sec, key) \
	if(auto val = get_config_int(sec, key)) \
	{ \
		var = vbound(*val,low,high,b); \
		if(b) \
			set_config_dbl(sec, key, var); \
		wrote_any = wrote_any || b; \
	}
	#define BOOL_READ(var, sec, key) \
	if(auto val = get_config_bool(sec, key)) \
	{ \
		var = *val; \
	}
	
	set_cfg(CFG_THEME);
	DBL_BOUND(RadioButton::fill_sel,0.5,1.0,"Style", "RadioButton Fill %")
	DBL_BOUND(CheckBox::fill_sel,0.5,1.0,"Style", "CheckBox Fill %")
	INT_BOUND(Grid::sel_style,0,NUM_STYLE-1,"Style","Grid: Cursor 2 Style")
	Theme::read_palette();
	set_cfg(CFG_ROOT);
	BOOL_READ(shift_center, "Sudoku", "shift_center")
	BOOL_READ(shape_mode, "GUI", "shape_mode")
	BOOL_READ(thicker_borders, "GUI", "thicker_borders")
	BOOL_READ(show_invalid, "Sudoku", "show_invalid")
	BOOL_READ(verbose_log, "GUI", "verbose_log")
	
	if(wrote_any)
		save_cfg();
	#undef DBL_BOUND
}
void setup_allegro()
{
	if(!al_init())
		fail("Failed to initialize Allegro!");
	
	if(!al_init_font_addon())
		fail("Failed to initialize Allegro Fonts!");
	
	if(!al_init_ttf_addon())
		fail("Failed to initialize Allegro TTF!");
	
	if(!al_init_primitives_addon())
		fail("Failed to initialize Allegro Primitives!");
	
	al_install_mouse();
	al_install_keyboard();
	
	configs[CFG_ROOT] = al_create_config();
	configs[CFG_THEME] = al_create_config();
	set_cfg(CFG_ROOT);
	default_configs();
	load_cfg();
	save_cfg();
	
	render_resx = get_config_dbl("GUI", "resx").value_or(CANVAS_W*2.0);
	render_resy = get_config_dbl("GUI", "resy").value_or(CANVAS_H*2.0);
	if(render_resx < CANVAS_W)
		render_resx = CANVAS_W;
	if(render_resy < CANVAS_H)
		render_resy = CANVAS_H;
	
	al_set_new_display_flags(ALLEGRO_RESIZABLE);
	display = al_create_display(render_resx, render_resy);
	if(!display)
		fail("Failed to create display!");
	
	al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	canvas = al_create_bitmap(CANVAS_W, CANVAS_H);
	if(!canvas)
		fail("Failed to create canvas bitmap!");
	
	timer = al_create_timer(ALLEGRO_BPS_TO_SECS(60));
	if(!timer)
		fail("Failed to create timer!");
	
	events = al_create_event_queue();
	if(!events)
		fail("Failed to create event queue!");
	al_init_user_event_source(&event_source);
	al_register_event_source(events, &event_source);
	al_register_event_source(events, al_get_display_event_source(display));
	al_register_event_source(events, al_get_timer_event_source(timer));
	al_register_event_source(events, al_get_keyboard_event_source());
	
	al_set_window_constraints(display, CANVAS_W, CANVAS_H, 0, 0);
	al_apply_window_constraints(display, true);
	
	refresh_configs();
	render_resx = -1; //force on_resize to run
	on_resize();
	init_fonts();
	init_shapes();
}

u64 rand(u64 range)
{
	return rng() % range;
}
u64 rand(u64 min, u64 max)
{
	if(max < min)
		std::swap(min,max);
	u64 range = max-min+1;
	return rand(range) + min;
}

