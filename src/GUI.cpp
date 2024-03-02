#include "Main.hpp"
#include "GUI.hpp"
#include "Font.hpp"

InputState* cur_input;
double render_xscale = 1, render_yscale = 1;
int render_resx = CANVAS_W, render_resy = CANVAS_H;
ALLEGRO_BITMAP* shape_bmps[9] = {nullptr};

void ClipRect::store()
{
	al_get_clipping_rectangle(&x, &y, &w, &h);
}
void ClipRect::load()
{
	al_set_clipping_rectangle(x, y, w, h);
}
ClipRect::ClipRect()
{
	store();
}
void ClipRect::set(int X, int Y, int W, int H)
{
	al_set_clipping_rectangle(X,Y,W,H);
}

void BmpBlender::store()
{
	al_get_separate_blender(&op, &src, &dest, &a_op, &a_src, &a_dest);
	col = al_get_blend_color();
}
void BmpBlender::load()
{
	al_set_separate_blender(op, src, dest, a_op, a_src, a_dest);
	al_set_blend_color(col);
}
BmpBlender::BmpBlender()
{
	store();
}
void BmpBlender::set(int OP, int SRC, int DEST)
{
	al_set_blender(OP, SRC, DEST);
}
void BmpBlender::set(int OP, int SRC, int DEST, int A_OP, int A_SRC, int A_DEST)
{
	al_set_separate_blender(OP, SRC, DEST, A_OP, A_SRC, A_DEST);
}
void BmpBlender::set_col(ALLEGRO_COLOR const& c)
{
	al_set_blend_color(c);
}
void BmpBlender::set_opacity_mode()
{
	al_set_blender(ALLEGRO_ADD,ALLEGRO_ONE,ALLEGRO_ZERO);
}

void DrawContainer::run()
{
	cur_input->update_start();
	auto buttons = cur_input->buttons;
	auto os = cur_input->oldstate & 0x7;
	
	if(buttons & os) //Remember the held state
		buttons &= os;
	
	cur_input->buttons &= ~0x7;
	if(buttons & 0x1)
		cur_input->buttons |= 0x1;
	else if(buttons & 0x2)
		cur_input->buttons |= 0x2;
	else if(buttons & 0x4)
		cur_input->buttons |= 0x4;
	
	mouse();
	tick();
	
	cur_input->update_end();
}
void DrawContainer::draw() const
{
	for(shared_ptr<GUIObject> const& obj : *this)
	{
		auto old_parent = obj->draw_parent;
		
		obj->draw_parent = draw_parent;
		obj->draw();
		obj->draw_parent = old_parent;
	}
}
void DrawContainer::tick()
{
	for(shared_ptr<GUIObject>& obj : *this)
	{
		auto old_parent = obj->draw_parent;
		
		obj->draw_parent = draw_parent;
		obj->tick();
		obj->draw_parent = old_parent;
	}
}
bool DrawContainer::mouse()
{
	for(auto it = rbegin(); it != rend(); ++it)
	{
		auto const& obj = (*it);
		auto old_parent = obj->draw_parent;
		
		obj->draw_parent = draw_parent;
		bool ret = obj->mouse();
		obj->draw_parent = old_parent;
		
		if(ret)
			return true;
	}
	return false;
}
void DrawContainer::on_disp_resize()
{
	for(shared_ptr<GUIObject>& obj : *this)
		obj->on_disp_resize();
}
void DrawContainer::key_event(ALLEGRO_EVENT const& ev)
{
	for(shared_ptr<GUIObject>& obj : *this)
		obj->key_event(ev);
}
void DrawContainer::run_loop()
{
	cur_input->update_start();
	cur_input->update_end();
	redraw = true;
	while(program_running && (!run_proc || run_proc()))
	{
		if(redraw && events_empty())
		{
			run();
			dlg_draw();
			dlg_render();
			redraw = false;
		}
		else run_events(redraw);
	}
}

void Dialog::run()
{
	InputState* old = cur_input;
	cur_input = &state;
	DrawContainer::run();
	cur_input = old;
}
void Dialog::draw() const
{
	InputState* old = cur_input;
	cur_input = &const_cast<Dialog*>(this)->state;
	DrawContainer::draw();
	cur_input = old;
}
void Dialog::tick()
{
	InputState* old = cur_input;
	cur_input = &state;
	DrawContainer::tick();
	cur_input = old;
}
bool Dialog::mouse()
{
	InputState* old = cur_input;
	cur_input = &state;
	bool ret = DrawContainer::mouse();
	cur_input = old;
	return ret;
}
void Dialog::on_disp_resize()
{
	InputState* old = cur_input;
	cur_input = &state;
	DrawContainer::on_disp_resize();
	cur_input = old;
}
void Dialog::key_event(ALLEGRO_EVENT const& ev)
{
	InputState* old = cur_input;
	cur_input = &state;
	DrawContainer::key_event(ev);
	cur_input = old;
}
void Dialog::run_loop()
{
	InputState* old = cur_input;
	cur_input = &state;
	DrawContainer::run_loop();
	cur_input = old;
}

void clear_a5_bmp(Color col, ALLEGRO_BITMAP* bmp)
{
	if(bmp)
	{
		ALLEGRO_STATE old_state;
		al_store_state(&old_state, ALLEGRO_STATE_TARGET_BITMAP);
		
		al_set_target_bitmap(bmp);
		al_clear_to_color(col);
		
		al_restore_state(&old_state);
	}
	else al_clear_to_color(col);
}
bool ClickInfo::check(ClickInfo const& other) const
{
	static const double DCLICK_DELAY = 1.0;
	return other.target == target && abs(other.time-time) < DCLICK_DELAY;
}
void InputState::update_start()
{
	al_get_mouse_state(this);
	
	dblstate = 0;
	double tm = al_get_time();
	ClickInfo inf = {hovered,tm};
	for(int q = 0; q < 3; ++q)
	{
		if((buttons & (0b1<<q)) && !(oldstate & (0b1<<q)))
		{
			if(dclicks[q].check(inf))
				dblstate |= 0b1 << q;
			dclicks[q] = inf;
		}
	}
}
void InputState::update_end()
{
	oldstate = buttons;
}

bool InputState::shift() const
{
	ALLEGRO_KEYBOARD_STATE st;
	al_get_keyboard_state(&st);
	return (al_key_down(&st,ALLEGRO_KEY_LSHIFT)
		|| al_key_down(&st,ALLEGRO_KEY_RSHIFT));
}
bool InputState::ctrl_cmd() const
{
	ALLEGRO_KEYBOARD_STATE st;
	al_get_keyboard_state(&st);
	return (al_key_down(&st,ALLEGRO_KEY_LCTRL)
		|| al_key_down(&st,ALLEGRO_KEY_RCTRL)
		|| al_key_down(&st,ALLEGRO_KEY_COMMAND));
}
bool InputState::alt() const
{
	ALLEGRO_KEYBOARD_STATE st;
	al_get_keyboard_state(&st);
	return (al_key_down(&st,ALLEGRO_KEY_ALT)
		|| al_key_down(&st,ALLEGRO_KEY_ALTGR));
}

