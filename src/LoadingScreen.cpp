#include "LoadingScreen.hpp"
#include "Platform.hpp"
#include <SDL.h>
#include <utility>
namespace th11::sdl {
bool LoadingScreen::start(GameResources& resources){
    AnmResource sig_file,ascii_file;
    if(!resources.open_anm("sig.anm",sig_file)||!resources.open_anm("ascii.anm",ascii_file)){host::log("loading screen: sig.anm / ascii.anm unavailable");return false;}
    return start(std::move(sig_file),std::move(ascii_file));
}
bool LoadingScreen::start(AnmResource&& sig_file,AnmResource&& ascii_file){
    stop();sig=std::move(sig_file);ascii=std::move(ascii_file);
    std::string names;for(const auto& t:ascii.textures)names+=" "+t.name;
    host::log("loading screen: sig.anm %zu scripts, ascii.anm %zu scripts, textures:%s",sig.scripts.size(),ascii.scripts.size(),names.c_str());
    if(sig.scripts.empty()||ascii.scripts.size()<7){host::log("loading screen: scripts missing, keeping the bar");return false;}
    if(!graphics.preload(sig)||!graphics.preload(ascii)){host::log("loading screen: %s",graphics.error.c_str());graphics.unload(sig);graphics.unload(ascii);return false;}
    loaded=true;
    const Vec3 at{480,392,0};
    if(!manager.create(sig,0,0,0)||!manager.create(ascii,6,0,0,false,false,&at)){host::log("loading screen: script failed (opcode %u)",manager.last_error);stop();return false;}
    camera={};camera.mode=1;active=true;clock=host::seconds();show();return true;
}
void LoadingScreen::pump(){
    if(!active)return;const double now=host::seconds();u32 frames=u32((now-clock)*60);if(!frames)return;
    if(frames>4){frames=4;clock=now;}else clock+=frames/60.0;
    for(u32 f=0;f<frames;f++)if(!manager.update(false)||!manager.update(true)){host::log("loading screen: animation stopped (opcode %u)",manager.last_error);manager.clear();break;}
    show();
}
void LoadingScreen::stop(){
    if(active){std::string names;for(const auto& n:drawn)names+=" "+n;host::log("loading screen: drew%s",names.empty()?" nothing":names.c_str());}
    active=false;manager.clear();drawn.clear();
    if(loaded){renderer.flush();renderer.invalidate();graphics.unload(sig);graphics.unload(ascii);loaded=false;}
}
void LoadingScreen::show(){
    renderer.invalidate();if(!renderer.select_target(nullptr,0)||!renderer.clear_target(0xff000000))return;
    renderer.offset={};renderer.tint_enabled=false;renderer.set_camera(camera,true);renderer.pipeline().fog=false;
    for(u32 layer=0;layer<31;++layer){
        for(auto* vm=manager.layer_first(layer);vm;vm=vm->draw_next)if(vm->sprite&&vm->resource&&vm->sprite->texture<vm->resource->textures.size()&&drawn.size()<16)drawn.insert(vm->resource->textures[vm->sprite->texture].name);
        if(renderer.draw_layer(manager.layer_first(layer))==-2){host::log("loading screen: draw failed");manager.clear();break;}
    }
    renderer.flush();graphics.present();SDL_PumpEvents();
}
}
