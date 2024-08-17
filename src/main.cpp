#include <algorithm>
#include <concurrentqueue.h>
#include <filesystem>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <span>
#include <sstream>
static_assert(sizeof(float) == 4); // #include <stdfloat>
#include <string>
#include <string_view>
#include <vector>
#if _WIN32
#define NOMINMAX 1
#include <windows.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <Firework.Core.hpp>
#include <Components/Panel.h>
#include <TacradCLI.h>

namespace fs = std::filesystem;

using namespace Firework::Internal;

inline Entity2D* textEntry;
inline TacradCLI* textEntryInput;

int main()
{
    Application::setTargetFrameRate(30);
    
    EngineEvent::OnInitialize += []
    {
        textEntry = new Entity2D();
        textEntry->name = L"Text Entry Field";

        textEntryInput = textEntry->addComponent<TacradCLI>();
        textEntryInput->fontSize = 16;
        
        Debug::printHierarchy();
    };
    
    return 0;
}