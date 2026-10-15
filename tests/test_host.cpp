// Host-side tests for the Switch port layer (Linux; no game data needed).
//   pointer handles  - the LP64 replacement for 32-bit ECL return words
//   ecl jumps        - backward (negative) ECL jumps on a 64-bit target
//   pad defaults     - Switch buttons through the game's default key config
//   thbgm stream     - PCM offsets, seeking and intro/loop points
//   font baker       - T11G tables accepted by the upstream GlyphAtlas and
//                      drawn by the upstream TextRaster (needs a CJK font)
//   renderer         - the SDL2/GLES3 port of the shared renderer draws,
//                      reads back and presents (needs a display, e.g. Xvfb)
#include "../game/Types.hpp"
#include "../game/TextRaster.hpp"
#include "../game/EclOwner.hpp"
#include "../game/GameConfig.hpp"
#include "../game/GameInput.hpp"
#include "../src/BgmStream.hpp"
#include "../src/FontBaker.hpp"
#include "../src/Platform.hpp"
#include "../portable/sdl/Renderer.hpp"
#include "../src/LoadingScreen.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
#include <string>
#include <vector>

using namespace th11;
static int failures=0;
#define CHECK(cond) do{if(!(cond)){std::printf("  FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);++failures;}}while(0)

static void test_pointer_handles(){
    std::puts("pointer handles");
    int a=0,b=0;
    CHECK(pointer_handle(nullptr)==0);CHECK(pointer_from_handle(0)==nullptr);
    const u32 ha=pointer_handle(&a),hb=pointer_handle(&b);
    CHECK(ha!=0&&hb!=0&&ha!=hb);
    CHECK(pointer_from_handle(ha)==&a);CHECK(pointer_from_handle(hb)==&b);
    CHECK(pointer_handle(&a)==ha); // stable
    CHECK(pointer_from_handle(0xffffff)==nullptr);
}

static void test_bgm_stream(const std::string& dir){
    std::puts("thbgm stream");
    const std::string path=dir+"/thbgm.dat";const u32 offset=20,frames=10000,intro=2500;
    {FILE* f=std::fopen(path.c_str(),"wb");std::fwrite("ZWAV",1,4,f);for(u32 i=4;i<offset;++i)std::fputc(0,f);
     for(u32 n=0;n<frames;++n){const i16 l=i16(n),r=i16(-i32(n));std::fwrite(&l,2,1,f);std::fwrite(&r,2,1,f);}std::fclose(f);}
    host::PcmStream s;s.file=std::fopen(path.c_str(),"rb");s.offset=offset;s.frames=frames;s.channels=2;s.rate=44100;
    auto config=ma_data_source_config_init();config.vtable=&host::pcm_vtable;
    CHECK(ma_data_source_init(&config,&s.base)==MA_SUCCESS);CHECK(host::pcm_seek(&s,0)==MA_SUCCESS);
    CHECK(ma_data_source_set_loop_point_in_pcm_frames(&s,intro,frames)==MA_SUCCESS);
    CHECK(ma_data_source_set_looping(&s,MA_TRUE)==MA_SUCCESS);
    std::vector<float> pcm(2*25000);ma_uint64 read=0;
    CHECK(ma_data_source_read_pcm_frames(&s,pcm.data(),25000,&read)==MA_SUCCESS);CHECK(read==25000);
    bool ordered=true;
    for(u32 i=0;i<25000;++i){const u32 expect=i<frames?i:intro+(i-frames)%(frames-intro);
        if(pcm[i*2]!=float(i16(expect))/32768.f||pcm[i*2+1]!=float(i16(-i32(expect)))/32768.f){ordered=false;std::printf("  frame %u: got %f expected %u\n",i,pcm[i*2]*32768.f,expect);break;}}
    CHECK(ordered);
    CHECK(host::pcm_seek(&s,frames+1)!=MA_SUCCESS);
    ma_data_source_uninit(&s.base);std::fclose(s.file);
}

