// Touhou 11 for Nintendo Switch: native entry point.
//
// The Application struct is th11_web/cpp/sdl/Application.cpp with the
// browser removed: the same session wiring, audio/text events, replay and
// score saving. Host changes: files live on the SD card instead of IDBFS,
// the 60 Hz loop is driven by vsync instead of requestAnimationFrame, and
// Switch buttons / the touch screen feed the upstream KeyboardMap and
// TouchController exactly like the web launcher's key and touch forwarding.
#include "GraphicsDevice.hpp"
#include "AudioDevice.hpp"
#include "FontDevice.hpp"
#include "Platform.hpp"
#include "../game/MusicCatalog.hpp"
#include "../game/GameSession.hpp"
#include "../game/FrameStatistics.hpp"
#include "../game/AnmRenderer.hpp"
#include "../portable/sdl/FrameCadence.hpp"
#include "../portable/input/TouchController.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <memory>
#include <vector>
#include <dirent.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace th11::sdl {
namespace {
using host::paths;
std::string save_path(const char* name){return paths().save_file(name);}

struct Application:StageResourceEffects {
    GraphicsDevice graphics;
    AudioDevice audio;
    FontDevice fonts{graphics};
    GameResources resources;
    GameSession session;
    FrameStatistics frame_statistics;
    AsciiText frame_text{session.resources.core.ascii};
    AnmRenderer renderer;
    std::vector<u8> archive;
    std::vector<u8> saved_score_state,saved_config_state;
    bool initialized=false;
    std::string error;