bool InputState::unfocus()
{
	if(!focused) return true;
	u32 newret = focused->mouse_event(MOUSE_LOSTFOCUS);
	if(newret & MRET_TAKEFOCUS)
		return false;
	focused = nullptr;
	return true;
}
bool InputState::refocus(InputObject* targ)
{
	new_focus = targ;
	bool ret = unfocus();
	new_focus = nullptr;
	if(ret)
	{
		focused = targ;
		targ->mouse_event(MOUSE_GOTFOCUS);
	}
	return ret;
}

void update_scale()
{
	extern ALLEGRO_DISPLAY* display;
	render_resx = al_get_display_width(display);
	render_resy = al_get_display_height(display);
	
	render_xscale = double(render_resx)/CANVAS_W;
	render_yscale = double(render_resy)/CANVAS_H;
}

void scale_min(u16& v)
{
	v *= std::min(render_xscale,render_yscale);
}
void scale_max(u16& v)
{
	v *= std::max(render_xscale,render_yscale);
}
void scale_x(u16& x)
{
	x *= render_xscale;
}
void scale_y(u16& y)
{
	y *= render_yscale;
}
void scale_pos(u16& x, u16& y)
{
	x *= render_xscale;
	y *= render_yscale;
}
void scale_pos(u16& x, u16& y, u16& w, u16& h)
{
	scale_pos(x,y);
	w *= render_xscale;
	h *= render_yscale;
}
void unscale_min(u16& v)
{
	v /= std::min(render_xscale,render_yscale);
}
void unscale_max(u16& v)
{
	v /= std::max(render_xscale,render_yscale);
}
void unscale_x(u16& x)
{
	x /= render_xscale;
}
void unscale_y(u16& y)
{
	y /= render_yscale;
}
void unscale_pos(u16& x, u16& y)
{
	x /= render_xscale;
	y /= render_yscale;
}
void unscale_pos(u16& x, u16& y, u16& w, u16& h)
{
	unscale_pos(x,y);
	w /= render_xscale;
	h /= render_yscale;
}

void GUIObject::on_disp_resize()
{
	if(onResizeDisplay)
		onResizeDisplay();
	realign();
}
bool GUIObject::disabled() const
{
	return (flags&FL_DISABLED)
		|| (dis_proc && dis_proc(*this))
		|| (draw_parent && draw_parent->disabled());
}
bool GUIObject::visible() const
{
	if(draw_parent && !draw_parent->visible())
		return false;
	if(vis_proc && !vis_proc(*this))
		return false;
	return true;
}
bool GUIObject::selected() const
{
	return sel_proc ? sel_proc(*this) : (flags&FL_SELECTED);
}
bool GUIObject::focused() const
{
	return cur_input && cur_input->focused == this;
}

void InputObject::focus()
{
	if(cur_input && cur_input->focused != this)
		cur_input->refocus(this);
}
void InputObject::key_event(ALLEGRO_EVENT const& ev)
{
	if(tab_target && ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_TAB)
		tab_target->focus();
}
u32 InputObject::handle_ev(MouseEvent e)
{
	return mouse_killfocus(*this, e);
}
u32 InputObject::mouse_event(MouseEvent e)
{
	return onMouse ? onMouse(*this,e) : handle_ev(e);
}
void InputObject::unhover()
{
	if(mouseflags&MFL_HASMOUSE)
	{
		mouseflags &= ~MFL_HASMOUSE;
		process(mouse_event(MOUSE_HOVER_EXIT));
	}
	if(cur_input->hovered == this)
		cur_input->hovered = nullptr;
}
void InputObject::process(u32 retcode)
{
	if(retcode & MRET_TAKEFOCUS)
		focus();
}
bool InputObject::process_dbl(u32 retcode)
{
	process(retcode);
	return retcode&MRET_USED_DBL;
}
bool InputObject::mouse()
{
	if(!visible()) return false;
	auto& m = *cur_input;
	u16 X = xpos(), Y = ypos(), W = width(), H = height();
	scale_pos(X,Y,W,H);
	bool inbounds = (m.x >= X && m.x < X+W) && (m.y >= Y && m.y < Y+H);
	if(inbounds)
	{
		if(m.hovered && m.hovered != this)
			m.hovered->unhover();
		m.hovered = this;
		if(!(mouseflags&MFL_HASMOUSE))
		{
			mouseflags |= MFL_HASMOUSE;
			process(mouse_event(MOUSE_HOVER_ENTER));
		}
		
		if(disabled())
			return true;
		if(m.lrelease())
			process(mouse_event(MOUSE_LRELEASE));
		else if(m.lrelease())
			process(mouse_event(MOUSE_RRELEASE));
		else if(m.lrelease())
			process(mouse_event(MOUSE_MRELEASE));
		
		if(m.dlclick())
		{
			if(!process_dbl(mouse_event(MOUSE_DLCLICK)))
				process(mouse_event(MOUSE_LCLICK));
		}
		else if(m.drclick())
		{
			if(!process_dbl(mouse_event(MOUSE_DRCLICK)))
				process(mouse_event(MOUSE_RCLICK));
		}
		else if(m.dmclick())
		{
			if(!process_dbl(mouse_event(MOUSE_DMCLICK)))
				process(mouse_event(MOUSE_MCLICK));
		}
		else if(m.lclick())
			process(mouse_event(MOUSE_LCLICK));
		else if(m.rclick())
			process(mouse_event(MOUSE_RCLICK));
		else if(m.mclick())
			process(mouse_event(MOUSE_MCLICK));
		else if(m.buttons & 0x1)
			process(mouse_event(MOUSE_LDOWN));
		else if(m.buttons & 0x2)
			process(mouse_event(MOUSE_RDOWN));
		else if(m.buttons & 0x4)
			process(mouse_event(MOUSE_MDOWN));
		
		return true;
	}
	else
	{
		unhover();
		return false;
	}
}

// DrawWrapper
void DrawWrapper::draw() const
{
	if(!visible()) return;
	const_cast<DrawContainer*>(&cont)->draw_parent = this;
	cont.draw();
}
void DrawWrapper::tick()
{
	if(!visible()) return;
	cont.draw_parent = this;
	cont.tick();
}
bool DrawWrapper::mouse()
{
	if(!visible()) return false;
	cont.draw_parent = this;
	return cont.mouse();
}

