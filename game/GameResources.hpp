#pragma once
#include "Archive.hpp"
#include "AnmResource.hpp"
#include "EclResource.hpp"
#include "ShtResource.hpp"
#include "StageResource.hpp"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace th11 {

struct StageResources {
    StageResource scene;
    AnmResource background;
    AnmResource logo;
    AnmResource enemies;
    EclProgram timeline;
    std::array<std::vector<u8>,6> messages{};
    std::vector<EclProgram> boss_programs;
};

// Owns the decoded TH11 archive index and provides the same named-resource
// boundary to ANM, ECL and SHT loaders. Gameplay managers receive these
// decoded objects later; they do not know about archive offsets or ciphers.
class GameResources final : public EclResourceProvider {
public:
    bool open_archive(const u8* bytes, u32 size);
    bool read(const std::string& name, std::vector<u8>& data) override;
    bool has(const std::string& name) const noexcept;
    bool open_anm(const std::string& name, AnmResource& output);
    bool open_sht(const std::string& name, ShtResource& output);
    bool load_ecl(const std::string& name, EclProgram& output);
    bool load_stage(u32 stage, StageResources& output);
    bool animation(u32 slot, const std::string& name) override;
    const std::string& error() const noexcept { return last_error; }
    u32 resource_count() const noexcept { return u32(archive.entries.size()); }
    const std::string& animation_name(u32 slot) const noexcept;

private:
    Archive archive;
    std::unordered_map<std::string,u32> lookup;
    std::array<std::string,64> animations{};
    std::string last_error;
    bool fail(const char* message);
};

}