    Application():renderer(graphics) {session.resources.effects=this;}
    void reset_frame_window(){frame_statistics.window_start=host::seconds();frame_statistics.frames=0;}
    bool read_file(const std::string& path,std::vector<u8>& data,u32 limit){return host::read_file(path,data,limit);}
    bool write_file(const std::string& path,const std::vector<u8>& data){
        if(!host::write_file(path,data)){error="Unable to write "+path;return false;}return true;
    }
    bool save_scores(){
        if(!initialized)return true;
        if(!session.title)session.scores.write_records(session.spell_records,session.clear_records);
        std::vector<u8> state;state.reserve(7*0x68d4+0x448);
        for(const auto& character:session.scores.characters)state.insert(state.end(),character.begin(),character.end());
        state.insert(state.end(),session.scores.settings.begin(),session.scores.settings.end());
        if(state!=saved_score_state){std::vector<u8> data;if(!session.scores.save(data)){error="Unable to encode scores";return false;}
            if(!write_file(save_path("scoreth11.dat"),data))return false;saved_score_state=std::move(state);}
        const auto& cfg=session.config.bytes;std::vector<u8> config(cfg.begin(),cfg.end());
        if(config!=saved_config_state){if(!write_file(save_path("th11.cfg"),config))return false;saved_config_state=std::move(config);}
        return true;
    }
    bool save_replay(u32 slot,const char* name){
        if(slot>99||!name||std::strlen(name)>8)return false;std::vector<u8> data;
        if(!session.save_replay(name,data,true,frame_statistics.slowdown())){error="No active recording to save";return false;}
        const bool touch=session.recording.uses_touch();char file[80];std::snprintf(file,sizeof(file),"replay/th11_%.2u.%s",slot,touch?"rpyx":"rpy");
        if(!write_file(save_path(file),data))return false;
        std::snprintf(file,sizeof(file),"replay/th11_%.2u.%s",slot,touch?"rpy":"rpyx");std::remove(save_path(file).c_str());return true;
    }
    bool stage_music_held()const{return session.state.stage==6&&session.battle&&session.battle->frame<300&&!session.state.demo;}
    bool play_stage_music(u32 stage){
        if(session.state.demo)return true;
        if(!audio.music(stage_music(stage))){error=audio.error;return false;}
        audio.pause_music(stage==6);return true;
    }
    bool pause(){if(!session.pause())return false;if(!stage_music_held())audio.pause_music(true);return true;}
    bool resume(){if(!session.resume())return false;if(!stage_music_held())audio.pause_music(false);return true;}
    bool audio_events(){
        if(session.ending){auto& ending=*session.ending;for(const auto sound:ending.sounds)audio.effects.enqueue(sound);
            if(ending.music_request>=0){audio.pause_music(false);if(!audio.music(ending.music_request)){error=audio.error;return false;}}
            if(ending.music_fade>=0)audio.fade_music(ending.music_fade);
        }
        if(session.pause_menu){for(const auto sound:session.pause_menu->sounds)audio.effects.enqueue(sound);session.pause_menu->sounds.clear();}
        if(session.title){
            for(const auto sound:session.title->sounds)audio.effects.enqueue(sound);
            if(session.title->music_pause)audio.pause_music(true);
            if(session.title->music_request>=0){audio.pause_music(false);if(!audio.music(session.title->music_request)){error=audio.error;return false;}}
            if(session.title->volume_changed){audio.music_volume=session.config.music_volume();audio.effects.master_volume=session.config.sound_volume();audio.refresh_volume();}
            session.title->sounds.clear();
        }
        if(!session.battle)return true;
        for(const auto& event:session.battle->events){
            switch(event.kind){
            case BattleEventKind::Sound:
                if(event.positional)audio.effects.positioned(event.value,event.position.x);else audio.effects.enqueue(event.value);
                break;
            case BattleEventKind::StopSound:audio.effects.stop(event.value);break;
            case BattleEventKind::StageMusic:if(!play_stage_music(event.value))return false;break;
            case BattleEventKind::BossMusic:
                if(!audio.music(stage_music(event.value,true))){error=audio.error;return false;}break;
            case BattleEventKind::MusicResume:audio.pause_music(false);break;
            case BattleEventKind::MusicFade:audio.fade_music(i32(event.position.x*60.f));break;
            default:break;
            }
        }
        session.battle->events.clear();return true;
    }
    bool text_events(){
        if(session.ending){renderer.flush();for(const auto& request:session.ending->text_requests){auto* vm=session.animations.find(request.animation);if(vm&&!fonts.text(*vm,request)){error=fonts.error;return false;}}session.ending->text_requests.clear();}
        if(session.title){renderer.flush();for(const auto& request:session.title->text_requests){auto* vm=session.animations.find(request.animation);if(vm&&!fonts.text(*vm,request)){error=fonts.error;return false;}}session.title->text_requests.clear();}
        if(!session.battle)return true;
        renderer.flush();
        for(const auto& request:session.battle->dialogue_text_requests){auto* vm=session.animations.find(request.animation);if(vm&&!fonts.text(*vm,request)){error=fonts.error;return false;}}
        session.battle->dialogue_text_requests.clear();return true;
    }
    bool prepare(StageResources& stage)override{
        renderer.flush();
        for(auto* file:{&stage.background,&stage.logo,&stage.enemies})if(!graphics.preload(*file)){error=graphics.error;return false;}
        return true;
    }
    void release(StageResources& stage)override{
        renderer.flush();renderer.invalidate();
        for(auto* file:{&stage.background,&stage.logo,&stage.enemies})graphics.unload(*file);
    }
    bool prepare_animation(AnmResource& file)override{renderer.flush();return graphics.preload(file);}
    void release_animation(AnmResource& file)override{renderer.flush();renderer.invalidate();graphics.unload(file);}
    bool read_replay(const std::string& path,std::vector<u8>& bytes)override{return read_file(path,bytes,64*1024*1024);}

