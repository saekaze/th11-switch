#pragma once
#include "GraphicsDevice.hpp"
#include "../game/TextRaster.hpp"
#include "../game/Dialogue.hpp"
#include <functional>
namespace th11::sdl {
class FontDevice {
    GraphicsDevice& graphics;TextRaster raster;bool ready=false;
public:
    explicit FontDevice(GraphicsDevice& g):graphics(g){}
    std::string error;
    u32 writes=0;
    // Switch port: loading-bar hook for the first-launch glyph bake.
    std::function<void(float)> progress;
    bool initialize();
    bool text(AnmVm&,const DialogueText&);
};
}