static bool test_font_baker(const std::string& dir){
    std::puts("font baker");
    const char* candidates[]={"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc","/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc"};
    std::string font;for(const char* c:candidates)if(host::exists(c)){font=c;break;}
    if(font.empty()){std::puts("  skipped: no CJK font installed");return false;}
    std::vector<u8> bytes;CHECK(host::read_file(font,bytes,64u*1024*1024));CHECK(host::write_file(dir+"/msgothic.ttc",bytes));
    host::paths().data=host::paths().save=dir;
    host::BakedFonts baked;std::string error;const double begin=host::seconds();
    const bool ok=host::bake_fonts(baked,error);CHECK(ok);if(!ok){std::printf("  %s\n",error.c_str());return true;}
    std::printf("  baked 4 fonts in %.2fs (%zu/%zu/%zu/%zu bytes)\n",host::seconds()-begin,baked.files[0].size(),baked.files[1].size(),baked.files[2].size(),baked.files[3].size());
    TextRaster raster;
    for(u32 n=0;n<6;++n)CHECK(raster.glyphs.load(n,baked.files[n].data(),u32(baked.files[n].size())));
    CHECK(raster.glyphs.ready());
    // "東方地霊殿" in Shift-JIS, drawn with the upstream text path.
    const std::string text="\x93\x8c\x95\xfb\x92\x6e\x97\xec\x93\x61";
    for(i32 font_index=0;font_index<4;++font_index){
        TextStyle style;style.height=17;style.font=font_index;style.color=0xffffff;style.outline=0x000000;
        CHECK(raster.rasterize(text,style));
        u32 lit=0;const auto* px=reinterpret_cast<const u16*>(raster.scratch.pixels.data());
        for(u32 n=0;n<raster.scratch.pixels.size()/2;++n)if((px[n]&0xf000)&&(px[n]&0x0fff))++lit;
        std::printf("  font %d: %u lit texels\n",font_index,lit);CHECK(lit>200);
        if(const char* dump=std::getenv("TH11_DUMP_TEXT")){ // ARGB4444 scratch -> 24-bit BMP for eyeballing
            const u32 w=raster.scratch.width,h=raster.scratch.height,row=w*3,size=54+row*h;std::vector<u8> bmp(size,0);
            auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)bmp[at+i]=u8(v>>(8*i));};
            bmp[0]='B';bmp[1]='M';put(2,size,4);put(10,54,4);put(14,40,4);put(18,w,4);put(22,h,4);put(26,1,2);put(28,24,2);
            for(u32 y=0;y<h;++y)for(u32 x=0;x<w;++x){const u16 v=px[(h-1-y)*w+x];const u32 a=v>>12;auto c=[&](u32 n){return u8(((n&15)*17*a+96*(15-a))/15);};
                u8* d=bmp.data()+54+y*row+x*3;d[0]=c(v);d[1]=c(v>>4);d[2]=c(v>>8);}
            host::write_file(std::string(dump)+std::to_string(font_index)+".bmp",bmp);
        }
    }
    return true;
}

