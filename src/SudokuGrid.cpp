#include "SudokuGrid.hpp"
#include "PuzzleGen.hpp"

namespace Sudoku
{
	void Cell::clear()
	{
		*this = Cell();
	}
	void Cell::clear_marks()
	{
		clear_marks(current_mode());
	}
	void Cell::clear_marks(EntryMode m)
	{
		switch(m)
		{
			case ENT_ANSWER:
				val = 0;
				break;
			case ENT_CENTER:
				memset(center_marks, 0, sizeof(center_marks));
				break;
			case ENT_CORNER:
				memset(corner_marks, 0, sizeof(corner_marks));
				break;
		}
	}
	EntryMode Cell::current_mode() const
	{
		if(val)
			return ENT_ANSWER;
		if(shape_mode)
			return ENT_CORNER;
		for(int q = 0; q < 9; ++q)
			if(center_marks[q])
				return ENT_CENTER;
		return ENT_CORNER;
	}
	void Cell::draw(u16 X, u16 Y, u16 W, u16 H) const
	{
		bool given = (flags & CFL_GIVEN);
		bool invalid = show_invalid && (flags & CFL_INVALID);
		Color bgc = C_CELL_BG;
		if(shape_mode)
			bgc = given ? C_SHAPES_GIVEN_BG : (invalid ? C_SHAPES_INVALID_BG : C_SHAPES_USER_BG);
		al_draw_filled_rectangle(X, Y, X+W-1, Y+H-1, bgc);
		al_draw_rectangle(X, Y, X+W-1, Y+H-1, Color(C_CELL_BORDER), thicker_borders ? 1 : 0.5);
		
		
		if(shape_mode)
		{
			if(val)
			{
				al_draw_scaled_bitmap(shape_bmps[val-1],
					0, 0, SHAPE_SZ, SHAPE_SZ,
					X, Y, W, H, 0);
			}
			else if(!given)
			{
				// Center marks don't work for shape mode, so only corners
				u16 SHAPE_W = CELL_SZ, SHAPE_H = CELL_SZ;
				scale_pos(SHAPE_W,SHAPE_H);
				SHAPE_W /= 3;
				SHAPE_H /= 3;
				const int CX = X+(W/2);
				int xs[] = {0,SHAPE_W,SHAPE_W*2};
				int ys[] = {0,SHAPE_H,SHAPE_H*2};
				for(u8 q = 0; q < 9; ++q)
				{
					if(!corner_marks[q]) continue;
					if(q > SH_MAX) continue;
					al_draw_scaled_bitmap(shape_bmps[q],
						0, 0, SHAPE_SZ, SHAPE_SZ,
						X+xs[q%3], Y+ys[q/3], SHAPE_W, SHAPE_H, 0);
				}
			}
		}
		else
		{
			Color fgc = invalid ? C_CELL_INVALID_FG : (given ? C_CELL_GIVEN : C_CELL_TEXT);
			if(val)
			{
				string text = to_string(val);
				ALLEGRO_FONT* f = fonts[FONT_ANSWER].get();
				int tx = (X+W/2);
				int ty = (Y+H/2)-(al_get_font_line_height(f)/2);
				al_draw_text(f, fgc, tx, ty, ALLEGRO_ALIGN_CENTRE, text.c_str());
			}
			else if(!given)
			{
				string cen_marks, crn_marks;
				for(u8 q = 0; q < 9; ++q)
				{
					if(center_marks[q])
						cen_marks += to_string(q+1);
					if(corner_marks[q])
						crn_marks += to_string(q+1);
				}
				if(!cen_marks.empty())
				{
					auto font = FONT_MARKING5;
					if(cen_marks.size() > 5)
						font = Font(FONT_MARKING5 + cen_marks.size()-5);
					ALLEGRO_FONT* f = fonts[font].get();
					int tx = (X+W/2);
					int ty = (Y+H/2)-(al_get_font_line_height(f)/2);
					al_draw_text(f, Color(C_CELL_TEXT), tx, ty, ALLEGRO_ALIGN_CENTRE, cen_marks.c_str());
				}
				if(!crn_marks.empty())
				{
					auto font = FONT_MARKING5;
					ALLEGRO_FONT* f = fonts[font].get();
					u16 vx = 6, vy = 6;
					scale_pos(vx, vy);
					int xs[] = {vx,W-vx,vx,W-vx,W/2,W/2,vx,W-vx,W/2-4};
					int ys[] = {vy,vy,H-vy,H-vy,vy,H-vy,H/2,H/2,H/2-4};
					auto fh = al_get_font_line_height(f);
					char buf[2] = {0,0};
					for(int q = 0; q < 9 && q < crn_marks.size(); ++q)
					{
						buf[0] = crn_marks[q];
						al_draw_text(f, Color(C_CELL_TEXT), X+xs[q], Y+ys[q] - fh/2,
							ALLEGRO_ALIGN_CENTRE, buf);
					}
				}
			}
		}
	}
	void Cell::draw_sel(u16 X, u16 Y, u16 W, u16 H, u8 hlbits, bool special) const
	{
		u16 HLW = 4, HLH = 4;
		Color col = C_HIGHLIGHT;
		if(special)
		{
			col = C_HIGHLIGHT2;
			hlbits = ~0;
		}
		if(!hlbits)
			return;
		scale_pos(HLW, HLH);
		for(u8 q = 0; q < NUM_DIRS; ++q)
		{
			u16 TX = X, TY = Y, TW = W, TH = H;
			if(hlbits & (1<<q))
			{
				switch(q)
				{
					case DIR_UP:
					case DIR_DOWN:
						if(!(hlbits & (1<<DIR_LEFT)))
						{
							TX -= HLW;
							TW += HLW;
						}
						if(!(hlbits & (1<<DIR_RIGHT)))
						{
							TW += HLW;
						}
						break;
					case DIR_LEFT:
					case DIR_RIGHT:
						if(!(hlbits & (1<<DIR_UP)))
						{
							TY -= HLH;
							TH += HLH;
						}
						if(!(hlbits & (1<<DIR_DOWN)))
						{
							TH += HLH;
						}
						break;
				}
				switch(q)
				{
					case DIR_UP:
						al_draw_filled_rectangle(TX, TY, TX+TW-1, TY+HLH-1, col);
						hlbits &= ~((1<<DIR_UPLEFT)|(1<<DIR_UPRIGHT));
						break;
					case DIR_DOWN:
						al_draw_filled_rectangle(TX, TY+TH-HLH, TX+TW-1, TY+TH-1, col);
						hlbits &= ~((1<<DIR_DOWNLEFT)|(1<<DIR_DOWNRIGHT));
						break;
					case DIR_LEFT:
						al_draw_filled_rectangle(TX, TY, TX+HLW-1, TY+TH-1, col);
						hlbits &= ~((1<<DIR_UPLEFT)|(1<<DIR_DOWNLEFT));
						break;
					case DIR_RIGHT:
						al_draw_filled_rectangle(TX+TW-HLW, TY, TX+TW-1, TY+TH-1, col);
						hlbits &= ~((1<<DIR_UPRIGHT)|(1<<DIR_DOWNRIGHT));
						break;
					case DIR_UPLEFT:
					{
						u16 TX2 = TX-HLW, TWOFF = HLW;
						u16 TY2 = TY-HLH, THOFF = HLH;
						al_draw_filled_rectangle(TX2, TY, TX2+TWOFF+HLW-1, TY+HLH-1, col);
						al_draw_filled_rectangle(TX, TY2, TX+HLW-1, TY2+THOFF+HLH-1, col);
						break;
					}
					case DIR_UPRIGHT:
					{
						u16 TX2 = TX, TWOFF = HLW;
						u16 TY2 = TY-HLH, THOFF = HLH;
						al_draw_filled_rectangle(TX2+TW-HLW, TY, TX2+TWOFF+TW-1, TY+HLH-1, col);
						al_draw_filled_rectangle(TX+TW-HLW, TY2, TX+TW-1, TY2+THOFF+HLH-1, col);
						break;
					}
					case DIR_DOWNLEFT:
					{
						u16 TX2 = TX-HLW, TWOFF = HLW;
						u16 TY2 = TY, THOFF = HLH;
						al_draw_filled_rectangle(TX2, TY+TH-HLH, TX2+TWOFF+HLW-1, TY+TH-1, col);
						al_draw_filled_rectangle(TX, TY2+TH-HLH, TX+HLW-1, TY2+THOFF+TH-1, col);
						break;
					}
					case DIR_DOWNRIGHT:
					{
						u16 TX2 = TX, TWOFF = HLW;
						u16 TY2 = TY, THOFF = HLH;
						al_draw_filled_rectangle(TX2+TW-HLW, TY+TH-HLH, TX2+TWOFF+TW-1, TY+TH-1, col);
						al_draw_filled_rectangle(TX+TW-HLW, TY2+TH-HLH, TX+TW-1, TY2+THOFF+TH-1, col);
						break;
					}
				}
			}
		}
	}
	
