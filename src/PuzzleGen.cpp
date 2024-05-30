#include "Main.hpp"
#include "GUI.hpp"
#include "PuzzleGen.hpp"
#include <sstream>
#include <thread>
#include <mutex>

namespace PuzzleGen
{

struct PuzzleQueue
{
	BuiltPuzzle take();
	void give(BuiltPuzzle&& puz);
	size_t size() const;
	size_t atm_size();

	bool try_lock_if_unempty();
	
	void lock() {mut.lock();}
	bool try_lock() {return mut.try_lock();}
	void unlock() {mut.unlock();}
private:
	deque<BuiltPuzzle> queue;
	std::mutex mut;
};
BuiltPuzzle PuzzleQueue::take()
{
	BuiltPuzzle ret = queue.front();
	queue.pop_front();
	return ret;
}
void PuzzleQueue::give(BuiltPuzzle&& puz)
{
	queue.emplace_back(puz);
}
size_t PuzzleQueue::size() const
{
	return queue.size();
}
size_t PuzzleQueue::atm_size()
{
	lock();
	size_t ret = size();
	unlock();
	return ret;
}
bool PuzzleQueue::try_lock_if_unempty()
{
	if(!try_lock())
		return false;
	if(queue.empty())
	{
		unlock();
		return false;
	}
	return true;
}

struct PuzzleGenFactory
{
	static BuiltPuzzle get(Difficulty d);
	static void init();
	static void shutdown();
private:
	static const u8 KEEP_READY = 10;
	static PuzzleQueue puzzles[NUM_DIFF];
	
	Difficulty d;
	volatile bool running;
	std::thread runtime;
	
	void run();
	