namespace {
struct Surfaces {std::map<u32,std::vector<u8>> pixels;std::map<u32,touhou::sdl::Surface> info;};
touhou::sdl::Surface resolve(void* owner,u32 id){auto& s=*static_cast<Surfaces*>(owner);auto it=s.info.find(id);if(it==s.info.end())return {};auto r=it->second;auto& p=s.pixels[id];r.data=p.data();r.size=u32(p.size());return r;}
void add(Surfaces& s,u32 id,u32 w,u32 h,touhou::graphics::PixelFormat f,u32 bpp){s.pixels[id].assign(size_t(w)*h*bpp,0);s.info[id]={id,w,h,f,w*bpp,nullptr,0,0};}
}
static bool test_renderer(){
    std::puts("renderer (SDL2 + GLES3)");
    if(!std::getenv("DISPLAY")&&!std::getenv("WAYLAND_DISPLAY")){std::puts("  skipped: no display");return false;}
    using namespace touhou::graphics;
    Surfaces surfaces;add(surfaces,1,640,480,PixelFormat::Bgra8,4);add(surfaces,2,640,480,PixelFormat::Depth16,2);add(surfaces,3,2,2,PixelFormat::Bgra8,4);
    for(u32 n=0;n<4;++n){auto* p=surfaces.pixels[3].data()+n*4;p[0]=0;p[1]=255;p[2]=0;p[3]=255;} // green texture
    touhou::sdl::Renderer r(10,resolve,&surfaces);r.title="th11-switch test";
    const bool ok=r.initialize();CHECK(ok);if(!ok){std::printf("  %s\n",r.error());return true;}
    r.state.target=1;r.state.depth=2;r.viewport({0,0,640,480,0,1});
    r.clear(3,0xff0000ff,1,0); // blue
    struct V {float x,y,z,w;u32 diffuse;float u,v;};
    const V quad[4]{{100,100,0,1,0xffffffff,0,0},{200,100,0,1,0xffffffff,1,0},{100,200,0,1,0xffffffff,0,1},{200,200,0,1,0xffffffff,1,1}};
    auto& p=r.pipeline();p=PipelineState{};p.depthTest=false;p.color.operation=p.alpha.operation=ColorOperation::First;p.color.first=p.alpha.first={ArgumentSource::Texture};
    r.state.texture=3;r.state.layout=attributes(VertexLayout::ScreenColorUv);
    r.draw(Topology::Strip,2,quad,sizeof(V));
    r.read(1);
    auto pixel=[&](u32 x,u32 y){const auto* q=surfaces.pixels[1].data()+(y*640+x)*4;return u32(q[2])<<16|u32(q[1])<<8|q[0];};
    std::printf("  centre %06x corner %06x\n",pixel(150,150),pixel(10,10));
    CHECK(pixel(150,150)==0x00ff00);CHECK(pixel(10,10)==0x0000ff);CHECK(pixel(99,150)==0x0000ff);CHECK(pixel(150,99)==0x0000ff);
    ++surfaces.info[1].version;r.present(1);
    std::printf("  picture %dx%d at %d,%d in %dx%d\n",r.picture.width,r.picture.height,r.picture.x,r.picture.y,r.picture.drawable_width,r.picture.drawable_height);
    CHECK(r.picture.width*3==r.picture.height*4);CHECK(r.picture.x*2+r.picture.width==r.picture.drawable_width);
    CHECK(glGetError()==GL_NO_ERROR);
    return true;
}

// ECL jump offsets are signed. Read as u32 they only wrap correctly on 32-bit
// targets; on AArch64 a backward jump landed 4 GiB away (stage crash).
static void test_ecl_backward_jump(){
    std::puts("ecl jumps");
    struct Services final:EclServices {
        i32 integer(i32)override{return 0;}double floating(i32)override{return 0;}
        i32* integer_reference(i32)override{return nullptr;}float* float_reference(i32)override{return nullptr;}
        i32 command(EclContext&)override{return 0;}void* allocate(u32 n)override{return std::malloc(n);}void release(void* p)override{std::free(p);}
    } services;
    // [0] time 1: delete   [16] time 0: jump -16, time := 1
    alignas(4) u8 code[40]{};
    const EclInstruction del{1,1,16,0,0xff,0,0},jump{0,12,24,0,0xff,2,0};
    std::memcpy(code,&del,16);std::memcpy(code+16,&jump,16);const i32 args[2]{-16,1};std::memcpy(code+32,args,8);
    auto context=std::make_unique<EclContext>();context->instruction=reinterpret_cast<const EclInstruction*>(code+16);
    CHECK(context->update(1,services)==-1);CHECK(context->instruction==nullptr);CHECK(context->time==1);
}

