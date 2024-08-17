#pragma once

#define NOMINMAX 1

#include <format>
#include <miniaudio.h>
#include <random>
#include <sstream>

#include <Components/Panel.h>
#include <Components/Text.h>
#include <Core/Application.h>
#include <Core/CoreEngine.h>
#include <Core/Display.h>
#include <Core/Time.h>
#include <EntityComponentSystem/ComponentData.h>
#include <EntityComponentSystem/EngineEvent.h>
#include <EntityComponentSystem/EntityManagement.h>
#include <GL/Renderer.h>
#include <Objects/Entity2D.h>

namespace fs = std::filesystem;

using namespace Firework;
using namespace Firework::Internal;
using namespace Firework::Mathematics;
using namespace Firework::GL;
using namespace Firework::PackageSystem;

struct TacradCLISystem;

constexpr char32_t cursorCharacter = U'\x2582';

class TacradCLI : public ComponentData2D
{
    inline static TrueTypeFontPackageFile* font;
    inline static std::u32string_view lineInit = U"\n[tacrad] ";
    inline static std::u32string_view conInit = 
UR"(tacrad music player [Version 1.0.0]
(c) 2024 Yellow Chemistry Publishing. All rights reserved.

Welcome to the tacrad CLI! (Enter "help" for details.)
)";

    Text* _text;
    std::u32string str { TacradCLI::conInit.begin(), TacradCLI::conInit.end() };

    RectFloat updateRect;

    std::vector<std::u32string> cmdHistory;
    size_t nextHistoryIndex = SIZE_MAX;

    float nextTimePoint = 0.0f;
    bool showCursorThisFrame = true;

    bool seekIfOutOfFrame = false;
    float scrollTop = 0.0f;

    // In **characters**.
    uint32_t width();

    void postEntry();
    void pushLine();
    void writeLine(std::u32string_view line);

    // CLI ^ / Music Player v

    inline static std::random_device seeder;
    inline static std::mt19937 randEngine { seeder() };
    inline static std::uniform_real_distribution<float> dist { 0.0f, 1.0f };

    struct Command
    {
        void (TacradCLI::* execute)(const std::vector<std::u32string>& cmd);
        std::u32string_view name;
        std::u32string_view description;
        bool aliasOrHidden = false;
        std::vector<std::u32string_view> aliasOf;
    };
    static std::vector<std::pair<uint64_t, Command>> commands;

    inline static ma_engine engine;
    ma_sound music;
    
    bool playing = false;
    bool paused = false;
    bool loop = false;
    enum class PlaylistType
    {
        Sequential,
        Shuffle,
        Queued
    } type = PlaylistType::Sequential;
    
    ma_uint64 prevFrame = 0;
    ma_uint64 frameLen;
    float musicLen;
    std::u32string musicName;

    std::list<std::pair<std::u32string, fs::path>> queue;
    decltype(queue)::iterator queuePos = queue.end();
    
    inline static std::u32string toLower(std::u32string_view str)
    {
        std::u32string ret(str.begin(), str.end());
        std::transform(ret.begin(), ret.end(), ret.begin(), [](char32_t c){ return (char32_t)std::tolower((int)c); });
        return ret;
    }
    static std::u32string musicLookup(std::u32string_view name, fs::path& file);

    void startMusic(std::u32string_view query);
    void tryPlayNextAlphabetical(std::u32string_view prev, bool wasPaused);
    void stopMusic();
    void tryPlayNextShuffle(bool wasPaused = false);
    void incrQueuePos();
    void tryPlayNextQueued(bool wasPaused = false);

    void musicResume();
    void musicPause();
    
    void onCommand(std::u32string_view command);
    void commandHelp(const std::vector<std::u32string>& cmd);
    void commandPlayOrTogglePlaying(const std::vector<std::u32string>& cmd);
    void commandResumeOrPlay(const std::vector<std::u32string>& cmd);
    void commandPlay(const std::vector<std::u32string>& cmd);
    void commandResume(const std::vector<std::u32string>& cmd);
    void commandPause(const std::vector<std::u32string>& cmd);
    void commandSeek(const std::vector<std::u32string>& cmd);
    void commandVolume(const std::vector<std::u32string>& cmd);
    void commandStop(const std::vector<std::u32string>& cmd);
    void commandNext(const std::vector<std::u32string>& cmd);
    void commandPlaylist(const std::vector<std::u32string>& cmd);
    void commandExit(const std::vector<std::u32string>& cmd);
public:
    inline TacradCLI() = default;
    void onCreate();

    const Property<float, float> fontSize
    {
        [this]() -> float { return this->_text->fontSize; },
        [this](float value) { this->_text->fontSize = value; }
    };

    friend struct ::TacradCLISystem;
};