// Column
u16 Column::width() const
{
	return w+2*padding;
}
u16 Column::height() const
{
	u16 H = 2*padding;
	for(shared_ptr<GUIObject> const& obj : cont)
		H += obj->height() + spacing;
	return H;
}
void Column::realign(size_t start)
{
	u16 H = padding;
	for(size_t q = 0; q < start; ++q)
	{
		shared_ptr<GUIObject>& obj = cont[q];
		H += obj->height() + spacing;
	}
	for(size_t q = start; q < cont.size(); ++q)
	{
		shared_ptr<GUIObject>& obj = cont[q];
		switch(align)
		{
			default:
				obj->setx(x+padding);
				break;
			case ALLEGRO_ALIGN_CENTRE:
				obj->setx(x+padding+(w-obj->width())/2);
				break;
			case ALLEGRO_ALIGN_RIGHT:
				obj->setx(x+padding+(w-obj->width()));
				break;
		}
		
		obj->sety(y + H);
		H += obj->height() + spacing;
		
		obj->realign();
	}
	h = H+padding;
}
void Column::add(shared_ptr<GUIObject> obj)
{
	auto objw = obj->width();
	if(objw > w)
	{
		w = objw;
		realign();
	}
	DrawWrapper::add(obj);
	realign(cont.size()-1);
}
Column::Column(u16 X, u16 Y, u16 padding, u16 spacing, i8 align)
	: DrawWrapper(X, Y), padding(padding), spacing(spacing),
	align(align)
{}
// Row
u16 Row::width() const
{
	u16 W = 2*padding;
	for(shared_ptr<GUIObject> const& obj : cont)
		W += obj->width() + spacing;
	return W;
}
u16 Row::height() const
{
	return h+2*padding;
}
void Row::realign(size_t start)
{
	u16 W = padding;
	for(size_t q = 0; q < start; ++q)
	{
		shared_ptr<GUIObject>& obj = cont[q];
		W += obj->width() + spacing;
	}
	for(size_t q = start; q < cont.size(); ++q)
	{
		shared_ptr<GUIObject>& obj = cont[q];
		switch(align)
		{
			default:
				obj->sety(y+padding);
				break;
			case ALLEGRO_ALIGN_CENTRE:
				obj->sety(y+padding+(h-obj->height())/2);
				break;
			case ALLEGRO_ALIGN_RIGHT:
				obj->sety(y+padding+(h-obj->height()));
				break;
		}
		obj->setx(x + W);
		W += obj->width() + spacing;
		
		obj->realign();
	}
	w = W+padding;
}
void Row::add(shared_ptr<GUIObject> obj)
{
	auto objh = obj->height();
	if(objh > h)
	{
		h = objh;
		realign();
	}
	DrawWrapper::add(obj);
	realign(cont.size()-1);
}
Row::Row(u16 X, u16 Y, u16 padding, u16 spacing, i8 align)
	: DrawWrapper(X, Y), padding(padding), spacing(spacing),
	align(align)
{}

// Switcher
optional<u16> Switcher::get_sel() const
{
	if(get_sel_proc)
		return get_sel_proc();
	return sel_ind;
}
void Switcher::select(optional<u16> targ)
{
	if(set_sel_proc)
		return set_sel_proc(targ);
	sel_ind = targ;
}
void Switcher::draw() const
{
	if(!visible()) return;
	auto ind = get_sel();
	if(!ind) return;
	DrawContainer& cont = *const_cast<DrawContainer*>(conts[*ind].get());
	cont.draw_parent = this;
	cont.draw();
}
bool Switcher::mouse()
{
	if(!visible()) return false;
	auto ind = get_sel();
	if(!ind) return false;
	DrawContainer& cont = *(conts[*ind].get());
	cont.draw_parent = this;
	return cont.mouse();
}
void Switcher::tick()
{
	if(!visible()) return;
	auto ind = get_sel();
	if(!ind) return;
	DrawContainer& cont = *(conts[*ind].get());
	cont.draw_parent = this;
	cont.tick();
}


// RadioButton
void RadioButton::draw() const
{
	if(!visible()) return;
	bool dis = disabled();
	bool sel = selected();
	bool hov = !dis && (flags&FL_HOVERED);
	u16 CX = x, CY = y;
	u16 X = x + radius + pad, Y = CY;
	u16 RAD = radius, RAD2 = RAD*fill_sel;
	scale_pos(CX,CY);
	scale_pos(X,Y);
	scale_min(RAD);
	scale_min(RAD2);
	const Color bgc = dis ? C_RAD_DIS_BG : (hov ? C_RAD_HOVBG : C_RAD_BG);
	const Color fgc = dis ? C_RAD_DIS_FG : C_RAD_FG;
	const Color border = C_RAD_BORDER;
	const Color textc = dis ? C_RAD_DIS_TXT : C_RAD_TXT;
	al_draw_filled_circle(CX, CY, RAD+pad/2 + (hov ? 0 : -1), border);
	al_draw_filled_circle(CX, CY, RAD, bgc);
	if(sel)
		al_draw_filled_circle(CX, CY, RAD2, fgc);
	ALLEGRO_FONT* f = font.get().get();
	Y = CY - al_get_font_line_height(f) / 2;
	if(!text.empty())
		al_draw_text(f, textc, X, Y, ALLEGRO_ALIGN_LEFT, text.c_str());
}
u32 RadioButton::handle_ev(MouseEvent e)
{
	switch(e)
	{
		case MOUSE_LCLICK:
			flags ^= FL_SELECTED;
			return MRET_TAKEFOCUS;
		case MOUSE_HOVER_ENTER:
			flags |= FL_HOVERED;
			break;
		case MOUSE_HOVER_EXIT:
			flags &= ~FL_HOVERED;
			break;
	}
	return MRET_OK;
}
u16 RadioButton::xpos() const
{
	return x - radius - pad;
}
u16 RadioButton::ypos() const
{
	return y - height()/2;
}
u16 RadioButton::width() const
{
	ALLEGRO_FONT* f = font.get().get_base();
	return radius*2 + pad*2 + al_get_text_width(f, text.c_str());
}
u16 RadioButton::height() const
{
	ALLEGRO_FONT* f = font.get().get_base();
	return std::max(al_get_font_line_height(f), radius*2);
}
double RadioButton::fill_sel = 0.5;

// RadioSet
void RadioSet::init(vector<string>& opts, FontRef fnt, u16 rad)
{
	cont.clear();
	u16 ind = 0;
	for(string const& str : opts)
	{
		shared_ptr<RadioButton> rb = make_shared<RadioButton>(str, fnt, rad);
		add(rb);
		rb->sel_proc = [this,ind](GUIObject const& ref) -> bool
			{
				return this->get_sel() == ind;
			};
		rb->onMouse = [this,ind](InputObject& ref,MouseEvent e)
			{
				if(e == MOUSE_LCLICK)
				{
					this->select(ind);
					this->focus();
					return MRET_OK;
				}
				return ref.handle_ev(e);
			};
		++ind;
	}
}
optional<u16> RadioSet::get_sel() const
{
	if(get_sel_proc)
		return get_sel_proc();
	return sel_ind;
}
optional<string> RadioSet::get_sel_text() const
{
	auto ind = get_sel();
	if(!ind)
		return nullopt;
	return std::static_pointer_cast<RadioButton>(cont[*ind])->text;
}
void RadioSet::select(optional<u16> targ)
{
	if(set_sel_proc)
		return set_sel_proc(targ);
	sel_ind = targ;
}