    // Switch port: a simple bar while start-up reads th11.dat and (first
    // launch only) bakes the text glyphs; later launches reuse fontcache/.
    void loading_bar(float fraction){
        static double last=-1;const double now=host::seconds();if(fraction>0&&fraction<1&&now-last<1.0/30)return;last=now;
        auto& gpu=graphics.backend;gpu.state.target=GraphicsDevice::screen;gpu.state.depth=GraphicsDevice::depth;gpu.viewport({0,0,640,480,0,1});
        const i32 x0=160,x1=480,y0=400,y1=412,fill=x0+2+i32(float(x1-x0-4)*std::clamp(fraction,0.f,1.f));
        gpu.clear(1,0xff000000,1,0);const i32 frame[]{x0,y0,x1,y1};gpu.clear(1,0xff606060,1,0,frame,1);
        const i32 inner[]{x0+2,y0+2,x1-2,y1-2};gpu.clear(1,0xff000000,1,0,inner,1);
        if(fill>x0+2){const i32 bar[]{x0+2,y0+2,fill,y1-2};gpu.clear(1,0xffe0e0e0,1,0,bar,1);}
        gpu.present(GraphicsDevice::screen);SDL_PumpEvents();
    }
    bool read_archive() {
        const auto path=paths().data_file("th11.dat");
        if(!read_file(path,archive,256*1024*1024)||archive.empty()){error="Unable to read "+path;return false;}
        return resources.open_archive(archive.data(),u32(archive.size()));
    }

    bool initialize() {
        if(initialized){if(!audio.initialize(resources)){error=audio.error;return false;}return true;}
        if(!graphics.initialize()){error=graphics.error;return false;}
        loading_bar(0);double begin=host::seconds();
        if(!read_archive()){if(error.empty())error=resources.error();return false;}
        host::log("th11.dat read in %.1fs",host::seconds()-begin);loading_bar(.1f);
        if(!audio.initialize(resources)){error=audio.error;return false;}
        fonts.progress=[this](float f){loading_bar(.1f+.85f*f);};const bool fonts_ok=fonts.initialize();fonts.progress=nullptr;
        if(!fonts_ok){error=fonts.error;return false;}
        loading_bar(.95f);begin=host::seconds();
        host::make_directory(paths().save);host::make_directory(save_path("replay"));
        if(host::exists(save_path("scoreth11.dat"))){std::vector<u8> saved;if(!read_file(save_path("scoreth11.dat"),saved,4*1024*1024)||!session.load_scores(saved.data(),u32(saved.size()))){error="Invalid scoreth11.dat: "+session.scores.error;return false;}}
        if(host::exists(save_path("th11.cfg"))){std::vector<u8> saved;if(!read_file(save_path("th11.cfg"),saved,60)||!session.config.open(saved.data(),u32(saved.size()))){error="Invalid th11.cfg";return false;}saved_config_state=saved;}
        audio.music_volume=session.config.music_volume();audio.effects.master_volume=session.config.sound_volume();audio.refresh_volume();
        session.recording_timestamp=u64(std::time(nullptr));
        if(!session.open_title(resources,true)){error=session.error;return false;}
        auto& core=session.resources.core;
        for(AnmResource* file:{&core.text,&core.ascii,&core.bullet,&core.enemy,&core.front,&core.title,&core.title_variant,&core.players[0],&core.players[1]})
            if(!graphics.preload(*file)){error=graphics.error;return false;}
        renderer.viewport={0,0,640,480,0,1};
        // GameBattle's STD owner creates backdrop VMs and publishes its camera.
        host::log("title prepared in %.1fs",host::seconds()-begin);
        frame_statistics.window_start=host::seconds();
        initialized=true;return audio_events()&&text_events();
    }

