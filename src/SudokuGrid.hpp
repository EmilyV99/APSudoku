#pragma once

#include "Main.hpp"
#include "GUI.hpp"
#include "Font.hpp"

namespace Sudoku
{
	// Return results for checking the state of the puzzle
	typedef u8 PuzzleState;
	#define PS_SOLVED     0b0000 //All cells are filled correctly
	#define PS_INCOMPLETE 0b0001 //At least one cell is empty
	#define PS_INVALID    0b0010 //Two cells conflict with one another
	#define PS_WRONG      0b0100 //A cell conflicts with the solution
	
	// Flags for cells
	#define CFL_GIVEN      0b0001
	#define CFL_INVALID    0b0010
	#define CFL_SELECTED   0b0100
	
	struct Cell;
	struct Grid;
	
	struct Cell
	{
		u8 solution = 0;
		u8 val = 0;
		bool center_marks[9] = {0,0,0,0,0,0,0,0,0};
		bool corner_marks[9] = {0,0,0,0,0,0,0,0,0};
		u8 flags = 0;
		
		void clear();
		void clear_marks();
		void clear_marks(EntryMode m);
		EntryMode current_mode() const;
		
		void draw(u16 x, u16 y, u16 w, u16 h) const;
		void draw_sel(u16 x, u16 y, u16 w, u16 h, u8 hlbits, bool special) const;
		
		void enter(EntryMode m, u8 val);
	};
	enum
	{
		STYLE_NONE,
		STYLE_UNDER,
		STYLE_OVER,
		NUM_STYLE
	};
	struct Grid : public InputObject
	{
		static int sel_style;
		Cell cells[9*9];
		
		Cell* get(u8 row, u8 col);
		Cell* get_hov();
		optional<u8> find(Cell* c);
		
		bool filled() const;
		bool check() const;
		
		void clear();
		void exit();
		void clear_invalid();
		void draw() const override;
		void key_event(ALLEGRO_EVENT const& ev) override;
		
		void deselect();
		void deselect(Cell* c);
		void select(Cell* c);
		set<Cell*> get_selected() const {return selected;}
		
		bool active() const;
		void generate(Difficulty d);
		
		u32 handle_ev(MouseEvent e) override;
		
		Grid(u16 X, u16 Y);
	private:
		set<Cell*> selected;
		Cell* focus_cell;
		
	};
}

