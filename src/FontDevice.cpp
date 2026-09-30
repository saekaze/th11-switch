// Switch port of th11_web/cpp/sdl/FontDevice.cpp. text() is upstream's; the
// glyph tables come from, in order:
//   1. <data>/fonts/ - the GDI-baked tables from an upstream web build
//      (th11_web/assets/sdl-native/fonts), pixel-exact with Windows;
//   2. <save>/fontcache/ - a previous FreeType bake (see FontBaker.hpp);
//   3. a fresh FreeType bake, written to fontcache/ for the next launch.
#include "FontDevice.hpp"
#include "FontBaker.hpp"
#include "Platform.hpp"
namespace th11::sdl {
namespace {
const char* const files[]={"font0.bin","font1.bin","font2.bin","font3.bin","cp932.bin","blend4444.bin"};
// Bumped whenever the baker output changes; also keyed on the source fonts.
std::string bake_stamp(){
    std::string stamp="th11-switch-fontbake-2";
    for(const char* name:{"msgothic.ttc","msmincho.ttc"}){const auto path=host::paths().data_file(name);
        FILE* f=std::fopen(path.c_str(),"rb");long size=-1;if(f){std::fseek(f,0,SEEK_END);size=std::ftell(f);std::fclose(f);}
        stamp+=std::string(" ")+name+"="+std::to_string(size);}
    return stamp;
}
}
bool FontDevice::initialize(){
    if(ready)return true;
    auto load_dir=[&](const std::string& dir){
        std::vector<u8> data[6];for(u32 n=0;n<6;++n)if(!host::read_file(dir+"/"+files[n],data[n],16*1024*1024))return false;
        TextRaster fresh;for(u32 n=0;n<6;++n)if(!fresh.glyphs.load(n,data[n].data(),u32(data[n].size())))return false;
        for(u32 n=0;n<6;++n)raster.glyphs.load(n,data[n].data(),u32(data[n].size()));return raster.glyphs.ready();
    };
    const auto original=host::paths().data_file("fonts"),cache=host::paths().save_file("fontcache");
    if(load_dir(original)){host::log("fonts: original GDI tables from %s",original.c_str());ready=true;return true;}
    const auto stamp=bake_stamp();std::vector<u8> saved;
    if(host::read_file(cache+"/stamp.txt",saved,4096)&&std::string(saved.begin(),saved.end())==stamp&&load_dir(cache)){host::log("fonts: cached bake");ready=true;return true;}
    host::log("fonts: baking glyph tables (first launch only)...");const double begin=host::seconds();
    host::BakedFonts baked;if(!host::bake_fonts(baked,error,progress))return false;
    for(u32 n=0;n<6;++n)if(!raster.glyphs.load(n,baked.files[n].data(),u32(baked.files[n].size()))){error=std::string("Baked font table rejected: ")+files[n];return false;}
    host::log("fonts: baked in %.1fs from %s",host::seconds()-begin,baked.source.c_str());
    if(host::make_directory(cache)){bool ok=true;for(u32 n=0;n<6;++n)ok=ok&&host::write_file(cache+"/"+files[n],baked.files[n]);
        if(ok)host::write_file(cache+"/stamp.txt",reinterpret_cast<const u8*>(stamp.data()),stamp.size());}
    ready=raster.glyphs.ready();return ready;
}
bool FontDevice::text(AnmVm& vm,const DialogueText& request){
    if(!ready||!vm.sprite||!vm.resource){error="Missing text animation";return false;}
    const u32 handle=graphics.texture(*vm.resource,vm.sprite->texture);auto* image=graphics.pixels(handle);if(!image){error="Missing text texture";return false;}
    const auto& s=*vm.sprite;TextStyle style;style.offset=request.offset;style.height=vm.rectangle_columns?vm.rectangle_columns:17;style.font=request.style;
    style.color=request.color;style.spacing=u32(request.font);style.plain=(vm.flags2&2)!=0;
    if(request.right_aligned)style.offset=i32(double(s.width)-u32((style.height-1)*request.bytes.size()/2));
    if(request.centered)style.offset=i32(s.width)/2-i32((style.height-1)*request.bytes.size()/4);
    const ImageRect rect{i32(s.x),i32(s.y),i32(s.x+s.width),i32(s.y+s.height)};
    if(!raster.write(*image,rect,request.bytes,style)){error="Original text raster bounds: "+std::to_string(style.height);return false;}
    graphics.changed(handle);vm.flags|=1;++writes;return true;
}
}
