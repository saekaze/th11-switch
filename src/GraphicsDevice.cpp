#include "GraphicsDevice.hpp"
namespace th11::sdl {
touhou::sdl::Surface GraphicsDevice::resolve(void* owner,u32 id){
    auto& device=*static_cast<GraphicsDevice*>(owner);auto it=device.textures.find(id);if(it==device.textures.end())return {};
    auto& t=it->second;auto& image=t.image;return {id,image.width,image.height,image.format,image.pitch,image.pixels.data(),u32(image.pixels.size()),t.revision};
}
bool GraphicsDevice::initialize(){
    using namespace touhou::graphics;
    for(u32 id:{screen,depth}){auto& image=textures[id].image;image.width=640;image.height=480;image.pitch=640*(id==screen?4:2);image.format=id==screen?PixelFormat::Bgra8:PixelFormat::Depth16;if(id==screen)image.pixels.resize(image.pitch*image.height);}
    backend.title="Touhou 11: Subterranean Animism";
    if(!backend.initialize()){error=backend.error();return false;}backend.state.target=screen;backend.state.depth=depth;initialize_game_pipeline();return true;
}
bool GraphicsDevice::preload(AnmResource& file,bool low_color){
    if(resources.find(&file)!=resources.end())return true;
    std::vector<u32> handles;handles.reserve(file.textures.size());
    for(const auto& source:file.textures){Texture texture;
        if(!texture.image.load(source,low_color)){error="Unable to prepare ANM texture: "+source.name;for(const auto id:handles){backend.release(id);textures.erase(id);}return false;}
        const auto id=next_handle++;textures.emplace(id,std::move(texture));handles.push_back(id);backend.prepare(id);
    }
    resources.emplace(&file,std::move(handles));return true;
}
void GraphicsDevice::unload(const AnmResource& file){
    auto it=resources.find(&file);if(it==resources.end())return;backend.flush();for(u32 id:it->second){backend.release(id);textures.erase(id);}resources.erase(it);
}
u32 GraphicsDevice::texture(const AnmResource& file,u32 index){
    const auto it=resources.find(&file);if(it==resources.end()||index>=it->second.size()){error="ANM texture was not preloaded";return 0;}return it->second[index];
}
bool GraphicsDevice::select_target(const AnmResource* file,u32 index){
    const u32 handle=file?texture(*file,index):screen;if(!handle)return false;
    backend.flush();backend.state.target=handle;
    // The original target switch resets the device viewport to the surface.
    // CPU projection still uses the active scene camera until its next update.
    const auto& image=textures.at(handle).image;
    backend.viewport({0,0,image.width,image.height,0,1});return true;
}
bool GraphicsDevice::clear_target(u32 color,const GraphicsViewport* rect){
    if(rect){const i32 box[]={i32(rect->x),i32(rect->y),i32(rect->x+rect->width),i32(rect->y+rect->height)};backend.clear(3,color,1,0,box,1);}
    else backend.clear(3,color,1,0);
    return true;
}
}
