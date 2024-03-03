#pragma once

#include "Main.hpp"
#include "Font.hpp"
#include "Theme.hpp"
extern double render_xscale, render_yscale;
extern int render_resx, render_resy;
const u8 SHAPE_SCL = 4, CELL_SZ = 32, SHAPE_SZ = SHAPE_SCL*CELL_SZ;
extern ALLEGRO_BITMAP* shape_bmps[9];

struct ClipRect
{
	int x,y,w,h;
	void store();
	void load();
	ClipRect();
	static void set(int X, int Y, int W, int H);
};

struct BmpBlender
{
	int op,src,dest;
	int a_op, a_src, a_dest;
	ALLEGRO_COLOR col;
	void store();
	void load();
	BmpBlender();
	static void set_col(ALLEGRO_COLOR const& col);
	static void set(int OP, int SRC, int DEST);
	static void set(int OP, int SRC, int DEST, int A_OP, int A_SRC, int A_DEST);
	static void set_opacity_mode();
};

struct InputObject;
struct GUIObject;
struct ClickInfo
{
	InputObject* target = nullptr;
	double time = 0.0;
	
	bool check(ClickInfo const& other) const;
};
struct InputState : public ALLEGRO_MOUSE_STATE
{
	int oldstate = 0;
	int dblstate = 0;
	InputObject* hovered = nullptr;
	InputObject* focused = nullptr;
	InputObject* new_focus = nullptr;
	ClickInfo dclicks[3];
	
	void update_start();
	void update_end();
	
	bool dlclick() const {return (dblstate&0x7) == 0x1 && lclick();}
	bool drclick() const {return (dblstate&0x7) == 0x2 && rclick();}
	bool dmclick() const {return (dblstate&0x7) == 0x4 && mclick();}
	bool lclick() const {return (buttons&0x7) == 0x1 && !(oldstate&0x7);}
	bool rclick() const {return (buttons&0x7) == 0x2 && !(oldstate&0x7);}
	bool mclick() const {return (buttons&0x7) == 0x4 && !(oldstate&0x7);}
	bool lrelease() const {return !(buttons&0x1) && (oldstate&0x7) == 0x1;}
	bool rrelease() const {return !(buttons&0x2) && (oldstate&0x7) == 0x2;}
	bool mrelease() const {return !(buttons&0x4) && (oldstate&0x7) == 0x4;}
	
	bool shift() const;
	bool ctrl_cmd() const;
	bool alt() const;
	
	bool unfocus();
	bool refocus(InputObject* targ);
};
extern InputState* cur_input;

struct DrawContainer : public vector<shared_ptr<GUIObject>>
{
	GUIObject const* draw_parent = nullptr;
	bool redraw = true;
	std::function<bool()> run_proc;
	virtual void run();
	virtual void draw() const;
	virtual void tick();
	virtual bool mouse();
	virtual void on_disp_resize();
	virtual void key_event(ALLEGRO_EVENT const& ev);
	
	virtual void run_loop();
};

struct Dialog : public DrawContainer
{
	InputState state;
	virtual void run() override;
	virtual void draw() const override;
	virtual void tick() override;
	virtual bool mouse() override;
	virtual void on_disp_resize() override;
	virtual void key_event(ALLEGRO_EVENT const& ev) override;
	
	virtual void run_loop() override;
};

void clear_a5_bmp(Color col, ALLEGRO_BITMAP* bmp = nullptr);

void update_scale();
void scale_min(u16& v);
void scale_max(u16& v);
void scale_x(u16& x);
void scale_y(u16& y);
u16 _scale_x(u16 x);
u16 _scale_y(u16 y);
void scale_pos(u16& x, u16& y);
void scale_pos(u16& x, u16& y, u16& w, u16& h);
void unscale_min(u16& v);
void unscale_max(u16& v);
void unscale_x(u16& x);
void unscale_y(u16& y);
u16 _unscale_x(u16 x);
u16 _unscale_y(u16 y);
void unscale_pos(u16& x, u16& y);
void unscale_pos(u16& x, u16& y, u16& w, u16& h);

enum MouseEvent
{
	MOUSE_HOVER_ENTER,
	MOUSE_HOVER_EXIT,
	MOUSE_GOTFOCUS,
	MOUSE_LOSTFOCUS,
	MOUSE_LCLICK,
	MOUSE_RCLICK,
	MOUSE_MCLICK,
	MOUSE_DLCLICK,
	MOUSE_DRCLICK,
	MOUSE_DMCLICK,
	MOUSE_LRELEASE,
	MOUSE_RRELEASE,
	MOUSE_MRELEASE,
	MOUSE_LDOWN,
	MOUSE_RDOWN,
	MOUSE_MDOWN,
};