// CheckBox
void CheckBox::draw() const
{
	if(!visible()) return;
	bool dis = disabled();
	bool sel = selected();
	bool hov = !dis && (flags&FL_HOVERED);
	u16 CX = x, CY = y;
	u16 X = x + radius + pad, Y = CY;
	u16 RAD = radius, RAD2 = RAD*fill_sel;
	scale_pos(CX,CY);
	scale_pos(X,Y);
	scale_min(RAD);
	scale_min(RAD2);
	u16 BRAD = RAD+pad/2 + (hov ? 0 : -1);
	const Color bgc = dis ? C_RAD_DIS_BG : (hov ? C_RAD_HOVBG : C_RAD_BG);
	const Color fgc = dis ? C_RAD_DIS_FG : C_RAD_FG;
	const Color border = C_RAD_BORDER;
	const Color textc = dis ? C_RAD_DIS_TXT : C_RAD_TXT;
	al_draw_filled_rectangle(CX-BRAD, CY-BRAD, CX+BRAD, CY+BRAD, border);
	al_draw_filled_rectangle(CX-RAD, CY-RAD, CX+RAD, CY+RAD, bgc);
	if(sel)
		al_draw_filled_rectangle(CX-RAD2, CY-RAD2, CX+RAD2, CY+RAD2, fgc);
	ALLEGRO_FONT* f = font.get().get();
	Y = CY - al_get_font_line_height(f) / 2;
	if(!text.empty())
		al_draw_text(f, textc, X, Y, ALLEGRO_ALIGN_LEFT, text.c_str());
}
double CheckBox::fill_sel = 0.5;

// BaseButton
u32 BaseButton::handle_ev(MouseEvent e)
{
	switch(e)
	{
		case MOUSE_HOVER_ENTER:
			flags |= FL_HOVERED;
			break;
		case MOUSE_HOVER_EXIT:
			flags &= ~FL_HOVERED;
			break;
	}
	return MRET_OK;
}

BaseButton::BaseButton()
	: InputObject(0, 0, 96, 32)
{}
BaseButton::BaseButton(u16 X, u16 Y)
	: InputObject(X, Y, 96, 32)
{}
BaseButton::BaseButton(u16 X, u16 Y, u16 W, u16 H)
	: InputObject(X, Y, W, H)
{}
// Button
void Button::draw() const
{
	if(!visible()) return;
	u16 X = x, Y = y, W = w, H = h;
	scale_pos(X,Y,W,H);
	
	Color fg = C_BUTTON_FG;
	Color bg = C_BUTTON_BG;
	const Color border = C_BUTTON_BORDER;
	bool dis = disabled();
	bool sel = !dis && selected();
	bool hov = !dis && (flags&FL_HOVERED);
	if(sel)
		std::swap(fg,bg);
	else if(hov)
		bg = C_BUTTON_HOVBG;
	else if(dis)
	{
		fg = C_BUTTON_DISTXT;
		bg = C_BUTTON_DISBG;
	}
	if(force_bg)
		bg = *force_bg;
	if(force_fg)
		fg = *force_fg;
	
	// Fill the button
	al_draw_filled_rectangle(X, Y, X+W-1, Y+H-1, bg);
	// Draw the border 
	al_draw_rectangle(X, Y, X+W-1, Y+H-1, border, hov ? 2 : 1);
	// Finally, the text
	ALLEGRO_FONT* f = font.get().get();
	int tx = (X+W/2);
	int ty = (Y+H/2)-(al_get_font_line_height(f)/2);
	al_draw_text(f, fg, tx, ty, ALLEGRO_ALIGN_CENTRE, text.c_str());
}

u16 Button::stringw() const
{
	return al_get_text_width(font.get().get_base(), text.c_str());
}
u16 Button::stringh() const
{
	return al_get_font_line_height(font.get().get_base());
}
Button::Button()
	: BaseButton(), font(FontDef(-20, false, BOLD_NONE))
{}
Button::Button(string const& txt)
	: BaseButton(), font(FontDef(-20, false, BOLD_NONE)),
		text(txt)
{}
Button::Button(string const& txt, FontRef fnt)
	: BaseButton(), font(fnt), text(txt)
{}
Button::Button(string const& txt, FontRef fnt, u16 X, u16 Y)
	: BaseButton(X, Y), font(fnt), text(txt)
{}
Button::Button(string const& txt, FontRef fnt, u16 X, u16 Y, u16 W, u16 H)
	: BaseButton(X, Y, W, H), font(fnt), text(txt)
{}

// BmpButton
void BmpButton::draw() const
{
	if(!visible()) return;
	u16 X = x, Y = y, W = w, H = h;
	scale_pos(X,Y,W,H);
	
	Color fg = C_BUTTON_FG;
	Color bg = C_BUTTON_BG;
	const Color border = C_BUTTON_BORDER;
	bool dis = disabled();
	bool sel = !dis && selected();
	bool hov = !dis && (flags&FL_HOVERED);
	if(sel)
		std::swap(fg,bg);
	else if(hov)
		bg = C_BUTTON_HOVBG;
	else if(dis)
		bg = C_BUTTON_DISBG;
	if(force_bg)
		bg = *force_bg;
	
	// Fill the button
	al_draw_filled_rectangle(X, Y, X+W-1, Y+H-1, bg);
	// Draw the border 
	al_draw_rectangle(X, Y, X+W-1, Y+H-1, border, hov ? 2 : 1);
	// Finally, the image
	if(bmp)
	{
		u16 bw = al_get_bitmap_width(bmp);
		u16 bh = al_get_bitmap_height(bmp);
		al_draw_scaled_bitmap(bmp,
			0, 0, bw, bh,
			X, Y, W, H, 0);
	}
}

BmpButton::BmpButton()
	: BaseButton(), bmp(nullptr)
{}
BmpButton::BmpButton(ALLEGRO_BITMAP* bmp)
	: BaseButton(), bmp(bmp)
{}
BmpButton::BmpButton(ALLEGRO_BITMAP* bmp, u16 X, u16 Y)
	: BaseButton(X, Y), bmp(bmp)
{}
BmpButton::BmpButton(ALLEGRO_BITMAP* bmp, u16 X, u16 Y, u16 W, u16 H)
	: BaseButton(X, Y, W, H), bmp(bmp)
{}

