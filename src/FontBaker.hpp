// Switch host: builds the glyph tables TH11's GlyphAtlas reads (T11G fonts,
// the CP932 map and the 4-bit blend table) with FreeType.
//
// Upstream bakes these offline with Windows GDI (th11_web/scripts/
// prepare-fonts.mjs) because the original renders dialogue with CreateFont +
// TextOut. Here the same four CreateFont requests - MS Gothic 32/400,
// MS Mincho 32/600, MS Gothic 15/700, MS Mincho 15/700 - are rasterised with
// FreeType from the user's own font files. Coverage is quantised to 16
// classes; blend4444 mixes those like GDI's antialiased text on ARGB4444.
#pragma once
#include "../game/Types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace th11::host {
struct BakedFonts {
    std::vector<u8> files[6]; // font0..3.bin, cp932.bin, blend4444.bin
    std::string source;       // what was used, for the log
};
// Returns false with `error` set when no usable font was found.
// `progress` (optional) receives 0..1 while the glyphs are rasterised.
bool bake_fonts(BakedFonts& out,std::string& error,const std::function<void(float)>& progress=nullptr);
// The CP932 -> UTF-16 map the upstream baker derives from shift_jis.
const u16* cp932_table();
}
