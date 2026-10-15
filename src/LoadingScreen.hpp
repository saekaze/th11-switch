// Switch port: the original start-up screen while TH11 loads, built the way
// TH10's start-up builds it (StartupScreen, 0x41f990 / 0x41fdd0 in
// YomotsuHisami/th10): sig.anm script 0 (the "東方Project / 11th" picture)
// and ascii.anm script 6 at (480,392) (少女祈祷中). Both come from the player's
// th11.dat and run on their own AnmManager, so nothing the game computes
// changes. If either is missing, start-up keeps the plain bar. The idea and
// the TH15 version come from シエル's PSP ports.
#pragma once
#include "GraphicsDevice.hpp"
#include "../game/AnmManager.hpp"
#include "../game/AnmRenderer.hpp"
#include "../game/GameResources.hpp"
#include "../game/SceneCamera.hpp"
#include <set>
#include <string>
namespace th11::sdl {
class LoadingScreen {
    GraphicsDevice& graphics;AnmRenderer& renderer;AnmManager manager;AnmResource sig,ascii;SceneCamera camera;
    std::set<std::string> drawn;double clock=0;bool active=false,loaded=false;
    void show();
public:
    LoadingScreen(GraphicsDevice& g,AnmRenderer& r):graphics(g),renderer(r){}
    ~LoadingScreen(){stop();}
    bool start(GameResources& resources);   // th11.dat is open
    bool start(AnmResource&& sig_file,AnmResource&& ascii_file);
    void pump();    // between loading steps: draws when a 60 Hz frame is due
    void stop();
    bool running()const noexcept{return active;}
};
}