// ShapeRect
void ShapeRect::draw() const
{
	if(!visible()) return;
	u16 X = x, Y = y, W = w, H = h;
	scale_pos(X,Y,W,H);
	if(c_border)
		al_draw_rectangle(X, Y, X+W-1, Y+H-1, c_border->get(), brd_thick);
	al_draw_filled_rectangle(X, Y, X+W-1, Y+H-1, c_fill);
}

ShapeRect::ShapeRect()
	: InputObject(), c_fill(C_BACKGROUND)
{}
ShapeRect::ShapeRect(u16 X, u16 Y, u16 W, u16 H)
	: InputObject(X,Y,W,H), c_fill(C_BACKGROUND)
{}
ShapeRect::ShapeRect(u16 X, u16 Y, u16 W, u16 H, Color c)
	: InputObject(X,Y,W,H), c_fill(c), brd_thick(0)
{}
ShapeRect::ShapeRect(u16 X, u16 Y, u16 W, u16 H, Color c, Color cb, double border_thick)
	: InputObject(X,Y,W,H), c_fill(c), c_border(cb), brd_thick(border_thick)
{}

// Label
void draw_text(u16 X, u16 Y, string const& str, i8 align, FontRef font, Color c_txt, optional<Color> c_shadow = nullopt)
{
	ALLEGRO_FONT* f = font.get().get();
	if(c_shadow)
		al_draw_text(f, c_shadow->get(), X + 2, Y + 2, align, str.c_str());
	al_draw_text(f, c_txt, X, Y, align, str.c_str());
}
void Label::draw() const
{
	if(!visible()) return;
	u16 X = x, Y = y;
	scale_pos(X,Y);
	if(type == TYPE_ERROR)
	{
		draw_text(X, Y, text, align, font, C_LBL_ERR_TEXT, C_LBL_ERR_SHADOW);
	}
	else
	{
		draw_text(X, Y, text, align, font, disabled() ? C_LBL_DIS_TEXT : C_LBL_TEXT, C_LBL_SHADOW);
	}
}
void Label::tick()
{
	if(text_proc)
		text = text_proc(*this);
	return InputObject::tick();
}
u16 Label::xpos() const
{
	u16 lx = InputObject::xpos();
	switch(align)
	{
		case ALLEGRO_ALIGN_LEFT:
			return lx;
		case ALLEGRO_ALIGN_CENTRE:
			return lx - width()/2;
		case ALLEGRO_ALIGN_RIGHT:
			return lx - width();
	}
	return lx;
}
u16 Label::width() const
{
	ALLEGRO_FONT* f = font.get().get_base();
	return f ? al_get_text_width(f, text.c_str()) : 0;
}
u16 Label::height() const
{
	ALLEGRO_FONT* f = font.get().get_base();
	return f ? al_get_font_line_height(f) : 0;
}
Label::Label()
	: InputObject(), text(), text_proc()
{}
Label::Label(string const& txt)
	: InputObject(), text(txt), align(ALLEGRO_ALIGN_CENTRE), text_proc()
{}
Label::Label(string const& txt, FontRef fd, i8 al)
	: InputObject(), text(txt), align(al), font(fd), text_proc()
{}
Label::Label(string const& txt, u16 X, u16 Y, FontRef fd, i8 al)
	: InputObject(X,Y), text(txt), align(al), font(fd), text_proc()
{}

bool blinkrate(u64 clk, u64 rate)
{
	return (clk % rate) < (rate / 2);
}
// TextField
void TextField::draw() const
{
	if(!visible()) return;
	u16 X = x, Y = y, W = w, H = height();
	u16 HPAD = pad, VPAD = pad;
	scale_pos(X,Y,W,H);
	scale_pos(HPAD,VPAD);
	bool dis = disabled();
	bool foc = !dis && focused();
	bool hov = !dis && (flags&FL_HOVERED);
	
	ClipRect clip;
	
	ALLEGRO_FONT* f = font.get().get();
	const Color bg = dis ? C_TF_DIS_BG : (hov ? C_TF_HOVBG : C_TF_BG);
	const Color fg = dis ? C_TF_DIS_FG : C_TF_FG;
	const Color border = C_TF_BORDER; 
	const Color selfg = C_TF_SEL_FG;
	const Color selbg = C_TF_SEL_BG;
	al_draw_rectangle(X,Y,X+W-1,Y+H-1,border,hov ? 4 : 2);
	al_draw_filled_rectangle(X,Y,X+W-1,Y+H-1,bg);
	
	ClipRect::set(X,Y,W,H);
	
	optional<u16> cursor_x;
	if(!content.empty())
	{
		if(cpos != cpos2) //selection to draw
		{
			auto TX = X+HPAD;
			string str = before_sel();
			al_draw_text(f, fg, TX, Y + VPAD, ALLEGRO_ALIGN_LEFT, str.c_str());
			TX += al_get_text_width(f,str.c_str());
			str = in_sel();
			auto SELW = al_get_text_width(f,str.c_str());
			auto FX = (!cpos || !cpos2) ? X : TX-1;
			al_draw_filled_rectangle(FX,Y,TX+SELW+1,Y+H-1,selbg);
			cursor_x = (cpos < cpos2) ? FX : (TX+SELW+1);
			al_draw_text(f, selfg, TX, Y + VPAD, ALLEGRO_ALIGN_LEFT, str.c_str());
			TX += SELW;
			str = after_sel();
			al_draw_text(f, fg, TX, Y + VPAD, ALLEGRO_ALIGN_LEFT, str.c_str());
		}
		else
		{
			al_draw_text(f, fg, X + HPAD, Y + VPAD, ALLEGRO_ALIGN_LEFT, content.c_str());
		}
	}
	if(foc && cpos <= content.size() && blinkrate(cur_frame,60)) //typing cursor
	{
		const Color cursorc2 = cpos < cpos2 ? C_TF_CURSOR : C_TF_SEL_CURSOR;
		const Color cursorc = cpos >= cpos2 ? C_TF_CURSOR : C_TF_SEL_CURSOR;
		if(!cursor_x)
		{
			string tmp = content.substr(0,cpos);
			u16 subw = al_get_text_width(f,tmp.c_str());
			cursor_x = X+HPAD+subw+1;
		}
		auto& lx = *cursor_x;
		if(cpos != cpos2)
			al_draw_filled_rectangle(lx-2, Y+VPAD, lx-2+1, Y+H-VPAD, cursorc2);
		al_draw_filled_rectangle(lx, Y+VPAD, lx+1, Y+H-VPAD, cursorc);
		//else al_draw_line(lx, Y+VPAD, lx, Y+H-VPAD, cursorc, 1);
	}
	
	clip.load();
}
u16 TextField::height() const
{
	ALLEGRO_FONT* f = font.get().get_base();
	return (2*pad) + (f ? al_get_font_line_height(f) : 0);
}

int TextField::get_int() const
{
	return std::stoi(content);
}
double TextField::get_double() const
{
	return std::stod(content);
}
string TextField::get_str() const
{
	return content;
}