// The port feeds 0 B, 1 A, 2 L/ZL, 3 R/ZR, 4 +, 5 X, 6 Y as gamepad buttons;
// the default config must give the previous fixed layout.
static void test_pad_defaults(){
    std::puts("pad defaults");
    GameConfig config;config.reset();const u8* c=config.bytes.data();
    auto keys=[&](u32 button,i32 x=0,i32 y=0){u8 b[128]{};if(button<128)b[button]=128;return controller_keys(0,b,128,x,y,c);};
    CHECK(keys(0)==1);CHECK(keys(1)==2);CHECK(keys(2)==8);CHECK(keys(3)==512);CHECK(keys(4)==256);
    CHECK(keys(5)==0&&keys(6)==0);
    CHECK(keys(200,-1000,0)==64);CHECK(keys(200,1000,0)==128);CHECK(keys(200,0,-1000)==16);CHECK(keys(200,0,1000)==32);
    CHECK(keys(200,500,-500)==0); // inside the 600 deadzone
}

// One ANM v7 chunk: a w x h texture in `color`, optional sprites and scripts.
static void anm_chunk(std::vector<u8>& anm,u16 w,u16 h,u32 color,const std::vector<std::array<float,4>>& sprites,const std::vector<std::vector<u8>>& scripts,bool last){
    const u32 ns=u32(sprites.size()),nc=u32(scripts.size()),table_end=64+ns*4+nc*8,name=table_end,first_sprite=name+8;u32 at=first_sprite+ns*20;std::vector<u32> script_at;
    for(const auto& code:scripts){script_at.push_back(at);at+=u32(code.size());}
    const u32 thtx=at,bytes=u32(w)*h*4,size=thtx+16+bytes,base=u32(anm.size());anm.resize(base+size,0);u8* c=anm.data()+base;
    const auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)c[at+i]=u8(v>>(8*i));};
    put(0,7,4);put(4,ns,2);put(6,nc,2);put(10,w,2);put(12,h,2);put(14,1,2);put(16,name,4);put(28,thtx,4);c[32]=1;put(36,last?0:size,4);std::memcpy(c+name,"tex",3);
    for(u32 i=0;i<ns;++i){const u32 o=first_sprite+i*20;put(64+i*4,o,4);put(o,i,4);std::memcpy(c+o+4,sprites[i].data(),16);}
    for(u32 i=0;i<nc;++i){put(64+ns*4+i*8,i,4);put(64+ns*4+i*8+4,script_at[i],4);std::memcpy(c+script_at[i],scripts[i].data(),scripts[i].size());}
    std::memcpy(c+thtx,"THTX",4);put(thtx+6,1,2);put(thtx+8,w,2);put(thtx+10,h,2);put(thtx+12,bytes,4);
    for(u32 i=0;i<u32(w)*h;++i)put(thtx+16+i*4,color,4);
}
// One ANM instruction: opcode, time, raw 32-bit arguments.
static void anm_op(std::vector<u8>& code,i16 opcode,i16 time,std::initializer_list<u32> args={}){
    const u32 at=u32(code.size()),length=8+u32(args.size())*4;code.resize(at+length,0);
    const auto put=[&](u32 o,u32 v,u32 n){for(u32 i=0;i<n;++i)code[at+o+i]=u8(v>>(8*i));};put(0,u16(opcode),2);put(2,length,2);put(4,u16(time),2);u32 o=8;for(u32 a:args){put(o,a,4);o+=4;}
}
static u32 bits(float f){u32 v;std::memcpy(&v,&f,4);return v;}