    bool tick(u32 held,bool pause_key=false) {
        if(!initialize())return false;
        const auto before=session.state.phase;
        const GameSessionInput input{held,0,0,pause_key,bool(held&0x100),bool(held&0x2),frame_statistics.fps};
        if(!session.update(input)){error=session.error;return false;}
        const auto after=session.state.phase;
        if(before!=GameSessionPhase::stage&&after==GameSessionPhase::stage&&session.state.frame==0)frame_statistics.reset_run();
        if(session.pause_menu)session.pause_menu->slowdown=frame_statistics.slowdown();
        if(session.title&&session.title->screen==TitleScreen::Results)session.title->result_slowdown=frame_statistics.slowdown();
        if(before!=GameSessionPhase::stage&&after==GameSessionPhase::stage){
            if(before==GameSessionPhase::paused&&session.state.frame!=0){if(!stage_music_held())audio.pause_music(false);}
            else{audio.effects.reset();if(!play_stage_music(session.state.stage))return false;}
        }
        if(before!=GameSessionPhase::paused&&after==GameSessionPhase::paused&&!stage_music_held())audio.pause_music(true);
        if(before!=GameSessionPhase::game_over&&after==GameSessionPhase::game_over){session.pause_menu->timestamp=u64(std::time(nullptr));audio.pause_music(false);if(!audio.music(17)){error=audio.error;return false;}}
        if(before==GameSessionPhase::ending&&session.title)session.title->result_timestamp=u64(std::time(nullptr));
        if(session.title&&session.title->replay_scan_requested)scan_replays();
        if(session.title&&session.title->screen==TitleScreen::ReplaySave){auto& menu=*session.title;
            if(menu.substate==3&&!menu.pending_replay){const u64 stamp=u64(std::time(nullptr));std::memcpy(session.recording.header.data()+12,&stamp,8);auto entry=std::make_shared<ReplayMenuEntry>();
                if(!session.save_replay("        ",entry->file,true,frame_statistics.slowdown())||!entry->replay.open(entry->file.data(),u32(entry->file.size()))){error="Completed replay metadata unavailable";return false;}menu.pending_replay=std::move(entry);
            }
            if(menu.replay_save_requested){if(!save_replay(menu.replay_file+1,menu.entered_name.data()))return false;menu.replay_save_requested=false;menu.pending_replay.reset();scan_replays();if(!save_scores())return false;}
        }
        if(session.pause_menu){auto& menu=*session.pause_menu;
            if(menu.capture_requested){
                auto* vm=session.animations.find(menu.background_animation);if(!vm||!vm->sprite||!vm->resource){error="Pause screenshot sprite missing";return false;}
                const auto& sprite=*vm->sprite;const u32 target=graphics.texture(*vm->resource,sprite.texture);renderer.flush();
                const i32 from[]={32,16,416,464},to[]={i32(sprite.x),i32(sprite.y),i32(sprite.x+sprite.width),i32(sprite.y+sprite.height)};
                if(!graphics.backend.resample(GraphicsDevice::screen,from,target,to,nullptr,0,0)){error="Pause GPU capture failed";return false;}
                graphics.changed(target);renderer.invalidate();menu.capture_requested=false;
            }
            if(menu.scan_requested)scan_replays();
            if(menu.recording_metadata_requested){menu.timestamp=u64(std::time(nullptr));std::memcpy(session.recording.header.data()+12,&menu.timestamp,8);}
            if(menu.save_requested){if(!save_replay(menu.cursor.selected+1,menu.entered_name.data()))return false;menu.save_requested=false;scan_replays();if(!save_scores())return false;}
        }
        if(session.battle&&after==GameSessionPhase::stage&&!session.battle->spell_timing.update(session.battle->spell_flags,session.battle->spells.frame_count,host::seconds(),session.state.replay)){error="Invalid spell timing state";return false;}
        if(!audio_events()||!text_events())return false;
        audio.update();audio.pump();
        frame_statistics.sample(host::seconds(),after==GameSessionPhase::stage&&session.battle&&session.battle->stage_active&&!session.state.replay);
        frame_text.clear();const auto label=frame_statistics.label();frame_text.add(label.text.c_str(),label.position,label.style);
        if(!session.draw(renderer,&frame_text)){error=session.error;return false;}
        graphics.present();
        return true;
    }
    void scan_replays(){
        if(!session.title&&!session.pause_menu)return;
        auto load=[&](u32 slot,const std::string& path){std::vector<u8> bytes;if(!read_file(path,bytes,64*1024*1024))return;
            auto entry=std::make_shared<TitleMenu::ReplayEntry>();if(!entry->replay.open(bytes.data(),u32(bytes.size())))return;
            entry->path=path;entry->replay.retain_metadata();if(session.title)session.title->replay_files[slot]=std::move(entry);else if(slot<25)session.pause_menu->replay_files[slot]=std::move(entry);};
        for(u32 slot=0;slot<25;++slot){char file[80];std::snprintf(file,sizeof(file),"replay/th11_%.2u.rpy",slot+1);const auto path=save_path(file);load(slot,path);load(slot,path+"x");}
        if(session.title&&session.title->screen==TitleScreen::Replays){std::vector<std::string> names;auto* dir=opendir(save_path("replay").c_str());if(dir){while(auto* e=readdir(dir)){const std::string n=e->d_name;if(n.size()>=15&&n.rfind("th11_ud",0)==0&&(n.substr(11)==".rpy"||n.substr(11)==".rpyx"))names.push_back(n);}closedir(dir);}
            std::sort(names.begin(),names.end());for(u32 i=0;i<names.size()&&i<50;++i)load(25+i,save_path("replay/")+names[i]);}
        if(session.title&&session.title->screen==TitleScreen::Replays)session.title->replay_scan_complete();
    }
    // Switch port: '−' saves the current 640x480 frame as a BMP into
    // snapshot/ next to the saves, like the PC game's snapshot folder.
    bool snapshot(){
        renderer.flush();graphics.backend.read(GraphicsDevice::screen);auto* image=graphics.pixels(GraphicsDevice::screen);if(!image)return false;
        host::make_directory(save_path("snapshot"));char file[64];std::string path;
        for(u32 n=0;n<1000;++n){std::snprintf(file,sizeof(file),"snapshot/th11_%03u.bmp",n);path=save_path(file);if(!host::exists(path))break;}
        const u32 w=image->width,h=image->height,row=w*3,pad=(4-row%4)%4,size=54+(row+pad)*h;std::vector<u8> bmp(size,0);
        auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)bmp[at+i]=u8(v>>(8*i));};
        bmp[0]='B';bmp[1]='M';put(2,size,4);put(10,54,4);put(14,40,4);put(18,w,4);put(22,h,4);put(26,1,2);put(28,24,2);put(34,size-54,4);
        for(u32 y=0;y<h;++y){const u8* src=image->pixels.data()+(h-1-y)*image->pitch;u8* dst=bmp.data()+54+y*(row+pad);for(u32 x=0;x<w;++x){dst[x*3]=src[x*4];dst[x*3+1]=src[x*4+1];dst[x*3+2]=src[x*4+2];}}
        const bool ok=host::write_file(path,bmp);host::log("snapshot %s: %s",path.c_str(),ok?"ok":"failed");return ok;
    }
};
std::unique_ptr<Application> app;
struct Key {const char* code;const char* sdl;u32 scan,vk;bool hosted=false;SDL_Scancode native=SDL_SCANCODE_UNKNOWN;};
#include "../portable/input/KeyboardMap.inc"
touhou::input::TouchController gestures;
std::array<u8,256> previous_scans{};