	void Cell::enter(EntryMode m, u8 v)
	{
		if(flags & CFL_GIVEN)
			return;
		switch(m)
		{
			case ENT_ANSWER:
				if(val == v)
					val = 0;
				else val = v;
				break;
			case ENT_CENTER:
				center_marks[v-1] = !center_marks[v-1];
				break;
			case ENT_CORNER:
				corner_marks[v-1] = !corner_marks[v-1];
				break;
		}
	}
	
	int Grid::sel_style = STYLE_OVER;
	
	Cell* Grid::get(u8 row, u8 col)
	{
		if(row >= 9 || col >= 9)
			return nullptr;
		return &cells[9*row + col];
	}
	Cell* Grid::get_hov()
	{
		u16 X = x, Y = y, W = CELL_SZ, H = CELL_SZ;
		scale_pos(X,Y,W,H);
		u8 col = (cur_input->x - X) / W;
		u8 row = (cur_input->y - Y) / H;
		return get(row,col);
	}
	optional<u8> Grid::find(Cell* c)
	{
		for(u8 q = 0; q < 9*9; ++q)
			if(c == &cells[q])
				return q;
		return nullopt;
	}
	
	bool Grid::filled() const
	{
		for(Cell const& c : cells)
			if(!c.val)
				return false;
		return true;
	}
	bool Grid::check()
	{
		if(!active()) return false;
		bool ret = true;
		for(Cell const& c : cells)
			if(c.solution != c.val)
			{
				ret = false;
				break;
			}
		if(!ret && filled()) //mark invalid cells
		{
			clear_invalid();
			_invalid = true;
			for(u8 q = 0; q < 9*9; ++q)
			{
				Cell& c = cells[q];
				if(c.flags & CFL_INVALID)
					continue;
				if(c.flags & CFL_GIVEN)
					continue;
				u8 row = (q/9);
				u8 col = (q%9);
				for(u8 ind = 0; ind < 9; ++ind) //same row
				{
					Cell& other = cells[ind + (row*9)];
					if(&other == &c) continue;
					if(other.val == c.val)
					{
						c.flags |= CFL_INVALID;
						other.flags |= CFL_INVALID;
					}
				}
				for(u8 ind = 0; ind < 9; ++ind) //same col
				{
					Cell& other = cells[col + (ind*9)];
					if(&other == &c) continue;
					if(other.val == c.val)
					{
						c.flags |= CFL_INVALID;
						other.flags |= CFL_INVALID;
					}
				}
				u8 bcol = col - (col%3), brow = row - (row%3);
				for(u8 ind = 0; ind < 9; ++ind) //same box
				{
					Cell& other = cells[(bcol + (ind%3)) + (brow + (ind/3))*9];
					if(&other == &c) continue;
					if(other.val == c.val)
					{
						c.flags |= CFL_INVALID;
						other.flags |= CFL_INVALID;
					}
				}
			}
		}
		return ret;
	}
	