static void test_loading_screen(){
    std::puts("loading screen (sig.anm script 0, ascii.anm script 6)");
    sdl::GraphicsDevice graphics;if(!graphics.initialize()){std::printf("  skipped: %s\n",graphics.error.c_str());return;}
    AnmRenderer renderer(graphics);
    // Stand-ins: sig.anm script 0 shows a red 64x64 sprite at (320,240);
    // ascii.anm script 6 makes script 5 (a white 32x32 sprite) as its child,
    // as the real one makes its parts. Op 3 = sprite, 48 = position, 63 = wait, 88 = child.
    std::vector<std::vector<u8>> sig_scripts(1);anm_op(sig_scripts[0],3,0,{0});anm_op(sig_scripts[0],48,0,{bits(320),bits(240),bits(0)});anm_op(sig_scripts[0],63,0);anm_op(sig_scripts[0],-1,0);
    std::vector<u8> sig_bytes;anm_chunk(sig_bytes,64,64,0xffff0000,{{0,0,64,64}},sig_scripts,true);
    std::vector<std::vector<u8>> scripts(7);for(u32 i=0;i<5;++i)anm_op(scripts[i],-1,0);
    anm_op(scripts[5],3,0,{0});anm_op(scripts[5],63,0);anm_op(scripts[5],-1,0);
    anm_op(scripts[6],88,0,{5});anm_op(scripts[6],63,0);anm_op(scripts[6],-1,0);
    std::vector<u8> ascii_bytes;anm_chunk(ascii_bytes,32,32,0xffffffff,{{0,0,32,32}},scripts,true);
    AnmResource sig,ascii;CHECK(sig.open(sig_bytes.data(),u32(sig_bytes.size())));CHECK(ascii.open(ascii_bytes.data(),u32(ascii_bytes.size())));
    {sdl::LoadingScreen screen(graphics,renderer);CHECK(screen.start(std::move(sig),std::move(ascii)));CHECK(screen.running());
     for(u32 i=0;i<4;++i){SDL_Delay(17);screen.pump();}
     renderer.flush();graphics.backend.read(sdl::GraphicsDevice::screen);auto* image=graphics.pixels(sdl::GraphicsDevice::screen);CHECK(image!=nullptr);
     if(image){auto pixel=[&](u32 x,u32 y){const auto* q=image->pixels.data()+y*image->pitch+x*4;return u32(q[2])<<16|u32(q[1])<<8|q[0];};
        u32 l=640,r=0,t=480,b=0;for(u32 y=0;y<480;++y)for(u32 x=0;x<640;++x)if(pixel(x,y)==0xffffff){l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);}
        std::printf("  sig sprite %06x at 320,240; child sprite at x %u-%u, y %u-%u; corner %06x\n",pixel(320,240),l,r,t,b,pixel(5,5));
        CHECK(pixel(320,240)==0xff0000);CHECK(pixel(300,220)==0xff0000);CHECK(pixel(5,5)==0x000000);
        CHECK(l>=462&&l<=466&&r>=493&&r<=497&&t>=374&&t<=378&&b>=405&&b<=409);}   // 32x32 around (480,392)
     screen.stop();CHECK(!screen.running());}
    // Too few scripts: start() refuses, so start-up keeps the bar.
    AnmResource few,few_sig;CHECK(few.open(sig_bytes.data(),u32(sig_bytes.size())));CHECK(few_sig.open(sig_bytes.data(),u32(sig_bytes.size())));
    {sdl::LoadingScreen screen(graphics,renderer);CHECK(!screen.start(std::move(few_sig),std::move(few)));CHECK(!screen.running());}
}

int main(){
    const std::string dir=std::string(std::getenv("TMPDIR")?std::getenv("TMPDIR"):"/tmp")+"/th11-switch-test";
    host::make_directory(dir);
    test_pointer_handles();test_ecl_backward_jump();test_pad_defaults();test_bgm_stream(dir);test_font_baker(dir);
    if(SDL_Init(SDL_INIT_VIDEO)==0){test_renderer();SDL_Quit();}else std::printf("renderer skipped: %s\n",SDL_GetError());
    if(SDL_Init(SDL_INIT_VIDEO)==0){test_loading_screen();SDL_Quit();}
    std::printf(failures?"FAILED (%d)\n":"all host tests passed\n",failures);
    return failures?1:0;
}