Key* key(const char* code){for(auto& k:keyboard_map)if(!std::strcmp(k.code,code))return &k;return nullptr;}
void host_key(const char* code,bool down){if(auto* k=key(code))k->hosted=down;}

touhou::input::TouchState touch_state(){
    touhou::input::TouchState s;auto& session=app->session;
    if(session.state.phase!=GameSessionPhase::stage||!session.battle||!session.battle->player)return s;
    if(session.state.replay){s.context=3;return s;}
    if(session.battle->dialogue&&session.battle->dialogue->active){s.context=2;return s;}
    const auto& player=*session.battle->player;const auto& p=player.motion.state;
    s.context=1;s.instance=int(session.state.stage);s.ready=player.state.life_state==1;
    s.x=p.position.x;s.y=p.position.y;s.fast=float(p.normal_speed)/128;s.slow=float(p.focus_speed)/128;
    s.min_x=-184;s.max_x=184;s.min_y=32;s.max_y=432;return s;
}
void clear_inputs(){for(auto& k:keyboard_map)k.hosted=false;previous_scans.fill(0);gestures.reset();if(app->session.battle)app->session.battle->player_input.movement.touch_mode=0;}

// Switch controller as TH11's gamepad (like upstream's SDL joystick path):
// the game's own key config maps the buttons, and its defaults (shot 0,
// bomb 1, focus 2, skip 3, pause 4) give the th10-switch layout on first launch.
//   0 B  1 A  2 L/ZL  3 R/ZR  4 +  5 X  6 Y
// The d-pad and sticks only move - they are never buttons, so Key Config
// cannot pick them. X and Y also keep their Enter / Retry shortcuts while no
// action is bound to them. − = snapshot.
struct PadInput {u8 buttons[128]{};u32 pressed=0;i32 x=0,y=0;bool minus=false;};
#ifdef __SWITCH__
PadState pad;
PadInput read_pad(){
    padUpdate(&pad);PadInput p;if(!padIsConnected(&pad))return p;
    const u64 held=padGetButtons(&pad);const auto stick=padGetStickPos(&pad,0);
    const u64 map[7]{HidNpadButton_B,HidNpadButton_A,HidNpadButton_L|HidNpadButton_ZL,HidNpadButton_R|HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_X,HidNpadButton_Y};
    for(u32 b=0;b<7;++b)if(held&map[b]){p.buttons[b]=128;p.pressed|=1u<<b;}
    // Axes in the game's units (±1000, cfg deadzone 600); the d-pad is a full push.
    p.x=i32(stick.x)*1000/32767;p.y=-i32(stick.y)*1000/32767;
    if(held&HidNpadButton_Left)p.x=-1000;if(held&HidNpadButton_Right)p.x=1000;if(held&HidNpadButton_Up)p.y=-1000;if(held&HidNpadButton_Down)p.y=1000;
    p.minus=held&HidNpadButton_Minus;return p;
}
#else
PadInput read_pad(){return {};}
#endif
bool snapshot_held=false,snapshot_requested=false;