	void Grid::clear()
	{
		if(active())
			exit();
		for(Cell& c : cells)
			c.clear();
		_invalid = false;
	}
	void Grid::exit()
	{
		_active = false;
		if(onExit)
			onExit(*this);
	}
	void Grid::clear_invalid()
	{
		if(_invalid)
		{
			_invalid = false;
			for(Cell& c : cells)
				c.flags &= ~CFL_INVALID;
		}
	}
	void Grid::draw() const
	{
		bool sel[9*9] = {0};
		optional<u8> focus_ind;
		for(u8 q = 0; q < 9*9; ++q) // Map selected
			sel[q] = (cells[q].flags & CFL_SELECTED) != 0;
		//
		#define DRAW_FOCUS() \
		if(focus_ind) \
		{ \
			u8 q = *focus_ind; \
			u16 X = x + ((q%9)*CELL_SZ), \
				Y = y + ((q/9)*CELL_SZ), \
				W = CELL_SZ, H = CELL_SZ; \
			scale_pos(X,Y,W,H); \
			focus_cell->draw_sel(X, Y, W, H, 0, true); \
		}
		//
		for(u8 q = 0; q < 9*9; ++q) // Cell draws
		{
			u16 X = x + ((q%9)*CELL_SZ),
				Y = y + ((q/9)*CELL_SZ),
				W = CELL_SZ, H = CELL_SZ;
			scale_pos(X,Y,W,H);
			Cell const& c = cells[q];
			if(&c == focus_cell)
				focus_ind = q;
			c.draw(X, Y, W, H);
		}
		for(u8 q = 0; q < 9; ++q) // 3x3 box thicker borders
		{
			u16 X = x + ((q%3)*(CELL_SZ*3)),
				Y = y + ((q/3)*(CELL_SZ*3)),
				W = CELL_SZ*3, H = CELL_SZ*3;
			scale_pos(X,Y,W,H);
			al_draw_rectangle(X, Y, X+W-1, Y+H-1, Color(C_REGION_BORDER), 2);
		}
		for(u8 cindx = 0; cindx < cages.size(); ++cindx) // cage borders/sums
		{
			auto& cage = cages[cindx];
			assert(!cage.empty());
			optional<u8> ul;
			static const u8 CAGE_PAD = 2;
			for(u8 q : cage)
			{
				if(!ul) ul = q; //top-left cell
				u8 borders = 0;
				if(q-9 < 0 || !cage.contains(q-9))
					borders |= 1<<DIR_UP;
				if(q+9 >= 81 || !cage.contains(q+9))
					borders |= 1<<DIR_DOWN;
				if(!(q%9) || !cage.contains(q-1))
					borders |= 1<<DIR_LEFT;
				if(q%9==8 || !cage.contains(q+1))
					borders |= 1<<DIR_RIGHT;
				if(borders)
				{
					u16 X = x + ((q%9)*CELL_SZ) + CAGE_PAD,
						Y = y + ((q/9)*CELL_SZ) + CAGE_PAD,
						W = CELL_SZ - CAGE_PAD*2, H = CELL_SZ - CAGE_PAD*2;
					scale_pos(X,Y,W,H);
					
					u16 X2 = X+W/3, X3 = X2+W/3, X4 = X+W-1;
					u16 Y2 = Y+H/3, Y3 = Y2+H/3, Y4 = Y+H-1;
					
					Color dashcol = C_CAGE_BORDER;
					if(borders & (1<<DIR_UP))
					{
						al_draw_line(X, Y, X2, Y, dashcol, 0);
						al_draw_line(X3, Y, X4, Y, dashcol, 0);
					}
					if(borders & (1<<DIR_DOWN))
					{
						al_draw_line(X, Y4, X2, Y4, dashcol, 0);
						al_draw_line(X3, Y4, X4, Y4, dashcol, 0);
					}
					if(borders & (1<<DIR_LEFT))
					{
						al_draw_line(X, Y, X, Y2, dashcol, 0);
						al_draw_line(X, Y3, X, Y4, dashcol, 0);
					}
					if(borders & (1<<DIR_RIGHT))
					{
						al_draw_line(X4, Y, X4, Y2, dashcol, 0);
						al_draw_line(X4, Y3, X4, Y4, dashcol, 0);
					}
				}
			}
			u8 q = *ul;
			{
				u16 X = x + ((q%9)*CELL_SZ) + CAGE_PAD,
					Y = y + ((q/9)*CELL_SZ) + CAGE_PAD;
				scale_pos(X,Y);
				string text = to_string(cage_sum(cindx));
				ALLEGRO_FONT* f = fonts[FONT_MARKING8].get();
				al_draw_text(f, Color(C_CAGE_SUM), X, Y, ALLEGRO_ALIGN_LEFT, text.c_str());
			}
		}
		if(sel_style == STYLE_UNDER) DRAW_FOCUS()
		for(u8 q = 0; q < 9*9; ++q) // Selected cell highlights
		{
			u16 X = x + ((q%9)*CELL_SZ),
				Y = y + ((q/9)*CELL_SZ),
				W = CELL_SZ, H = CELL_SZ;
			scale_pos(X,Y,W,H);
			u8 hlbits = 0;
			Cell const& c = cells[q];
			if(c.flags & CFL_SELECTED)
			{
				bool u,d,l,r;
				if(!(u = (q >= 9)) || !sel[q-9])
					hlbits |= 1<<DIR_UP;
				if(!(d = (q < 9*9-9)) || !sel[q+9])
					hlbits |= 1<<DIR_DOWN;
				if(!(l = (q % 9)) || !sel[q-1])
					hlbits |= 1<<DIR_LEFT;
				if(!(r = ((q % 9) < 8)) || !sel[q+1])
					hlbits |= 1<<DIR_RIGHT;
				if(!(u&&l) || !sel[q-9-1])
					hlbits |= 1<<DIR_UPLEFT;
				if(!(u&&r) || !sel[q-9+1])
					hlbits |= 1<<DIR_UPRIGHT;
				if(!(d&&l) || !sel[q+9-1])
					hlbits |= 1<<DIR_DOWNLEFT;
				if(!(d&&r) || !sel[q+9+1])
					hlbits |= 1<<DIR_DOWNRIGHT;
			}
			c.draw_sel(X, Y, W, H, hlbits, false);
		}
		if(sel_style == STYLE_OVER) DRAW_FOCUS()
	}
	
