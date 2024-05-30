#pragma once

#include "Main.hpp"

enum Bold
{
	BOLD_LIGHT,
	BOLD_NONE,
	BOLD_SEMI,
	BOLD_NORMAL,
	BOLD_EXTRA,
	NUM_BOLDS
};
struct FontDef : public tuple<i16,bool,Bold> // A definition of a font
{
	i16& height() {return std::get<0>(*this);}
	i16 const& height() const {return std::get<0>(*this);}
	bool& italic() {return std::get<1>(*this);}
	bool const& italic() const {return std::get<1>(*this);}
	Bold& weight() {return std::get<2>(*this);}
	Bold const& weight() const {return std::get<2>(*this);}
	
	u16 pix_height() const
	{
		assert(height() < 0);
		return -height();
	}
	
	FontDef(i16 h = -20, bool ital = false, Bold b = BOLD_NONE);
	ALLEGRO_FONT* get() const;
	ALLEGRO_FONT* gen() const;
	//Unscaled, so, canvas-size
	ALLEGRO_FONT* get_base() const;
	ALLEGRO_FONT* gen_base() const;
};
enum Font
{
	FONT_ANSWER,
	FONT_MARKING5,
	FONT_MARKING6,
	FONT_MARKING7,
	FONT_MARKING8,
	FONT_MARKING9,
	NUM_FONTS
};
struct FontRef
{
	FontDef const& get() const;
	FontRef();
	FontRef(FontDef const& f);
	FontRef(const Font indx);
	operator FontDef const&() const {return get();}
private:
	optional<FontDef> f;
	optional<Font> indx;
};
extern FontDef fonts[NUM_FONTS];
void scale_fonts();