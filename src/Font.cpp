#include "Main.hpp"
#include "GUI.hpp"
#include "Font.hpp"

constexpr double render_font_scale()
{
	return std::min(render_xscale,render_yscale);
}

FontDef fonts[NUM_FONTS];

map<FontDef,ALLEGRO_FONT*> fontmap;
map<FontDef,ALLEGRO_FONT*> fontmap_base;
FontDef::FontDef(i16 h, bool ital, Bold b)
	: tuple<i16,bool,Bold>(h,ital,b)
{}
ALLEGRO_FONT* FontDef::get() const
{
	auto it = fontmap.find(*this);
	if(it != fontmap.end()) //Font already generated
		return it->second;
	return gen();
}
ALLEGRO_FONT* FontDef::gen() const
{
	auto it = fontmap.find(*this);
	if(it != fontmap.end()) //font already exists, don't leak
		al_destroy_font(it->second);
	string weightstr;
	switch(weight())
	{
		case BOLD_NONE:
			weightstr = italic() ? "" : "Regular";
			break;
		case BOLD_LIGHT:
			weightstr = "Light";
			break;
		case BOLD_SEMI:
			weightstr = "Semibold";
			break;
		case BOLD_NORMAL:
			weightstr = "Bold";
			break;
		case BOLD_EXTRA:
			weightstr = "ExtraBold";
			break;
	}
	if(italic())
		weightstr += "Italic";
	string fontstr = "assets/font_opensans/OpenSans-"+weightstr+".ttf";
	al_set_new_bitmap_flags(0);
	ALLEGRO_FONT* f = al_load_ttf_font(fontstr.c_str(), render_font_scale()*height(), 0);
	fontmap[*this] = f;
	return f;
}
ALLEGRO_FONT* FontDef::get_base() const
{
	auto it = fontmap_base.find(*this);
	if(it != fontmap_base.end()) //Font already generated
		return it->second;
	return gen_base();
}
ALLEGRO_FONT* FontDef::gen_base() const
{
	auto it = fontmap_base.find(*this);
	if(it != fontmap_base.end()) //font already exists, don't leak
		al_destroy_font(it->second);
	string weightstr;
	switch(weight())
	{
		case BOLD_NONE:
			weightstr = italic() ? "" : "Regular";
			break;
		case BOLD_LIGHT:
			weightstr = "Light";
			break;
		case BOLD_SEMI:
			weightstr = "Semibold";
			break;
		case BOLD_NORMAL:
			weightstr = "Bold";
			break;
		case BOLD_EXTRA:
			weightstr = "ExtraBold";
			break;
	}
	if(italic())
		weightstr += "Italic";
	string fontstr = "assets/font_opensans/OpenSans-"+weightstr+".ttf";
	al_set_new_bitmap_flags(0);
	ALLEGRO_FONT* f = al_load_ttf_font(fontstr.c_str(), height(), 0);
	fontmap_base[*this] = f;
	return f;
}

FontDef const& FontRef::get() const
{
	if(f) return *f;
	if(indx && unsigned(*indx) < NUM_FONTS) return fonts[*indx];
	
	static FontDef def_ret(-20, false, BOLD_NONE);
	error("INVALID COLOR ACCESS! Returning default font.");
	return def_ret;
}
FontRef::FontRef()
	: f(), indx()
{}
FontRef::FontRef(FontDef const& f)
	: f(f), indx()
{}
FontRef::FontRef(const Font indx)
	: f(), indx(indx)
{}

void scale_fonts()
{
	for(auto pair : fontmap)
		pair.first.gen();
}