	void Grid::deselect()
	{
		for(Cell* c : selected)
			c->flags &= ~CFL_SELECTED;
		selected.clear();
		focus_cell = nullptr;
	}
	void Grid::deselect(Cell* c)
	{
		if(!find(c))
			throw sudoku_exception("Cannot deselect cell not from this grid!");
		selected.erase(c);
		c->flags &= ~CFL_SELECTED;
		if(focus_cell == c)
			focus_cell = nullptr;
	}
	void Grid::select(Cell* c)
	{
		if(!find(c))
			throw sudoku_exception("Cannot select cell not from this grid!");
		selected.insert(c);
		c->flags |= CFL_SELECTED;
		focus_cell = c;
	}
	void Grid::super_select(Cell* sel)
	{
		if(!find(sel))
			throw sudoku_exception("Cannot select cell not from this grid!");
		u16 center = 0, corner = 0;
		for(int q = 0; q < 9; ++q)
		{
			if(sel->center_marks[q])
				center |= 0b1<<q;
			if(sel->corner_marks[q])
				corner |= 0b1<<q;
		}
		for(Cell& c : cells)
		{
			if(sel->val)
			{
				if(c.val == sel->val)
					select(&c);
			}
			else if(center)
			{
				u16 mark = 0;
				for(int q = 0; q < 9; ++q)
				{
					if(c.center_marks[q])
						mark |= 0b1<<q;
				}
				if(mark == center)
					select(&c);
			}
			else if(corner)
			{
				u16 mark = 0;
				for(int q = 0; q < 9; ++q)
				{
					if(c.corner_marks[q])
						mark |= 0b1<<q;
				}
				if(mark == corner)
					select(&c);
			}
			else
			{
				if(!c.val)
					select(&c);
			}
		}
		select(sel);
	}
	
