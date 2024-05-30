#pragma once

#include "Main.hpp"

namespace PuzzleGen
{
	void init();
	void shutdown();
	
	struct BuiltPuzzle
	{
		vector<pair<u8,bool>> cells;
		vector<set<u8>> cages;
	};
	struct Cage
	{
		u8 sum;
		set<u8> cells;
		set<u8> get_neighbors() const;
	};
	struct PuzzleCell
	{
		u8 sol;
		u8 val;
		bool given;
		Cage const* cage;
		
		set<u8> options;
		void clear();
		void reset_opts();
		PuzzleCell();
	};
	struct PuzzleGrid
	{
		PuzzleCell cells[9*9];
		vector<Cage> cages;
		PuzzleGrid(Difficulty d);
		
		static PuzzleGrid given_copy(PuzzleGrid const& g);
		bool is_unique() const;
		void print() const;
		void print_cages() const;
		void print_sol() const;
	private:
		PuzzleGrid();
		PuzzleGrid(PuzzleGrid const& other);
		void clear();
		void clear_cages();
		
		pair<set<u8>,u8> trim_opts(map<u8,set<u8>> const& banned);
		bool solve(bool check_unique);
		void populate();
		void killer_fill();
		void build(Difficulty d);
	};
	BuiltPuzzle gen_puzzle(Difficulty d);
	
	class puzzle_gen_exception : public sudoku_exception
	{
	public:
		puzzle_gen_exception(string const& msg)
			: sudoku_exception(msg)
		{}
	};
}