#define MFL_HASMOUSE 0x01

#define MRET_OK         (u32(0x00))
#define MRET_TAKEFOCUS  (u32(0x01))
#define MRET_USED_DBL   (u32(0x02))

#define FL_SELECTED 0b00000001
#define FL_DISABLED 0b00000010
#define FL_HOVERED  0b00000100

struct DrawWrapper;
enum WidgType
{
	TYPE_NORMAL,
	TYPE_ERROR,
	NUM_TYPES
};
struct GUIObject
{
	u8 flags;
	u8 custom_flags;
	u8 type;
	std::function<void(GUIObject&)> onResizeDisplay;
	//Obj is disabled if this returns true, OR flags&FL_DISABLED
	std::function<bool(GUIObject const&)> dis_proc;
	//If set, obj is selected iff returns true,
	//ELSE obj is selected if flags&FL_SELECT
	std::function<bool(GUIObject const&)> sel_proc;
	//If set, obj is invisible if returns true.
	std::function<bool(GUIObject const&)> vis_proc;
	
	GUIObject const* draw_parent;
	
	void on_disp_resize();
	virtual void draw() const {};
	virtual bool mouse() {return false;}
	virtual void tick() {}
	virtual u16 xpos() const {return 0;}
	virtual u16 ypos() const {return 0;}
	virtual void setx(u16 v) {}
	virtual void sety(u16 v) {}
	virtual void setw(u16 v) {}
	virtual void seth(u16 v) {}
	virtual u16 width() const {return 0;}
	virtual u16 height() const {return 0;}
	virtual u16 true_width() const {return width();}
	virtual u16 true_height() const {return height();}
	virtual bool disabled() const;
	virtual bool selected() const;
	virtual bool visible() const;
	virtual void key_event(ALLEGRO_EVENT const& ev) {}
	virtual void realign(size_t start = 0) {}
	virtual void focus(){}
	bool focused() const;
	GUIObject()
		: flags(0), custom_flags(0), onResizeDisplay(),
			dis_proc(), sel_proc(), vis_proc(),
			draw_parent(nullptr), type(TYPE_NORMAL)
	{}
};

struct InputObject : public GUIObject
{
	u16 x, y, w, h;
	u8 mouseflags;
	InputObject* tab_target;
	std::function<u32(InputObject&,MouseEvent)> onMouse;
	virtual bool mouse() override;
	virtual u16 xpos() const override {return x;}
	virtual u16 ypos() const override {return y;}
	void setx(u16 v) override {x = v + (x-xpos());}
	void sety(u16 v) override {y = v + (y-ypos());}
	void setw(u16 v) override {w = v;}
	void seth(u16 v) override {h = v;}
	virtual u16 width() const override {return w;}
	virtual u16 height() const override {return h;}
	virtual u32 handle_ev(MouseEvent ev);
	virtual void focus() override;
	virtual void key_event(ALLEGRO_EVENT const& ev) override;
	u32 mouse_event(MouseEvent ev);
	void unhover();
	InputObject()
		: GUIObject(), x(0), y(0), w(0), h(0),
		mouseflags(0), onMouse(), tab_target(nullptr) {}
	InputObject(u16 x, u16 y)
		: GUIObject(), x(x), y(y), w(0), h(0),
		mouseflags(0), onMouse(), tab_target(nullptr) {}
	InputObject(u16 x, u16 y, u16 w, u16 h)
		: GUIObject(), x(x), y(y), w(w), h(h),
		mouseflags(0), onMouse(), tab_target(nullptr) {}
private:
	void process(u32 retcode);
	bool process_dbl(u32 retcode);
};

struct DrawWrapper : public InputObject
{
	DrawContainer cont;
	virtual void draw() const override;
	virtual bool mouse() override;
	virtual void tick() override;
	virtual u16 width() const override {return 0;}
	virtual u16 height() const override {return 0;}
	
	virtual void add(shared_ptr<GUIObject> obj) {cont.emplace_back(obj);}
	DrawWrapper() = default;
	DrawWrapper(u16 X, u16 Y) : InputObject(X,Y) {}
};

struct MiscDrawWrapper : public DrawWrapper
{
	std::function<void(size_t)> onRealign;
	void realign(size_t start = 0) override
	{
		if(onRealign)
			onRealign(start);
	}
	virtual void add(shared_ptr<GUIObject> obj) override
	{
		DrawWrapper::add(obj);
		realign(cont.size()-1);
	}
	MiscDrawWrapper() = default;
};