	bool Grid::active() const
	{
		return _active;
	}
	bool Grid::has_invalid() const
	{
		return _invalid;
	}
	void Grid::generate(Difficulty d)
	{
		_invalid = false;
		auto puz = PuzzleGen::gen_puzzle(diff);
		auto& vec = puz.cells;
		for(u8 q = 0; q < 9*9; ++q)
		{
			Sudoku::Cell& c = cells[q];
			auto [v,g] = vec[q];
			c.clear();
			c.solution = v;
			if(g)
			{
				c.flags |= CFL_GIVEN;
				c.val = v;
			}
			else c.flags &= ~CFL_GIVEN;
		}
		cages.swap(puz.cages);
		_active = true;
	}
	
	u8 Grid::cage_sum(u8 indx, bool target) const
	{
		if(indx >= cages.size())
			return 0;
		auto& cage = cages[indx];
		u8 sum = 0;
		for(u8 q : cage)
			sum += target ? cells[q].solution : cells[q].val;
		return sum;
	}
	
	void Grid::enter(u8 val)
	{
		if(selected.empty())
			return;
		if(val == 0)
		{
			EntryMode m = NUM_ENT;
			for(Cell* c : selected)
			{
				if(c->flags & CFL_GIVEN)
					continue;
				EntryMode m2 = c->current_mode();
				if(m2 < m)
					m = m2;
			}
			for(Cell* c : selected)
			{
				if(c->flags & CFL_GIVEN)
					continue;
				c->clear_marks(m);
			}
		}
		else if(val <= 9)
		{
			auto m = get_mode();
			for(Cell* c : selected)
				c->enter(m, val);
		}
	}
	void Grid::key_event(ALLEGRO_EVENT const& ev)
	{
		bool shift = cur_input->shift();
		bool ctrl_cmd = cur_input->ctrl_cmd();
		bool alt = cur_input->alt();
		switch(ev.type)
		{
			case ALLEGRO_EVENT_KEY_DOWN:
			{
				switch(ev.keyboard.keycode)
				{
					case ALLEGRO_KEY_1:
					case ALLEGRO_KEY_2:
					case ALLEGRO_KEY_3:
					case ALLEGRO_KEY_4:
					case ALLEGRO_KEY_5:
					case ALLEGRO_KEY_6:
					case ALLEGRO_KEY_7:
					case ALLEGRO_KEY_8:
					case ALLEGRO_KEY_9:
					{
						enter(ev.keyboard.keycode-ALLEGRO_KEY_0);
						break;
					}
					case ALLEGRO_KEY_PAD_1:
					case ALLEGRO_KEY_PAD_2:
					case ALLEGRO_KEY_PAD_3:
					case ALLEGRO_KEY_PAD_4:
					case ALLEGRO_KEY_PAD_5:
					case ALLEGRO_KEY_PAD_6:
					case ALLEGRO_KEY_PAD_7:
					case ALLEGRO_KEY_PAD_8:
					case ALLEGRO_KEY_PAD_9:
					{
						enter(ev.keyboard.keycode-ALLEGRO_KEY_PAD_0);
						break;
					}
					case ALLEGRO_KEY_UP: case ALLEGRO_KEY_W:
					case ALLEGRO_KEY_DOWN: case ALLEGRO_KEY_S:
					case ALLEGRO_KEY_LEFT: case ALLEGRO_KEY_A:
					case ALLEGRO_KEY_RIGHT: case ALLEGRO_KEY_D:
						if(optional<u8> o_ind = find(focus_cell))
						{
							u8 ind = *o_ind;
							if(!shift)
								deselect();
							switch(ev.keyboard.keycode)
							{
								case ALLEGRO_KEY_UP: case ALLEGRO_KEY_W:
									if(ind >= 9)
										ind -= 9;
									break;
								case ALLEGRO_KEY_DOWN: case ALLEGRO_KEY_S:
									if(ind <= 9*9-9)
										ind += 9;
									break;
								case ALLEGRO_KEY_LEFT: case ALLEGRO_KEY_A:
									if(ind % 9)
										--ind;
									break;
								case ALLEGRO_KEY_RIGHT: case ALLEGRO_KEY_D:
									if((ind % 9) < 8)
										++ind;
									break;
							}
							select(&cells[ind]);
						}
						break;
					case ALLEGRO_KEY_TAB:
						if(!mode_mod())
						{
							mode = EntryMode((mode+1)%NUM_ENT);
							if(shape_mode && mode == ENT_CENTER)
								mode = ENT_CORNER;
						}
						break;
					case ALLEGRO_KEY_DELETE:
					case ALLEGRO_KEY_BACKSPACE:
						enter(0);
						break;
				}
				break;
			}
			case ALLEGRO_EVENT_KEY_UP:
				break;
			case ALLEGRO_EVENT_KEY_CHAR:
				break;
		}
		InputObject::key_event(ev);
	}
	u32 Grid::handle_ev(MouseEvent e)
	{
		u32 ret = MRET_OK;
		switch(e)
		{
			case MOUSE_DLCLICK:
				if(focus_cell && focus_cell == get_hov())
				{
					ret |= MRET_TAKEFOCUS|MRET_USED_DBL;
					Cell* c = focus_cell;
					if(!(cur_input->shift() || cur_input->ctrl_cmd()))
						deselect();
					super_select(c);
					break;
				}
			[[fallthrough]];
			case MOUSE_LCLICK:
				ret |= MRET_TAKEFOCUS;
				if(!(cur_input->shift() || cur_input->ctrl_cmd()))
					deselect();
			[[fallthrough]];
			case MOUSE_LDOWN:
				if((ret & MRET_TAKEFOCUS) || focused())
				{
					if(Cell* c = get_hov())
						select(c);
				}
				break;
			case MOUSE_RCLICK:
				ret |= MRET_TAKEFOCUS;
			[[fallthrough]];
			case MOUSE_RDOWN:
				if(!((ret & MRET_TAKEFOCUS) || focused()))
					break;
				if(cur_input->shift() || cur_input->ctrl_cmd())
				{
					if(Cell* c = get_hov())
						deselect(c);
				}
				else
				{
					deselect();
					if(Cell* c = get_hov())
						select(c);
				}
				break;
			case MOUSE_LOSTFOCUS:
				deselect();
				break;
		}
		return ret;
	}
	
	Grid::Grid(u16 X, u16 Y)
		: InputObject(X,Y,9*CELL_SZ,9*CELL_SZ), _invalid(false), focus_cell(nullptr),
		onExit(), selected()
	{}
}

