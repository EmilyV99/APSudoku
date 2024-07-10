#pragma once

#include <numbers>
#include "Main.hpp"

using std::numbers::pi;

u32 col_to_hex(ALLEGRO_COLOR const& c);
u32 col_to_hex(ALLEGRO_COLOR const* c);
ALLEGRO_COLOR hex_to_col(u32 val);

constexpr u32 rgb_to_hex(u8 r, u8 g, u8 b)
{
	return (r<<24)|(g<<16)|(b<<8)|0xFF;
}
constexpr u32 rgba_to_hex(u8 r, u8 g, u8 b, u8 a)
{
	return (r<<24)|(g<<16)|(b<<8)|(a<<0);
}

template<typename T>
string set_string(set<T> const& cont)
{
	stringstream s;
	s << "{ ";
	bool first = true;
	for(T const& val : cont)
	{
		if(!first)
			s << ", ";
		else first = false;
		s << val;
	}
	s << " }";
	return s.str();
}

template<typename T>
T vbound(T v, T low, T high)
{
	if(low > high)
	{
		T tmp = low;
		low = high;
		high = tmp;
	}
	if(v < low)
		return low;
	if(v > high)
		return high;
	return v;
}
template<typename T>
T vbound(T v, T low, T high, bool& bounded)
{
	if(low > high)
	{
		T tmp = low;
		low = high;
		high = tmp;
	}
	bounded = true;
	if(v < low)
		return low;
	if(v > high)
		return high;
	bounded = false;
	return v;
}

template<typename T>
T* rand(set<T>& s)
{
	if(s.empty()) return nullptr;
	size_t indx = rand(s.size());
	auto it = s.begin();
	std::advance(it,indx);
	return const_cast<T*>(&*it);
}

inline double vectorX(double len, double angle)
{
	return cos(angle)*len;
}

inline double vectorY(double len, double angle)
{
	return sin(angle)*len;
}

constexpr double deg_to_rad(double degrees)
{
	return degrees * (pi/180.0);
}
constexpr double operator ""_deg(unsigned long long int degrees)
{
	return deg_to_rad(degrees);
}
inline double operator ""_deg(char const* degrees)
{
	return deg_to_rad(std::stod(degrees));
}