struct Column : public DrawWrapper
{
	u16 padding, spacing;
	i8 align;
	void realign(size_t start = 0) override;
	virtual u16 width() const override;
	virtual u16 height() const override;
	virtual u16 true_width() const override;
	virtual u16 true_height() const override;
	virtual void add(shared_ptr<GUIObject> obj) override;
	Column() : padding(0), spacing(2), align(ALLEGRO_ALIGN_CENTRE) {};
	Column(u16 X, u16 Y, u16 padding = 0,
		u16 spacing = 2, i8 align = ALLEGRO_ALIGN_CENTRE);
};
struct Row : public DrawWrapper
{
	u16 padding, spacing;
	i8 align;
	void realign(size_t start = 0) override;
	virtual u16 width() const override;
	virtual u16 height() const override;
	virtual u16 true_width() const override;
	virtual u16 true_height() const override;
	virtual void add(shared_ptr<GUIObject> obj) override;
	Row() : padding(4), spacing(2), align(ALLEGRO_ALIGN_CENTRE) {};
	Row(u16 X, u16 Y, u16 padding = 4,
		u16 spacing = 2, i8 align = ALLEGRO_ALIGN_CENTRE);
};

struct Switcher : public InputObject
{
	vector<shared_ptr<DrawContainer>> conts;
	Switcher() : InputObject(), conts(),
		get_sel_proc(), set_sel_proc() {}
	Switcher(std::function<optional<u16>()> getter,
		std::function<void(optional<u16>)> setter)
		: InputObject(), conts(), get_sel_proc(getter),
		set_sel_proc(setter)
	{}
	optional<u16> get_sel() const;
	void select(optional<u16> ind);
	virtual void draw() const override;
	virtual bool mouse() override;
	virtual void tick() override;
	void add(shared_ptr<DrawContainer> cont) {conts.push_back(cont);}
private:
	optional<u16> sel_ind;
	std::function<optional<u16>()> get_sel_proc;
	std::function<void(optional<u16>)> set_sel_proc;
};

struct RadioButton : public InputObject
{
	u16 radius;
	string text;
	FontRef font;
	static const u16 pad = 4;
	static double fill_sel;
	
	virtual void draw() const override;
	virtual u16 xpos() const override;
	virtual u16 ypos() const override;
	virtual u16 width() const override;
	virtual u16 height() const override;
	virtual u16 true_width() const override;
	virtual u16 true_height() const override;
	RadioButton() : text(), font(FontDef(-20, false, BOLD_NONE)),
		radius(4)
	{}
	RadioButton(string const& txt, FontRef fnt, u16 rad = 4)
		: radius(rad), text(txt), font(fnt)
	{}
	RadioButton(u16 X, u16 Y, string const& txt, FontRef fnt, u16 rad = 4)
		: InputObject(X,Y), radius(rad), text(txt), font(fnt)
	{}
	
	u32 handle_ev(MouseEvent ev) override;
};
struct RadioSet : public Column
{
	RadioSet(vector<string> opts, FontRef fnt, u16 rad = 4)
		: Column(0,0,0,2,ALLEGRO_ALIGN_LEFT)
	{
		init(opts, fnt, rad);
		realign();
	}
	RadioSet(std::function<optional<u16>()> getter,
		std::function<void(optional<u16>)> setter,
		vector<string> opts, FontRef fnt, u16 rad = 4)
		: Column(0,0,0,2,ALLEGRO_ALIGN_LEFT),
		get_sel_proc(getter), set_sel_proc(setter)
	{
		init(opts, fnt, rad);
		realign();
	}
	optional<u16> get_sel() const;
	optional<string> get_sel_text() const;
	void select(optional<u16> ind);
private:
	optional<u16> sel_ind;
	std::function<optional<u16>()> get_sel_proc;
	std::function<void(optional<u16>)> set_sel_proc;
	void init(vector<string>& opts, FontRef fnt, u16 rad);
};
struct CheckBox : public RadioButton
{
	static double fill_sel;
	virtual void draw() const override;
	CheckBox() : RadioButton() {}
	CheckBox(string const& txt, FontRef fnt, u16 rad = 4)
		: RadioButton(txt,fnt,rad)
	{}
	CheckBox(u16 X, u16 Y, string const& txt, FontRef fnt, u16 rad = 4)
		: RadioButton(X,Y,txt,fnt,rad)
	{}
};

struct BaseButton : public InputObject
{
	void draw() const override = 0;
	optional<Color> force_bg;
	
	BaseButton();
	BaseButton(u16 X, u16 Y);
	BaseButton(u16 X, u16 Y, u16 W, u16 H);
	
	u32 handle_ev(MouseEvent ev) override;
};

struct Button : public BaseButton
{
	string text;
	FontRef font;
	optional<Color> force_fg;
	
	void draw() const override;
	u16 stringw() const;
	u16 stringh() const;
	