static bool badchar(char c)
{
	switch(c)
	{
		case '\t':
		case '\r':
		case '\n':
			return true;
	}
	return false;
}
bool TextField::validate(string& pastestr)
{
	string b = before_sel(), a = after_sel();
	string p;
	bool ret = false;
	for(char c : pastestr)
	{
		if(validate(content,b+p+c+a,c))
		{
			p += c;
			ret = true;
		}
	}
	pastestr = p;
	return ret;
}
bool TextField::validate(string const& ostr, string const& nstr, char c)
{
	static const u16 MAX_STR = 9999;
	if(nstr.size() > MAX_STR)
		return false;
	if(badchar(c))
		return false;
	if(onValidate)
		return onValidate(ostr, nstr, c);
	return true;
}
u32 TextField::handle_ev(MouseEvent e)
{
	switch(e)
	{
		case MOUSE_LCLICK:
		case MOUSE_LDOWN:
		{
			if(e == MOUSE_LDOWN && !focused())
				break;
			u16 X = x+pad, Y = y+pad, W = w, H = height()-2*pad;
			scale_pos(X,Y,W,H);
			ALLEGRO_FONT* f = font.get().get();
			u16 ind;
			char buf[2] = {0,0};
			auto mx = cur_input->x;
			for(ind = 0; ind < content.size(); ++ind)
			{
				buf[0] = content[ind];
				auto cw = al_get_text_width(f, buf);
				if(mx < X+cw/2)
					break;
				X += cw;
			}
			cpos = ind;
			if(e == MOUSE_LCLICK)
				cpos2 = cpos;
			return MRET_TAKEFOCUS;
		}
		case MOUSE_HOVER_ENTER:
			flags |= FL_HOVERED;
			break;
		case MOUSE_HOVER_EXIT:
			flags &= ~FL_HOVERED;
			break;
		case MOUSE_LOSTFOCUS:
			cpos2 = cpos;
			break;
	}
	return MRET_OK;
}
void TextField::key_event(ALLEGRO_EVENT const& ev)
{
	switch(ev.type)
	{
		case ALLEGRO_EVENT_KEY_CHAR:
		{
			bool is_char = false;
			bool shift = cur_input->shift();
			switch(ev.keyboard.keycode)
			{
				case ALLEGRO_KEY_LEFT:
					if(cpos > 0)
						--cpos;
					if(!shift)
						cpos2 = cpos;
					break;
				case ALLEGRO_KEY_RIGHT:
					if(cpos < content.size())
						++cpos;
					if(!shift)
						cpos2 = cpos;
					break;
				case ALLEGRO_KEY_HOME:
					cpos = 0;
					if(!shift)
						cpos2 = cpos;
					break;
				case ALLEGRO_KEY_END:
					cpos = u16(content.size());
					if(!shift)
						cpos2 = cpos;
					break;
				case ALLEGRO_KEY_BACKSPACE:
					if(cpos2 != cpos)
					{
						content = before_sel() + after_sel();
						cpos = cpos2 = std::min(cpos,cpos2);
					}
					else if(cpos > 0)
					{
						content = content.substr(0,cpos-1) + content.substr(cpos);
						--cpos;
						cpos2 = cpos;
					}
					break;
				case ALLEGRO_KEY_DELETE:
					if(cpos2 != cpos)
					{
						content = before_sel() + after_sel();
						cpos = cpos2 = std::min(cpos,cpos2);
					}
					else if(cpos < content.size())
					{
						content = content.substr(0,cpos) + content.substr(cpos+1);
					}
					break;
				case ALLEGRO_KEY_V:
					if(cur_input->ctrl_cmd())
						paste();
					else is_char = true;
					break;
				case ALLEGRO_KEY_C:
					if(cur_input->ctrl_cmd())
						copy();
					else is_char = true;
					break;
				case ALLEGRO_KEY_X:
					if(cur_input->ctrl_cmd())
						cut();
					else is_char = true;
					break;
				case ALLEGRO_KEY_ENTER:
				case ALLEGRO_KEY_PAD_ENTER:
					if(onEnter)
						onEnter();
					break;
				default:
					is_char = true;
					break;
			}
			if(is_char)
			{
				char c = char(ev.keyboard.unichar);
				if(c != ev.keyboard.unichar)
					break; //out-of-range character
				if(badchar(c))
					break;
				
				string nc = before_sel() + c + after_sel();
				if(validate(content,nc,c))
				{
					content = nc;
					cpos2 = cpos = std::min(cpos,cpos2)+1;
				}
			}
			break;
		}
	}
	InputObject::key_event(ev);
}

void TextField::cut()
{
	if(cpos == cpos2) return;
	copy();
	content = before_sel() + after_sel();
	cpos2 = cpos = std::min(cpos,cpos2);
}
void TextField::copy()
{
	if(cpos == cpos2) return;
	auto str = in_sel();
	al_set_clipboard_text(display, str.c_str());
}
void TextField::paste()
{
	char* str = al_get_clipboard_text(display);
	if(str)
	{
		string s(str);
		if(validate(s))
		{
			content = before_sel() + s + after_sel();
			cpos2 = cpos = std::min(cpos,cpos2)+u16(s.size());
		}
		
		al_free(str);
	}
}
string TextField::before_sel() const
{
	return content.substr(0,std::min(cpos,cpos2));
}
string TextField::in_sel() const
{
	if(cpos == cpos2)
		return "";
	auto low = std::min(cpos,cpos2);
	auto high = std::max(cpos,cpos2);
	return content.substr(low,high-low);
}
string TextField::after_sel() const
{
	return content.substr(std::max(cpos,cpos2));
}