void touch_event(const SDL_Event& e){
    // SDL2 reports touches normalised to the whole screen; the game wants the
    // 640x480 picture, which is pillarboxed inside it.
    const auto& p=app->graphics.backend.picture;if(p.width<=0||p.height<=0)return;
    const float x=(e.tfinger.x*p.drawable_width-p.x)/p.width,y=(e.tfinger.y*p.drawable_height-p.y)/p.height;
    const int type=e.type==SDL_FINGERDOWN?0:e.type==SDL_FINGERUP?2:1;
    if(type==0&&(x<0||x>1||y<0||y>1))return;
    gestures.pointer(type,int(e.tfinger.fingerId),std::clamp(x,0.f,1.f),std::clamp(y,0.f,1.f),SDL_GetTicks(),touch_state(),false);
}

bool sample_and_tick(){
    const PadInput pad=read_pad();const u8* config=app->session.config.bytes.data();
    const auto bound=[&](i16 button){for(u32 n=0;n<9;++n){i16 v;std::memcpy(&v,config+4+n*2,2);if(v==button)return true;}return false;};
    host_key("Enter",pad.buttons[5]&&!bound(5));host_key("KeyR",pad.buttons[6]&&!bound(6));
    if(pad.minus&&!snapshot_held)snapshot_requested=true;snapshot_held=pad.minus;
    bool keys[256]{};std::array<u8,256> scans{};const auto* physical=SDL_GetKeyboardState(nullptr);
    for(const auto& k:keyboard_map)if(k.hosted||(k.native!=SDL_SCANCODE_UNKNOWN&&physical[k.native])){if(k.scan<256)scans[k.scan]=128;if(k.vk<256)keys[k.vk]=true;if(k.vk>=160&&k.vk<=165)keys[16+(k.vk-160)/2]=true;if(k.scan==28||k.scan==156)keys[13]=true;}
    if(app->session.title){auto& title=*app->session.title;for(u32 i=0;i<256;++i)title.key_edges[i]=scans[i]&~previous_scans[i];title.number_keys=0;for(u32 i=0;i<9;++i)if(keys[49+i])title.number_keys|=1u<<i;title.controller_buttons=pad.pressed;}
    previous_scans=scans;
    const auto sample=gestures.sample(touch_state(),SDL_GetTicks(),keys[16],keys[37]||keys[38]||keys[39]||keys[40]);
    for(u32 n=0;n<256;++n)keys[n]=keys[n]||sample.keys[n];
    if(auto* b=app->session.battle.get()){b->player_input.movement.touch_mode=sample.motion;b->player_input.movement.touch_x=sample.x;b->player_input.movement.touch_y=sample.y;}
    const u32 raw=controller_keys(0,pad.buttons,128,pad.x,pad.y,config)|keyboard_keys(keys);
    const u32 held=(raw&~0x80100u)|((raw&0x80000)?256:0);
    return app->tick(held,(raw&256)!=0);
}