	Button();
	Button(string const& txt);
	Button(string const& txt, FontRef fnt);
	Button(string const& txt, FontRef fnt, u16 X, u16 Y);
	Button(string const& txt, FontRef fnt, u16 X, u16 Y, u16 W, u16 H);
};

struct BmpButton : public BaseButton
{
	ALLEGRO_BITMAP* bmp;
	
	void draw() const override;
	
	BmpButton();
	BmpButton(ALLEGRO_BITMAP* bmp);
	BmpButton(ALLEGRO_BITMAP* bmp, u16 X, u16 Y);
	BmpButton(ALLEGRO_BITMAP* bmp, u16 X, u16 Y, u16 W, u16 H);
};

struct ShapeRect : public InputObject
{
	Color c_fill;
	optional<Color> c_border;
	double brd_thick;
	
	void draw() const override;
	
	ShapeRect();
	ShapeRect(u16 X, u16 Y, u16 W, u16 H);
	ShapeRect(u16 X, u16 Y, u16 W, u16 H, Color c);
	ShapeRect(u16 X, u16 Y, u16 W, u16 H, Color c, Color cb, double border_thick);
};

void draw_text(string const& str, i8 align, FontRef font, Color c_txt, optional<Color> c_shadow = nullopt);
struct Label : public InputObject
{
	string text;
	i8 align;
	FontRef font;
	std::function<string(Label& ref)> text_proc;
	
	void draw() const override;
	u16 xpos() const override;
	//u16 ypos() const override; //add valign?
	u16 width() const override;
	u16 height() const override;
	u16 true_width() const override;
	u16 true_height() const override;
	void tick() override;
	
	Label();
	Label(string const& txt);
	Label(string const& txt, FontRef fd, i8 align = ALLEGRO_ALIGN_CENTRE);
	Label(string const& txt, u16 X, u16 Y, FontRef fd, i8 align = ALLEGRO_ALIGN_CENTRE);
};

struct TextField : public InputObject
{
	string content;
	FontRef font;
	u16 cpos, cpos2; //cpos is cursor, cpos2 is anchor of selection
	static const u16 pad = 2;
	std::function<bool(string const&,string const&,char)> onValidate;
	std::function<void(TextField&)> onUpdate;
	std::function<bool()> onEnter;
	
	void draw() const override;
	u16 height() const override;
	
	int get_int() const;
	double get_double() const;
	string get_str() const;
	bool is_int() const;
	bool is_uint() const;
	bool is_double() const;
	bool is_udouble() const;
	
	TextField() : InputObject(0,0,64,0), content(),
		font(FontDef(-20, false, BOLD_NONE)), cpos(0), cpos2(0)
	{}
	TextField(string const& txt) : InputObject(0,0,64,0), content(txt),
		font(FontDef(-20, false, BOLD_NONE)), cpos(0), cpos2(0)
	{}
	TextField(string const& txt, FontRef fd)
		: InputObject(0,0,64,0), content(txt), font(fd), cpos(0), cpos2(0)
	{}
	TextField(u16 X, u16 Y, u16 W, string const& txt, FontRef fd)
		: InputObject(X,Y,W,0), content(txt), font(fd), cpos(0), cpos2(0)
	{}
	
	bool validate(string const& ostr, string const& nstr, char c);
	u32 handle_ev(MouseEvent ev) override;
	void key_event(ALLEGRO_EVENT const& ev) override;
private:
	bool validate(string& pastestr);
	void update();
	void cut();
	void copy();
	void paste();
	string before_sel() const;
	string in_sel() const;
	string after_sel() const;
};

u32 mouse_killfocus(InputObject& ref,MouseEvent e);
bool validate_numeric(string const& ostr, string const& nstr, char c);
bool validate_float(string const& ostr, string const& nstr, char c);
bool validate_alphanum(string const& ostr, string const& nstr, char c);

void generate_popup(Dialog& popup, optional<u8>& ret, bool& running,
	string const& title, string const& msg, vector<string> const& strs, optional<u16> w = nullopt);
optional<u8> pop_confirm(string const& title,
	string const& msg, vector<string> const& strs, optional<u16> w = nullopt);
bool pop_okc(string const& title, string const& msg, optional<u16> w = nullopt);
bool pop_yn(string const& title, string const& msg, optional<u16> w = nullopt);
void pop_inf(string const& title, string const& msg, optional<u16> w = nullopt);

void init_shapes();

enum NumberShapes
{
	SH_CIRCLE,
	SH_SEMICIRCLE,
	SH_CRESCENT,
	SH_TRIANGLE,
	SH_STAR,
	SH_DIAMOND,
	SH_HOUSE,
	SH_SQUARE,
	SH_HEXAGON,
	SH_MAX
};

