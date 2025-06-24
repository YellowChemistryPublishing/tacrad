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

#define CONSOLE_PADDING_SIDE 20.0f
#define CONSOLE_PADDING_TOP 2.0f

#define CONSOLE_DROP_SHADOW_HEIGHT 5.0f

namespace fs = std::filesystem;

using namespace Firework;
using namespace Firework::Internal;
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

    struct Command
    {
        void (TacradCLI::* execute)(const std::vector<std::u32string>& cmd);
        std::u32string_view name;
        std::u32string_view description;
        bool aliasOrHidden = false;
        std::vector<std::u32string_view> aliasOf;
    };
    static std::vector<std::pair<uint64_t, Command>> commands;

    std::vector<std::u32string> cmdHistory;
    size_t nextHistoryIndex = SIZE_MAX;

    float nextTimePoint = 0.0f;
    bool showCursorThisFrame = true;

    bool seekIfOutOfFrame = false;

    // In **characters**.
    uint32_t width();

    void postEntry();
    void pushLine();

    void onCommand(std::u32string_view command);
    void commandHelp(const std::vector<std::u32string>& cmd);
    void commandClear(const std::vector<std::u32string>& cmd);
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
    
    void writeLine(std::u32string_view line);

    friend struct ::TacradCLISystem;
};