void fatal(const std::string& message){
    host::log("fatal: %s",message.c_str());
    // Tear down the game (GL context, window, audio) so the console can take
    // the framebuffer, then wait for +.
    app.reset();SDL_Quit();
#ifdef __SWITCH__
    consoleInit(nullptr);
    std::printf("\n  Touhou 11: Subterranean Animism - Switch port\n\n  Error: %s\n\n",message.c_str());
    std::printf("  Data folder: %s\n  Needed: th11.dat (v1.00a). Optional: thbgm.dat, msgothic.ttc, msmincho.ttc\n\n  Press + to exit.\n",paths().data.c_str());
    consoleUpdate(nullptr);
    while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}
    consoleExit(nullptr);
#else
    std::fprintf(stderr,"%s\n",message.c_str());
#endif
}
}
}

int main(int argc,char** argv){
    using namespace th11::sdl;using th11::u32;
    th11::host::locate_data(argc,argv);
#ifdef __SWITCH__
    // Same CPU boost the other native ports use; the GPU stays with the applet.
    if(R_SUCCEEDED(clkrstInitialize())){ClkrstSession cpu;if(R_SUCCEEDED(clkrstOpenSession(&cpu,PcvModuleId_CpuBus,3))){clkrstSetClockRate(&cpu,1785000000);clkrstCloseSession(&cpu);}clkrstExit();}
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
#endif
    th11::host::log("th11-switch: data %s",th11::host::paths().data.c_str());
    app=std::make_unique<Application>();
    if(!th11::host::exists(th11::host::paths().data_file("th11.dat"))){fatal("th11.dat not found");return 1;}
    if(!app->initialize()){fatal(app->error);return 1;}
    for(auto& k:keyboard_map)k.native=SDL_GetScancodeFromName(k.sdl);
    clear_inputs();app->reset_frame_window();
    // Vsync paces the loop at the panel's 60 Hz: one game tick per swap.
    // Measured time only adds catch-up ticks after a real stall (bounded to
    // 4 like upstream FrameCadence), so vsync jitter never drops a frame.
    constexpr double interval=touhou::sdl::FrameCadence::interval;
    double debt=0,previous=th11::host::seconds();bool running=true,ok=true;
    while(running&&ok
#ifdef __SWITCH__
          &&appletMainLoop()
#endif
    ){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            else if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP||e.type==SDL_FINGERMOTION)touch_event(e);
            else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)clear_inputs();
        }
        const double now=th11::host::seconds();debt+=std::clamp(now-previous,0.,.1);previous=now;
        u32 ticks=u32(std::floor((debt+interval*.5)/interval));ticks=std::min(4u,std::max(ticks,1u));
        debt=std::clamp(debt-ticks*interval,-interval*.5,.1);
        app->graphics.backend.defer=true;
        for(u32 n=0;n<ticks&&ok;++n)ok=sample_and_tick();
        if(ok&&snapshot_requested){snapshot_requested=false;app->snapshot();}
        app->graphics.backend.commit();app->graphics.backend.defer=false;
        app->audio.pump();
        if(ok&&app->session.state.phase==th11::GameSessionPhase::finished)running=false; // title menu Quit
    }
    if(!ok){fatal(app->error);return 1;}
    app->save_scores();
    app.reset();
    SDL_Quit();
    return 0;
}