	PuzzleGenFactory() = default;
	PuzzleGenFactory(Difficulty d) : d(d), running(false),
		runtime()
	{}
	#define NUM_E_FAC       1
	#define NUM_M_FAC       1
	#define NUM_H_FAC       3
	#define NUM_KILLER_FAC  3
	static PuzzleGenFactory e_factories[NUM_E_FAC];
	static PuzzleGenFactory m_factories[NUM_M_FAC];
	static PuzzleGenFactory h_factories[NUM_H_FAC];
	static PuzzleGenFactory killer_factories[NUM_KILLER_FAC];
	static set<PuzzleGenFactory*> factories;
};
PuzzleGenFactory PuzzleGenFactory::e_factories[NUM_E_FAC];
PuzzleGenFactory PuzzleGenFactory::m_factories[NUM_M_FAC];
PuzzleGenFactory PuzzleGenFactory::h_factories[NUM_H_FAC];
PuzzleGenFactory PuzzleGenFactory::killer_factories[NUM_KILLER_FAC];
set<PuzzleGenFactory*> PuzzleGenFactory::factories;
PuzzleQueue PuzzleGenFactory::puzzles[NUM_DIFF];

void PuzzleGenFactory::run()
{
	PuzzleQueue& queue = puzzles[d];
	while(running && program_running)
	{
		try
		{
			if(queue.atm_size() >= KEEP_READY)
			{
				al_rest(0.05);
				continue;
			}
			
			PuzzleGrid puzzle(d); //generate the puzzle at current difficulty
			//puzzle.print_sol();
			//puzzle.print_cages();
			//
			BuiltPuzzle puz;
			for(PuzzleCell const& cell : puzzle.cells)
				puz.cells.emplace_back(cell.sol, cell.given);
			for(Cage& cage : puzzle.cages)
				puz.cages.emplace_back(std::move(cage.cells));
			queue.lock();
			queue.give(std::move(puz));
			queue.unlock();
			al_rest(0.05);
		}
		catch(ignore_exception&)
		{}
	}
}
BuiltPuzzle PuzzleGenFactory::get(Difficulty d)
{
	PuzzleQueue& queue = puzzles[d];
	if(!queue.try_lock_if_unempty())
	{
		optional<u8> _ret;
		bool _foo;
		Dialog popup;
		popups.emplace_back(&popup);
		generate_popup(popup, _ret, _foo, "Please Wait", "Generating puzzle...", {});
		popup.run_proc = [&queue]()
			{
				return !queue.try_lock_if_unempty();
			};
		popup.run_loop();
		popups.pop_back();
	}
	if(!program_running)
		throw ignore_exception();
	
	BuiltPuzzle puz = queue.take();
	
	queue.unlock();
	
	return puz;
}
void PuzzleGenFactory::init()
{
	log("Launching puzzle factories...", true);
	for(u8 q = 0; q < NUM_E_FAC; ++q)
	{
		PuzzleGenFactory& fac = e_factories[q];
		new (&fac) PuzzleGenFactory(DIFF_EASY);
		fac.running = true;
		fac.runtime = std::thread(&PuzzleGenFactory::run, &fac);
		factories.insert(&fac);
	}
	for(u8 q = 0; q < NUM_M_FAC; ++q)
	{
		PuzzleGenFactory& fac = m_factories[q];
		new (&fac) PuzzleGenFactory(DIFF_NORMAL);
		fac.running = true;
		fac.runtime = std::thread(&PuzzleGenFactory::run, &fac);
		factories.insert(&fac);
	}
	for(u8 q = 0; q < NUM_H_FAC; ++q)
	{
		PuzzleGenFactory& fac = h_factories[q];
		new (&fac) PuzzleGenFactory(DIFF_HARD);
		fac.running = true;
		fac.runtime = std::thread(&PuzzleGenFactory::run, &fac);
		factories.insert(&fac);
	}
	for(u8 q = 0; q < NUM_KILLER_FAC; ++q)
	{
		PuzzleGenFactory& fac = killer_factories[q];
		new (&fac) PuzzleGenFactory(DIFF_KILLER);
		fac.running = true;
		fac.runtime = std::thread(&PuzzleGenFactory::run, &fac);
		factories.insert(&fac);
	}
	log("...launched!", true);
}
void PuzzleGenFactory::shutdown()
{
	log("Closing puzzle factories...", true);
	for(PuzzleGenFactory* f : factories)
		f->running = false;
	for(PuzzleGenFactory* f : factories)
		f->runtime.join();
	log("...closed!", true);
}

void init()
{
	PuzzleGenFactory::init();
}
void shutdown()
{
	PuzzleGenFactory::shutdown();
}

PuzzleCell::PuzzleCell()
	: val(0), given(true), cage(nullptr),
	options({1,2,3,4,5,6,7,8,9})
{}

void PuzzleCell::reset_opts()
{
	options = {1,2,3,4,5,6,7,8,9};
}
void PuzzleCell::clear()
{
	*this = PuzzleCell();
}

struct GridFillHistory
{
	u8 ind;
	map<u8,set<u8>> checked;
	GridFillHistory() : ind(0), checked() {}
};
struct GridGivenHistory
{
	u8 ind;
	u8 built_cages;
	set<u8> checked;
	GridGivenHistory() : ind(0), built_cages(0), checked() {}
};

set<u8> Cage::get_neighbors() const
{
	set<u8> ret;
	for(u8 c : cells)
	{
		if(c-9 >= 0)
			ret.insert(c-9);
		if(c+9 < 81)
			ret.insert(c+9);
		if(c%9)
			ret.insert(c-1);
		if(c%9 < 8)
			ret.insert(c+1);
	}
	for(u8 c : cells)
		ret.erase(c); //cells in the cage aren't neighbors
	return ret;
}

PuzzleGrid::PuzzleGrid(Difficulty d)
	: PuzzleGrid()
{
	populate();
	build(d);
}
PuzzleGrid::PuzzleGrid()
{
	clear();
}
void PuzzleGrid::clear()
{
	for (PuzzleCell& c : cells)
		c.clear();
}
void PuzzleGrid::clear_cages()
{
	cages.clear();
	for(PuzzleCell& c : cells)
		c.cage = nullptr;
}
PuzzleGrid::PuzzleGrid(PuzzleGrid const& other)
{
	for(u8 q = 0; q < 9*9; ++q)
		cells[q] = other.cells[q];
	cages = other.cages;
	for(Cage& cage : cages)
		for(u8 q : cage.cells)
			cells[q].cage = &cage;
}
PuzzleGrid PuzzleGrid::given_copy(PuzzleGrid const& g)
{
	PuzzleGrid ret(g);
	for(u8 q = 0; q < 9*9; ++q)
	{
		if(!ret.cells[q].given)
			ret.cells[q].val = 0;
	}
	return ret;
}
bool PuzzleGrid::is_unique() const
{
	PuzzleGrid test = given_copy(*this);
	return test.solve(true);
}

pair<set<u8>,u8> PuzzleGrid::trim_opts(map<u8,set<u8>> const& banned)
{
	bool killer = false;
	//Calculate basic sudoku options
	for(u8 index = 0; index < 9*9; ++index)
	{
		PuzzleCell& cell = cells[index];
		if(cell.val)
		{
			cell.options.clear();
			continue; //skip filled cells
		}
		
		//Start by assuming all valid digits are options
		cell.options = {1,2,3,4,5,6,7,8,9};
		
		//Remove options that failed trial-and-error
		auto banset = banned.find(index);
		if(banset != banned.end())
			for(u8 val : banset->second)
				cell.options.erase(val);
		
		//Build a list of 'neighbors', the cells that 'see' this cell
		u8 col = index%9;
		u8 row = index/9;
		u8 box = 3*(row/3)+(col/3);
		set<u8> neighbors;
		for(u8 q = 0; q < 9; ++q)
		{
			neighbors.insert(9*q + col); //same column
			neighbors.insert(9*row + q); //same row
			neighbors.insert(9*(3*(box/3) + (q/3)) + (3*(box%3) + (q%3))); //same box
		}
		if(cell.cage) //cells in the same cage are also neighbors
			for(u8 q : cell.cage->cells)
				neighbors.insert(q);
		
		//values placed in neighbor cells cannot be duplicated
		for(u8 q : neighbors)
			cell.options.erase(cells[q].val);
		
		if(cell.cage)
			killer = true;
	}
	//Killer cages have special logic
	if(killer)
	{
		bool didsomething = false;
		do
		{
			didsomething = false;
			for(u8 index = 0; index < 9*9; ++index)
			{
				PuzzleCell& cell = cells[index];
				if(cell.val)
					continue; //skip filled cells
				if(!cell.cage)
					continue; //skip uncaged cells
				u8 target_sum = cell.cage->sum;
				if(cell.cage->cells.size() == 1)
				{
					//1-cell cage, just force the value
					for(auto it = cell.options.begin(); it != cell.options.end();)
					{
						if(*it == target_sum)
							++it;
						else it = cell.options.erase(it);
					}
					continue;
				}
				u8 lowest_sum = 0; //the sum of every cell's lowest option (excluding current cell)
				u8 highest_sum = 0; //the sum of every cell's highest option (excluding current cell)
				for(u8 q : cell.cage->cells)
				{
					if(q == index)
						continue; //don't count current cell
					if(u8 v = cells[q].val)
					{
						lowest_sum += v;
						highest_sum += v;
					}
					else if(!cells[q].options.empty())
					{
						auto& opts = cells[q].options;
						auto low_it = opts.begin();
						auto high_it = opts.rbegin();
						assert(low_it != opts.end() && high_it != opts.rend());
						lowest_sum += *low_it;
						highest_sum += *high_it;
					}
				}
				//Eliminate options based on sum clues
				for(auto it = cell.options.begin(); it != cell.options.end();)
				{
					auto v = *it;
					if(lowest_sum+v > target_sum //Values that would go over the target
						|| highest_sum+v < target_sum) //Values that fail to reach the target
					{
						it = cell.options.erase(it);
						didsomething = true;
					}
					else ++it;
				}
			}
		}
		while(didsomething);
	}
	set<u8> least_opts;
	u8 least_count = 9;
	for(u8 index = 0; index < 9*9; ++index)
	{
		PuzzleCell& cell = cells[index];
		if(cell.val)
			continue; //skip filled cells
		//Now that we've applied all of the rules
		// we check if this cell has the least options, including ties
		u8 sz = (u8)cell.options.size();
		if(sz < least_count)
		{
			least_opts.clear();
			least_count = sz;
		}
		if(sz == least_count)
			least_opts.insert(index);
		if(least_count == 0)
			break; //can early-return, as a 0 count indicates failure regardless
	}
	//Returns the set of cells that had the least number of options
	// paired with how many options they had
	return {least_opts, least_count};
}

bool PuzzleGrid::solve(bool check_unique)
{
	// if `check_unique` is true, the puzzle will be mangled,
	//     but the function will return if it has a unique solution.
	// else, the puzzle will be solved with a unique solution, returning success.
	for (PuzzleCell& c : cells)
		c.reset_opts();
	bool solved = false;
	vector<GridFillHistory> history;
	history.emplace_back(); //add first step
	while(true)
	{
		if(!program_running)
			throw ignore_exception();
		GridFillHistory& step = history.back();
		// Trim the options, accounting for anything we've already failed trying
		auto [rem,cnt] = trim_opts(step.checked);
		bool goback = cnt == 0;
		if(rem.empty() && !goback) //success
		{
			if(check_unique)
			{
				if(solved)
					return false;
				solved = true;
				goback = true;
			}
			else return true;
		}
		if(goback) //failure
		{
			// Step back, and mark this as a bad path
			history.pop_back();
			if(history.empty())
				break;
			GridFillHistory& prev = history.back();
			cells[prev.ind].clear();
			continue;
		}
		// continuing
		// Assign a random least-options cell to a random of its options
		step.ind = *rand(rem);
		PuzzleCell& c = cells[step.ind];
		c.val = *rand(c.options);
		step.checked[step.ind].insert(c.val);
		history.emplace_back(); //add the next step
	}
	return solved;
}

//Fills the grid with a random valid solution
void PuzzleGrid::populate()
{
	clear();
	solve(false);
	for(PuzzleCell& c : cells)
		c.sol = c.val;
}
//Fills the grid with random killer cages
void PuzzleGrid::killer_fill()
{
	cages.clear(); //clear any from prior failures
	//Generate random cages of size 1-5 filling the entire grid
	static const u8 lowsz = 2, highsz = 7;
	set<u8> remaining;
	for(u8 q = 0; q < 81; ++q)
		remaining.insert(q);
	while(!remaining.empty())
	{
		Cage& cage = cages.emplace_back();
		set<u8> values;
		u8 target_size = rand(lowsz,highsz);
		u8 seed = *rand(remaining);
		remaining.erase(seed);
		cage.cells.insert(seed);
		values.insert(cells[seed].sol);
		while(cage.cells.size() < target_size)
		{
			auto neighbors = cage.get_neighbors();
			erase_if(neighbors, [&](u8 v){return values.contains(cells[v].sol) || !remaining.contains(v);});
			if(neighbors.empty())
				break; //no possible room to grow, end early
			u8 next = *rand(neighbors);
			remaining.erase(next);
			cage.cells.insert(next);
			values.insert(cells[next].sol);
		}
	}
	for(Cage& cage : cages)
	{
		cage.sum = 0;
		for(u8 q : cage.cells)
		{
			cells[q].cage = &cage;
			cage.sum += cells[q].sol;
		}
	}
}
//Starting from a filled grid, trims away givens
void PuzzleGrid::build(Difficulty d)
{
	//Grid should be filled with a valid end solution before call
	set<u8> givens;
	for(u8 q = 0; q < 9*9; ++q)
	{
		if(!cells[q].given)
			throw puzzle_gen_exception("non-full grid cannot be built");
		else givens.insert(q);
	}
	//
	u8 target_givens = 81;
	u8 givens_for_cages = 0;
	bool killer_mode = false;
	switch(d)
	{
		case DIFF_EASY:
			target_givens = 46;
			break;
		case DIFF_NORMAL:
			target_givens = 35;
			break;
		case DIFF_HARD:
			target_givens = 26;
			break;
		case DIFF_KILLER:
			target_givens = 26;
			givens_for_cages = 26;
			killer_mode = true;
			break;
	}
	if(givens_for_cages < target_givens)
		givens_for_cages = target_givens;
	
	do
	{
		set<u8> killer_singles;
		if(target_givens)
		{
			vector<GridGivenHistory> history;
			history.emplace_back();
			bool backtrack = false;
			while(true)
			{
				if(!program_running)
					throw ignore_exception();
				if(backtrack)
				{
					if(history.back().built_cages)
						clear_cages();
					history.pop_back();
					if(history.empty())
						break;
					GridGivenHistory& prev = history.back();
					if(!prev.built_cages)
					{
						cells[prev.ind].given = true;
						givens.insert(prev.ind);
						//log(format("Backtracked {} / {}", givens.size(), target_givens));
					}
					backtrack = false;
				}
				GridGivenHistory& step = history.back();
				if(step.built_cages)
				{
					if(++step.built_cages >= 50)
					{
						backtrack = true;
						continue;
					}
					//log("Building cages!");
					killer_singles.clear();
					killer_fill();
					if(givens.size() == target_givens)
						return; //success!
					for(Cage& cage : cages)
						if(cage.cells.size() == 1)
							killer_singles.insert(*cage.cells.begin());
					if(killer_singles.size() > target_givens)
						continue;
					history.emplace_back();
					continue;
				}
				set<u8> possible = givens;
				for(u8 q : step.checked)
					possible.erase(q);
				for(u8 q : killer_singles)
					possible.erase(q);
				if(possible.empty()) //fail, need backtrack
				{
					backtrack = true;
					continue;
				}
				step.ind = *rand(possible);
				cells[step.ind].given = false;
				step.checked.insert(step.ind);
				if(!is_unique()) //fail, retry this step
				{
					cells[step.ind].given = true;
					continue;
				}
				givens.erase(step.ind);
				//log(format("Reached {} / {}", givens.size(), target_givens));
				history.emplace_back();
				if(killer_mode && givens.size() == givens_for_cages)
					++(history.back().built_cages);
				else if(givens.size() == target_givens)
					return; //success!
			}
		}
		else
		{
			for(auto& cell : cells)
				cell.given = false;
			if(is_unique())
				return; //success!
		}
	}
	while(killer_mode); //killer mode retries from start on failure
	throw puzzle_gen_exception("grid build error");
}



void PuzzleGrid::print() const
{
	u8 ind = 0;
	for(PuzzleCell const& c : cells)
	{
		std::cout << (c.given ? u16(c.val) : 0);
		if(ind % 9 == 8)
			std::cout << std::endl;
		else
		{
			std::cout << " ";
			if(ind % 3 == 2)
				std::cout << "   ";
		}
		++ind;
	}
}

void PuzzleGrid::print_cages() const
{
	map<u8,i8> cell_to_cage;
	for(u8 q = 0; q < 81; ++q)
		cell_to_cage[q] = -1;
	for(u8 q = 0; q < cages.size(); ++q)
	{
		auto& cage = cages[q];
		for(u8 c : cage.cells)
			cell_to_cage[c] = q;
	}
	for(auto [ind,cage] : cell_to_cage)
	{
		std::cout << std::setw(2) << std::to_string(cage);
		if(ind % 9 == 8)
			std::cout << std::endl;
		else
		{
			std::cout << " ";
			if(ind % 3 == 2)
				std::cout << "   ";
		}
	}
}

void PuzzleGrid::print_sol() const
{
	u8 ind = 0;
	for(PuzzleCell const& c : cells)
	{
		std::cout << u16(c.sol);
		if(ind % 9 == 8)
			std::cout << std::endl;
		else
		{
			std::cout << " ";
			if(ind % 3 == 2)
				std::cout << "   ";
		}
		++ind;
	}
}

BuiltPuzzle gen_puzzle(Difficulty d)
{
	return PuzzleGenFactory::get(d);
}

}

