#include "GameResources.hpp"
#include <cstdio>

namespace th11 {

bool GameResources::fail(const char* message) {
    last_error = message ? message : "resource error";
    return false;
}

bool GameResources::open_archive(const u8* bytes, u32 size) {
    lookup.clear(); animations = {}; last_error.clear();
    if (!archive.open(bytes, size)) return fail("invalid THA1 archive");
    for (u32 i=0; i<archive.entries.size(); ++i) {
        const auto& entry = archive.entries[i];
        if (entry.name.empty() || lookup.find(entry.name) != lookup.end()) {
            archive.entries.clear(); lookup.clear();
            return fail("duplicate or empty archive resource");
        }
        lookup.emplace(entry.name, i);
    }
    return true;
}

bool GameResources::has(const std::string& name) const noexcept {
    return lookup.find(name) != lookup.end();
}

bool GameResources::read(const std::string& name, std::vector<u8>& data) {
    const auto found = lookup.find(name);
    if (found == lookup.end()) return fail("missing archive resource");
    if (!archive.read(found->second, data)) return fail("resource decode failed");
    return true;
}

bool GameResources::open_anm(const std::string& name, AnmResource& output) {
    std::vector<u8> data;
    if (!read(name, data) || !output.open(data.data(), u32(data.size())))
        return fail("invalid ANM resource");
    return true;
}

bool GameResources::open_sht(const std::string& name, ShtResource& output) {
    std::vector<u8> data;
    if (!read(name, data) || !output.open(data.data(), u32(data.size())))
        return fail("invalid SHT resource");
    return true;
}

bool GameResources::load_ecl(const std::string& name, EclProgram& output) {
    output.files.clear(); output.definitions.clear(); output.error.clear();
    if (!output.load(name, *this)) {
        last_error = output.error.empty() ? "invalid ECL resource" : output.error;
        return false;
    }
    return true;
}

bool GameResources::load_stage(u32 stage, StageResources& output) {
    if (stage < 1 || stage > 7) return fail("stage outside TH11 range");
    char name[64];
    std::vector<u8> scene;
    std::snprintf(name, sizeof name, "stage%02u.std", stage);
    if (!read(name, scene) || !output.scene.open(scene.data(), u32(scene.size()))) return fail("invalid STD resource");
    std::snprintf(name, sizeof name, "stage%02u.anm", stage);
    if (!open_anm(name, output.background)) return false;
    std::snprintf(name, sizeof name, "st%02ulogo.anm", stage);
    if (!open_anm(name, output.logo)) return false;
    std::snprintf(name, sizeof name, "stgenm%02u.anm", stage);
    if (!open_anm(name, output.enemies)) return false;
    std::snprintf(name, sizeof name, "stage%02u.ecl", stage);
    if (!load_ecl(name, output.timeline)) return false;
    for (u32 i=0; i<output.messages.size(); ++i) {
        std::snprintf(name, sizeof name, "st%02u_%02u%c.msg", stage, i/3, char('a'+i%3));
        if (!read(name, output.messages[i])) return false;
    }
    output.boss_programs.clear();
    for (const char* suffix : {"boss", "mboss"}) {
        std::snprintf(name, sizeof name, "stage%02u%s.ecl", stage, suffix);
        if (!has(name)) continue;
        output.boss_programs.emplace_back();
        if (!load_ecl(name, output.boss_programs.back())) return false;
    }
    return true;
}

bool GameResources::animation(u32 slot, const std::string& name) {
    if (slot >= animations.size() || !has(name)) return fail("missing ECL animation");
    animations[slot] = name;
    return true;
}

const std::string& GameResources::animation_name(u32 slot) const noexcept {
    static const std::string empty;
    return slot < animations.size() ? animations[slot] : empty;
}

}
