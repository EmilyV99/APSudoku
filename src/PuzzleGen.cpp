#include "Main.hpp"
#include "GUI.hpp"
#include "PuzzleGen.hpp"
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
	static PuzzleQueue puzzles[3];
	
	Difficulty d;
	volatile bool running;
	std::thread runtime;
	
	void run();
	
	PuzzleGenFactory() = default;
	PuzzleGenFactory(Difficulty d) : d(d), running(false),
		runtime()
	{}
	#define NUM_E_FAC 1
	#define NUM_M_FAC 1
	#define NUM_H_FAC 3
	static PuzzleGenFactory e_factories[NUM_E_FAC];
	static PuzzleGenFactory m_factories[NUM_M_FAC];
	static PuzzleGenFactory h_factories[NUM_H_FAC];
	static set<PuzzleGenFactory*> factories;
};
PuzzleGenFactory PuzzleGenFactory::e_factories[NUM_E_FAC];
PuzzleGenFactory PuzzleGenFactory::m_factories[NUM_M_FAC];
PuzzleGenFactory PuzzleGenFactory::h_factories[NUM_H_FAC];
set<PuzzleGenFactory*> PuzzleGenFactory::factories;
PuzzleQueue PuzzleGenFactory::puzzles[3];

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
			//
			BuiltPuzzle puz;
			for(PuzzleCell const& cell : puzzle.cells)
				puz.emplace_back(cell.val, cell.given);
			
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
	: val(0), given(true),
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
	set<u8> checked;
	GridGivenHistory() : ind(0), checked() {}
};

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
PuzzleGrid PuzzleGrid::given_copy(PuzzleGrid const& g)
{
	PuzzleGrid ret;
	for(u8 q = 0; q < 9*9; ++q)
	{
		ret.cells[q].given = g.cells[q].given;
		if(ret.cells[q].given)
			ret.cells[q].val = g.cells[q].val;
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
	set<u8> least_opts;
	u8 least_count = 9;
	for(u8 index = 0; index < 9*9; ++index)
	{
		PuzzleCell& cell = cells[index];
		if(cell.val)
		{
			cell.options.clear();
			continue; //skip filled cells
		}
		cell.options = {1,2,3,4,5,6,7,8,9};
		auto banset = banned.find(index);
		if(banset != banned.end())
			for(u8 val : banset->second)
				cell.options.erase(val);
		u8 col = index%9;
		u8 row = index/9;
		u8 box = 3*(row/3)+(col/3);
		set<u8> neighbors;
		for(u8 q = 0; q < 9; ++q)
		{
			neighbors.insert(9*q + col);
			neighbors.insert(9*row + q);
			neighbors.insert(9*(3*(box/3) + (q/3)) + (3*(box%3) + (q%3)));
		}
		for(u8 q : neighbors)
			cell.options.erase(cells[q].val);
		u8 sz = (u8)cell.options.size();
		if(sz < least_count)
		{
			least_opts.clear();
			least_count = sz;
		}
		if(sz == least_count)
			least_opts.insert(index);
	}
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
	u8 target_givens;
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
	}
	vector<GridGivenHistory> history;
	history.emplace_back();
	while(true)
	{
		if(!program_running)
			throw ignore_exception();
		GridGivenHistory& step = history.back();
		set<u8> possible = givens;
		for(u8 q : step.checked)
			possible.erase(q);
		if(possible.empty()) //fail, need backtrack
		{
			history.pop_back();
			if(history.empty())
				break;
			GridGivenHistory& prev = history.back();
			cells[prev.ind].given = true;
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
		if(givens.size() == target_givens)
			return; //success!
		history.emplace_back();
	}
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
				std::cout << "  ";
		}
		++ind;
	}
}

void PuzzleGrid::print_sol() const
{
	u8 ind = 0;
	for(PuzzleCell const& c : cells)
	{
		std::cout << u16(c.val);
		if(ind % 9 == 8)
			std::cout << std::endl;
		else
		{
			std::cout << " ";
			if(ind % 3 == 2)
				std::cout << "  ";
		}
		++ind;
	}
}

BuiltPuzzle gen_puzzle(Difficulty d)
{
	return PuzzleGenFactory::get(d);
}

}