//COMMON EVENT FUNCS
u32 mouse_killfocus(InputObject& ref,MouseEvent e)
{
	if(e == MOUSE_LCLICK)
		cur_input->unfocus();
	return MRET_OK;
}
bool validate_numeric(string const& ostr, string const& nstr, char c)
{
	return c >= '0' && c <= '9';
}
bool validate_float(string const& ostr, string const& nstr, char c)
{
	if(c == '.')
		return ostr.find_first_of(".") == string::npos;
	return validate_numeric(ostr,nstr,c);
}
bool validate_alphanum(string const& ostr, string const& nstr, char c)
{
	return (c >= '0' && c <= '9')
		|| (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z');
}

//POPUPS
void generate_popup(Dialog& popup, optional<u8>& ret, bool& running,
	string const& title, string const& msg, vector<string> const& strs, optional<u16> w)
{
	const uint POP_W = w ? *w : CANVAS_W/2;
	const uint BTN_H = 32, BTN_PAD = 5;
	//resizes automatically
	uint POP_H = CANVAS_H/3;
	uint POP_X, POP_Y;
	const uint TITLE_H = 16, TITLE_VSPC = 2, TITLE_HPAD = 5;
	
	shared_ptr<ShapeRect> bg, win, titlebar;
	vector<shared_ptr<Label>> lbls;
	shared_ptr<Row> btnrow;
	{ //Text Label (alters popup size, must go first)
		const u16 TXT_PAD = 15, TXT_VSPC = 3;
		u16 new_h = BTN_H + 2*BTN_PAD + 2*TXT_PAD;
		if(strs.empty())
			new_h = 2*TXT_PAD;
		FontDef fd(-20, false, BOLD_NONE);
		ALLEGRO_FONT* f = fd.get_base();
		char* buf = new char[msg.size()+1];
		strcpy(buf, msg.c_str());
		size_t pos = 0, anchor = 0, last_space = 0;
		vector<string> texts;
		while(true)
		{
			char c = buf[pos];
			buf[pos] = 0;
			//
			optional<string> add_text;
			if(uint(al_get_text_width(f, buf+anchor) + 2*TXT_PAD) >= POP_W)
			{
				char c2 = buf[last_space];
				char c3;
				optional<size_t> c3pos;
				if(pos > 0)
				{
					c3 = buf[pos-1];
					c3pos = pos-1;
				}
				//
				size_t start = anchor;
				if(anchor < last_space)
				{
					buf[last_space] = 0;
					anchor = last_space;
				}
				else if(c3pos)
				{
					buf[*c3pos] = 0;
					anchor = *c3pos;
				}
				else anchor = pos;
				add_text = string(buf+start);
				//Restore stolen chars
				if(c3pos)
					buf[*c3pos] = c3;
				buf[last_space] = c2;
			}
			else if(!c) //end of string
			{
				add_text = string(buf+anchor);
			}
			else if(c == '\n')
			{
				add_text = string(buf+anchor);
				anchor = pos;
				last_space = pos;
			}
			//
			if(c == ' ')
				last_space = pos;
			buf[pos++] = c;
			
			if(add_text)
				texts.emplace_back(std::move(*add_text));
			if(!c)
				break;
		}
		delete[] buf;
		const u16 fh = al_get_font_line_height(f);
		POP_H = new_h + (u16(texts.size())*(TXT_VSPC+fh)) - TXT_VSPC;
		POP_X = CANVAS_W/2 - POP_W/2;
		POP_Y = CANVAS_H/2 - POP_H/2;
		const u16 TXT_CX = CANVAS_W/2;
		u16 TXT_Y = POP_Y+TXT_PAD;
		for(string& str : texts)
		{
			str.erase(str.find_last_not_of(" \t\r\n")+1);
			str.erase(0, str.find_first_not_of(" \t\r\n"));
			lbls.emplace_back(make_shared<Label>(str, TXT_CX, TXT_Y, fd, ALLEGRO_ALIGN_CENTRE));
			TXT_Y += fh+TXT_VSPC;
		}
		
		FontDef title_fd(-i16(TITLE_H-TITLE_VSPC*2), true, BOLD_SEMI);
		lbls.emplace_back(make_shared<Label>(title, POP_X+TITLE_HPAD, POP_Y-TITLE_H+TITLE_VSPC, title_fd, ALLEGRO_ALIGN_LEFT));
	}
	{ //BG, to allow 'clicking off' / shade the background
		bg = make_shared<ShapeRect>(0,0,CANVAS_W,CANVAS_H,al_map_rgba(0,0,0,128));
		
		win = make_shared<ShapeRect>(POP_X,POP_Y,POP_W,POP_H,C_BACKGROUND,C_BLACK,4);
		
		titlebar = make_shared<ShapeRect>(POP_X,POP_Y-TITLE_H,POP_W,TITLE_H,C_BACKGROUND,C_BLACK,4);
	}
	if(!strs.empty())
	{ //Buttons
		btnrow = make_shared<Row>(0,0,BTN_PAD,2,ALLEGRO_ALIGN_CENTRE);
		for(size_t q = 0; q < strs.size(); ++q)
		{
			shared_ptr<Button> btn = make_shared<Button>(strs[q]);
			btn->h = BTN_H;
			btn->onMouse = [&ret,&running,q](InputObject& ref,MouseEvent e)
				{
					switch(e)
					{
						case MOUSE_LCLICK:
							ret = u8(q);
							running = false;
							ref.flags |= FL_SELECTED;
							return MRET_TAKEFOCUS;
					}
					return ref.handle_ev(e);
				};
			btnrow->add(btn);
		}
		btnrow->x = (CANVAS_W - btnrow->width()) / 2;
		btnrow->y = POP_Y+POP_H-btnrow->height();
		btnrow->realign();
	}
	popup.push_back(bg);
	popup.push_back(win);
	popup.push_back(titlebar);
	for(shared_ptr<Label>& lbl : lbls)
		popup.push_back(lbl);
	if(btnrow)
		popup.push_back(btnrow);
	
	on_resize();
}
optional<u8> pop_confirm(string const& title,
	string const& msg, vector<string> const& strs, optional<u16> w)
{
	Dialog popup;
	popups.emplace_back(&popup);
	
	bool running = true;
	optional<u8> ret;
	
	generate_popup(popup, ret, running, title, msg, strs, w);
	
	popup.run_proc = [&running](){return running;};
	log("Opening popup...", true);
	popup.run_loop();
	log("Closing popup...", true);
	popups.pop_back();
	return ret;
}
bool pop_okc(string const& title, string const& msg, optional<u16> w)
{
	return 0==pop_confirm(title, msg, {"OK", "Cancel"}, w);
}
bool pop_yn(string const& title, string const& msg, optional<u16> w)
{
	return 0==pop_confirm(title, msg, {"Yes", "No"}, w);
}
void pop_inf(string const& title, string const& msg, optional<u16> w)
{
	pop_confirm(title, msg, {"OK"}, w);
}

//INITS
void init_shapes() //For "Use Colors" mode
{
	al_set_new_bitmap_flags(ALLEGRO_MIPMAP|ALLEGRO_MIN_LINEAR);
	for(int q = 0; q < 9; ++q)
	{
		shape_bmps[q] = al_create_bitmap(32*SHAPE_SCL,32*SHAPE_SCL);
		clear_a5_bmp(C_TRANS, shape_bmps[q]);
	}
	// Draw the shapes
	const uint BORDER_W = 1*SHAPE_SCL;
	const uint CX = 16*SHAPE_SCL;
	const uint CY = 16*SHAPE_SCL;
	const uint RX = 32*SHAPE_SCL-1;
	const uint BY = 32*SHAPE_SCL-1;
	const Color borderc(C_SHAPES_BORDER);
	
	const uint SHP_RAD = 12*SHAPE_SCL;
	const uint SHP_BORDER = SHP_RAD+BORDER_W;
	// "1" - Circle
	{
		al_set_target_bitmap(shape_bmps[SH_CIRCLE]);
		al_draw_filled_circle(CX, CY, SHP_BORDER, borderc);
		al_draw_filled_circle(CY, CX, SHP_RAD, Color(C_SHAPES_1));
	}
	// "2" - Semicircle
	{
		al_set_target_bitmap(shape_bmps[SH_SEMICIRCLE]);
		ClipRect r;
		ClipRect::set(0,CY-1,RX+1,BY-(CY-1));
		al_draw_filled_circle(CX, CY, SHP_BORDER, borderc);
		al_draw_filled_circle(CY, CX, SHP_RAD, Color(C_SHAPES_2));
		r.load();
		al_draw_filled_rectangle(CX-SHP_BORDER, CY,
			CX+SHP_BORDER, CY-BORDER_W, borderc); //add top border
	}
	// "3" - Crescent
	{
		const int CRESC_RAD = 12*SHAPE_SCL;
		const int CX2 = CX+CRESC_RAD - (4 * SHAPE_SCL);
		const int INTERSECT_X = CX2 - (4*SHAPE_SCL);
		al_set_target_bitmap(shape_bmps[SH_CRESCENT]);
		al_draw_filled_circle(CX, CY, SHP_BORDER, borderc);
		al_draw_filled_circle(CY, CX, SHP_RAD, Color(C_SHAPES_3));
		
		ClipRect r;
		ClipRect::set(0,0,INTERSECT_X,BY);
		al_draw_filled_circle(CX2,CY,CRESC_RAD+BORDER_W,C_BLACK);
		r.load();
		
		BmpBlender bl;
		BmpBlender::set_opacity_mode();
		al_draw_filled_circle(CX2,CY,CRESC_RAD,C_TRANS);
		bl.load();
	}
	// "4" - Triangle
	{
		al_set_target_bitmap(shape_bmps[SH_TRIANGLE]);
		al_draw_filled_triangle(CX, CY-SHP_BORDER, CX-SHP_BORDER, CY+SHP_BORDER,
			CX+SHP_BORDER, CY+SHP_BORDER, borderc);
		al_draw_filled_triangle(CX, CY-SHP_RAD, CX-SHP_RAD, CY+SHP_RAD,
			CX+SHP_RAD, CY+SHP_RAD, Color(C_SHAPES_4));
	}
	// "5" - Star
	{
		al_set_target_bitmap(shape_bmps[SH_STAR]);
		const uint STAR_RAD = 13*SHAPE_SCL;
		const uint STAR_RAD2 = 6*SHAPE_SCL;
		float vert[20];
		float border_vert[20];
		double angle = 270_deg;
		for(int q = 0; q < 10; ++q)
		{
			auto rad = q%2 ? STAR_RAD2 : STAR_RAD;
			vert[q*2] = CX+vectorX(rad,angle);
			vert[q*2+1] = CY+vectorY(rad,angle);
			
			rad += BORDER_W;
			border_vert[q*2] = CX+vectorX(rad,angle);
			border_vert[q*2+1] = CY+vectorY(rad,angle);
			
			angle -= 36_deg;
		};
		al_draw_filled_polygon(border_vert, 10, borderc);
		al_draw_filled_polygon(vert, 10, Color(C_SHAPES_5));
	}
	// "6" - Diamond
	{
		al_set_target_bitmap(shape_bmps[SH_DIAMOND]);
		const uint DIAM_RAD = 13*SHAPE_SCL;
		const uint DIAM_RAD2 = 10*SHAPE_SCL;
		float vert[8];
		float border_vert[8];
		double angle = 270_deg;
		for(int q = 0; q < 4; ++q)
		{
			auto rad = q%2 ? DIAM_RAD2 : DIAM_RAD;
			vert[q*2] = CX+vectorX(rad,angle);
			vert[q*2+1] = CY+vectorY(rad,angle);
			
			rad += BORDER_W;
			border_vert[q*2] = CX+vectorX(rad,angle);
			border_vert[q*2+1] = CY+vectorY(rad,angle);
			
			angle -= 90_deg;
		};
		al_draw_filled_polygon(border_vert, 4, borderc);
		al_draw_filled_polygon(vert, 4, Color(C_SHAPES_6));
	}
	// "7" - House
	{
		al_set_target_bitmap(shape_bmps[SH_HOUSE]);
		float vert[14] =
		{
			CX,               CY-12*SHAPE_SCL,
			CX-12*SHAPE_SCL,  CY,
			CX-8*SHAPE_SCL,   CY,
			CX-8*SHAPE_SCL,   CY+12*SHAPE_SCL,
			CX+8*SHAPE_SCL,   CY+12*SHAPE_SCL,
			CX+8*SHAPE_SCL,   CY,
			CX+12*SHAPE_SCL,  CY
		};
		float border_vert[14]
		{
			CX,                        CY-12*SHAPE_SCL-BORDER_W*2,
			CX-12*SHAPE_SCL-BORDER_W*2,  CY+BORDER_W,
			CX-8*SHAPE_SCL-BORDER_W,   CY+BORDER_W,
			CX-8*SHAPE_SCL-BORDER_W,   CY+12*SHAPE_SCL+BORDER_W,
			CX+8*SHAPE_SCL+BORDER_W,   CY+12*SHAPE_SCL+BORDER_W,
			CX+8*SHAPE_SCL+BORDER_W,   CY+BORDER_W,
			CX+12*SHAPE_SCL+BORDER_W*2,  CY+BORDER_W
		};
		
		al_draw_filled_polygon(border_vert, 7, borderc);
		al_draw_filled_polygon(vert, 7, Color(C_SHAPES_7));
	}
	// "8" - Square
	{
		al_set_target_bitmap(shape_bmps[SH_SQUARE]);
		al_draw_filled_rectangle(CX-SHP_BORDER, CY-SHP_BORDER, CX+SHP_BORDER, CY+SHP_BORDER, borderc);
		al_draw_filled_rectangle(CX-SHP_RAD, CY-SHP_RAD, CX+SHP_RAD, CY+SHP_RAD, Color(C_SHAPES_8));
	}
	// "9" - Hexagon
	{
		al_set_target_bitmap(shape_bmps[SH_HEXAGON]);
		const uint HEX_RAD = SHP_RAD;
		float vert[12];
		float border_vert[12];
		double angle = 0_deg;
		for(int q = 0; q < 6; ++q)
		{
			auto rad = HEX_RAD;
			vert[q*2] = CX+vectorX(rad,angle);
			vert[q*2+1] = CY+vectorY(rad,angle);
			
			rad += BORDER_W;
			border_vert[q*2] = CX+vectorX(rad,angle);
			border_vert[q*2+1] = CY+vectorY(rad,angle);
			
			angle -= 60_deg;
		};
		al_draw_filled_polygon(border_vert, 6, borderc);
		al_draw_filled_polygon(vert, 6, Color(C_SHAPES_9));
	}
	al_set_new_bitmap_flags(0);
}

