#include "TacradCLI.h"

#include <span>

#include <Components/Mask.h>

#include <DropShadow.h>
#include <MusicPlayer.h>

using namespace Firework;

void TacradCLI::onCreate()
{
    this->entity()->addComponent<Mask>();
    
    Entity2D* consoleTopShadow = new Entity2D();
    consoleTopShadow->parent = this->entity();
    consoleTopShadow->name = L"Console Drop Shadow";
    consoleTopShadow->rectTransform()->localPosition = sysm::vector2(0.0f, 0.0f);
    consoleTopShadow->rectTransform()->rect = RectFloat
    (
        0, this->rectTransform()->rect().right,
        -CONSOLE_DROP_SHADOW_HEIGHT, this->rectTransform()->rect().left
    );
    consoleTopShadow->rectTransform()->rectAnchor = RectFloat(0.0f, 1.0f, 0.0f, 1.0f);
    DropShadow* shadow = consoleTopShadow->addComponent<DropShadow>();

    Entity2D* textEntity = new Entity2D();
    textEntity->parent = this->entity();
    textEntity->name = L"Text Object";
    
    this->_text = textEntity->addComponent<Text>();
    
    this->_text->fontFile = TacradCLI::font;
    this->_text->horizontalAlign = TextAlign::Minor;
    this->_text->verticalAlign = TextAlign::Minor;

    this->pushLine();

    this->_text->rectTransform()->localPosition = sysm::vector2(0.0f, -CONSOLE_PADDING_TOP);
    this->_text->rectTransform()->rect = RectFloat
    (
        0, this->rectTransform()->rect().right - CONSOLE_PADDING_SIDE,
        -this->_text->calculateBestFitHeight(), this->rectTransform()->rect().left + CONSOLE_PADDING_SIDE
    );
    this->_text->rectTransform()->rectAnchor = RectFloat(0.0f, 1.0f, 0.0f, 1.0f);
}

uint32_t TacradCLI::width()
{
    TrueTypeFontPackageFile* fontFile = this->_text->fontFile();
    if (fontFile)
    {
        Typography::Font& font = fontFile->fontHandle();
        Typography::GlyphMetrics metrics = font.getGlyphMetrics(font.getGlyphIndex(U' ')); // Monospace font or we're screwed.
        return uint32_t(this->rectTransform()->rect().width() / ((float)metrics.advanceWidth * (float)this->_text->fontSize / float(font.ascent - font.descent)));
    }
    else return uint32_t(this->rectTransform()->rect().width());
}

void TacradCLI::postEntry()
{
    this->nextTimePoint = 0.0f;
    this->showCursorThisFrame = true;
    this->seekIfOutOfFrame = true;
}
void TacradCLI::pushLine()
{
    this->str.append(TacradCLI::lineInit);
    this->postEntry();
}
void TacradCLI::writeLine(std::u32string_view line)
{
    this->str.push_back(U'\n');
    this->str.append(line);
    this->postEntry();
}

// theft https://stackoverflow.com/questions/48896142/is-it-possible-to-get-hash-values-as-compile-time-constants
constexpr auto hashString = [](std::u32string_view toHash) -> uint64_t
{
    // For this example, I'm requiring uint64_t to be 64-bit, but you could
    // easily change the offset and prime used to the appropriate ones
    // based on sizeof(uint64_t).
    static_assert(sizeof(uint64_t) == 8);
    // FNV-1a 64 bit algorithm
    uint64_t result = 0xcbf29ce484222325; // FNV offset basis

    for (char c : toHash) {
        result ^= c;
        result *= 1099511628211; // FNV prime
    }

    return result;
};

std::vector<std::pair<uint64_t, TacradCLI::Command>> TacradCLI::commands
{
    {
        hashString(U"help"),
        Command
        {
            .execute = &TacradCLI::commandHelp,
            .name = U"help",
            .description =
UR"(    desc:
    Display this help text.)"
        }
    },
    {
        hashString(U"clear"),
        Command
        {
            .execute = &TacradCLI::commandClear,
            .name = U"clear",
            .description =
UR"(    desc:
    Clear the console.)"
        }
    },
    {
        hashString(U"p"),
        Command
        {
            .execute = &TacradCLI::commandPlayOrTogglePlaying,
            .name = U"p",
            .aliasOrHidden = true,
            .aliasOf = { U"play", U"pause", U"resume" }
        }
    },
    {
        hashString(U">"),
        Command
        {
            .execute = &TacradCLI::commandResumeOrPlay,
            .name = U">",
            .aliasOrHidden = true,
            .aliasOf = { U"play", U"resume" }
        }
    },
    {
        hashString(U"play"),
        Command
        {
            .execute = &TacradCLI::commandPlay,
            .name = U"play",
            .description =
UR"(    args: [trackName] [trackName...]
        trackName: The name of the music item to play, or a query for one.
    desc:
    Plays a track.)"
        }
    },
    {
        hashString(U"resume"),
        Command
        {
            .execute = &TacradCLI::commandResume,
            .name = U"resume",
            .description =
UR"(    desc:
    Resumes playback.)"
        }
    },
    {
        hashString(U"res"),
        Command
        {
            .execute = &TacradCLI::commandResume,
            .name = U"res",
            .aliasOrHidden = true,
            .aliasOf = { U"resume" }
        }
    },
    {
        hashString(U"pause"),
        Command
        {
            .execute = &TacradCLI::commandPause,
            .name = U"pause",
            .description =
UR"(    desc:
    Pauses playback.)"
        }
    },
    {
        hashString(U"ps"),
        Command
        {
            .execute = &TacradCLI::commandPause,
            .name = U"ps",
            .aliasOrHidden = true,
            .aliasOf = { U"pause" }
        }
    },
    {
        hashString(U"#"),
        Command
        {
            .execute = &TacradCLI::commandPause,
            .name = U"#",
            .aliasOrHidden = true,
            .aliasOf = { U"pause" }
        }
    },
    {
        hashString(U"seek"),
        Command
        {
            .execute = &TacradCLI::commandSeek,
            .name = U"seek",
            .description =
UR"(    args: [seconds]
        seconds: Point in the track to seek to.
    desc:
    Seeks to a point in the music track currently playing.)"
        }
    },
    {
        hashString(U"volume"),
        Command
        {
            .execute = &TacradCLI::commandVolume,
            .name = U"volume",
            .description =
UR"(    args: [linVolume]
        linVolume: Volume as a linear quantity. 1.0 is as-is. > 1.0 will amplify.
    desc:
    Set the volume of playback.)"
        }
    },
    {
        hashString(U"vol"),
        Command
        {
            .execute = &TacradCLI::commandVolume,
            .name = U"vol",
            .aliasOrHidden = true,
            .aliasOf = { U"volume" }
        }
    },
    {
        hashString(U"stop"),
        Command
        {
            .execute = &TacradCLI::commandStop,
            .name = U"stop",
            .description =
UR"(    desc:
    Stops playback of the current track.)"
        }
    },
    {
        hashString(U"next"),
        Command
        {
            .execute = &TacradCLI::commandNext,
            .name = U"next",
            .aliasOrHidden = true
        }
    },
    {
        hashString(U"n"),
        Command
        {
            .execute = &TacradCLI::commandNext,
            .name = U"n",
            .aliasOrHidden = true
        }
    },
    {
        hashString(U">>"),
        Command
        {
            .execute = &TacradCLI::commandNext,
            .name = U">>",
            .aliasOrHidden = true
        }
    },
    {
        hashString(U"playlist"),
        Command
        {
            .execute = &TacradCLI::commandPlaylist,
            .name = U"playlist",
            .aliasOrHidden = true,
            .aliasOf = { U"playl" }
        }
    },
    {
        hashString(U"playl"),
        Command
        {
            .execute = &TacradCLI::commandPlaylist,
            .name = U"playl",
            .description =
UR"(    args: [flag]
        flag:
        Flag is one of -
        --next [alias: -n cmdalias: next, n, >>]: Play the next track on the list.
        --shuffle [alias: -sh]: Set the playlist to shuffle mode.
        --sequential [alias: --seq, -sq]: Set the playlist to sequential mode.
        --queued [alias: -q]: Set the playlist to queued mode.
        --push [alias: -p]: Push a track to the end of the playlist music queue.
        --list [alias: -l]: List the tracks in the playlist music queue.
        --index [alias: -i]: Set the track to play in the playlist music queue.
        --loop [alias: -lp]:
            Set music items to loop or autoplay. If no bool argument is given, the state is toggled.
            args: [opt: state]
                state: Should the playlist loop / autoplay. i.e. true, 1, false, 0
        --remove [alias: -r]: Remove a track from the playlist music queue.
        --clear [alias: -c]: Clear the playlist music queue.
    desc:
    Playlist related commands.)"
        }
    },
    {
        hashString(U"pl"),
        Command
        {
            .execute = &TacradCLI::commandPlaylist,
            .name = U"pl",
            .aliasOrHidden = true,
            .aliasOf = { U"playl" }
        }
    },
    {
        hashString(U"exit"),
        Command
        {
            .execute = &TacradCLI::commandExit,
            .name = U"exit",
            .description =
UR"(    desc:
    Exit this interface.)"
        }
    }
};

void TacradCLI::commandHelp(const std::vector<std::u32string>& cmd)
{
    robin_hood::unordered_map<uint64_t, std::vector<std::u32string_view>> aliases;
    for (auto&[hash, command] : TacradCLI::commands)
    {
        if (!command.aliasOf.empty())
            for (auto& alias : command.aliasOf)
                aliases[hashString(alias)].push_back(command.name);
    }

    std::u32string helpText = U"Help Text\n";
    for (auto&[hash, command] : TacradCLI::commands)
    {
        if (!command.aliasOrHidden)
        {
            helpText.append(command.name);
            if (aliases.contains(hash))
            {
                helpText.append(U" [alias: ");
                for (auto it = aliases[hash].begin(); it != --aliases[hash].end(); ++it)
                {
                    helpText.append(*it);
                    helpText.append(U", ");
                }
                helpText.append(aliases[hash].back());
                helpText.push_back(U']');
            }
            helpText.push_back(U'\n');
            helpText.append(command.description).push_back(U'\n');
        }
    }
    this->writeLine(helpText);
}
void TacradCLI::commandClear(const std::vector<std::u32string>& cmd)
{
    this->str.clear();
    this->str.append(TacradCLI::conInit);
    this->postEntry();
}
void TacradCLI::commandPlayOrTogglePlaying(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() == 1 && MusicPlayer::playing)
    {
        if (MusicPlayer::paused)
            MusicPlayer::musicResume();
        else MusicPlayer::musicPause();
    }
    else this->commandPlay(cmd);
}
void TacradCLI::commandResumeOrPlay(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() == 1 && MusicPlayer::paused)
        MusicPlayer::musicResume();
    else this->commandPlay(cmd);
}
void TacradCLI::commandPlay(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() < 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] Track title argument must be given to \"play\"!\n");
        return;
    }

    if (MusicPlayer::playing)
        MusicPlayer::stopMusic();

    std::u32string lookupName = cmd[1];
    for (auto& word : std::span(++++cmd.begin(), cmd.end()))
    {
        lookupName.push_back(U' ');
        lookupName.append(word);
    }
    MusicPlayer::startMusic(lookupName);
}
void TacradCLI::commandResume(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() > 1) [[unlikely]]
    {
        this->writeLine(U"[log.error] \"resume\" takes no arguments!\n");
        return;
    }

    if (MusicPlayer::playing)
        MusicPlayer::musicResume();
    else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
}
void TacradCLI::commandPause(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() > 1) [[unlikely]]
    {
        this->writeLine(U"[log.error] \"pause\" takes no arguments!\n");
        return;
    }

    if (MusicPlayer::playing)
        MusicPlayer::musicPause();
    else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
}
void TacradCLI::commandSeek(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() < 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] Seek position argument (in seconds) must be given to \"seek\"!\n");
        return;
    }
    if (cmd.size() > 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] Extra arguments given to \"seek\"!\n");
        return;
    }

    if (MusicPlayer::playing)
    {
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
        char* readEnd = nullptr;
        std::string convBytes = conv.to_bytes(cmd[1]);
        float q = std::strtof(convBytes.c_str(), &readEnd);
    
        if ((readEnd - convBytes.data()) != convBytes.size())
        {
            this->writeLine(U"[log.error] Invalid index argument given to \"seek\"!\n");
            return;
        }

        float seekQuery = (float)ma_engine_get_sample_rate(&MusicPlayer::engine) * q;
        if (seekQuery >= 0.0f && seekQuery <= MusicPlayer::frameLen)
            ma_sound_seek_to_pcm_frame(&MusicPlayer::music, (ma_uint64)seekQuery);
        else this->writeLine(U"[log.error] Seek query out of duration of media!\n");
    }
    else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
}
void TacradCLI::commandVolume(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() < 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] Volume argument (linear) must be given to \"vol\"!\n");
        return;
    }
    if (cmd.size() > 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] Extra arguments given to \"vol\"!\n");
        return;
    }

    std::u32string vStr = cmd[1];
    std::wstring wvStr; wvStr.reserve(vStr.size());
    for (auto c : vStr)
        wvStr.push_back(c);
    std::wistringstream ss(std::move(wvStr));
    float v; ss >> v;
    ma_engine_set_volume(&MusicPlayer::engine, v);
}
void TacradCLI::commandStop(const std::vector<std::u32string>& cmd)
{
    if (MusicPlayer::playing)
        MusicPlayer::stopMusic();
    else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" to start media.\n");
}
void TacradCLI::commandNext(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() > 2)
        this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
    
    MusicPlayer::next();
}
void TacradCLI::commandPlaylist(const std::vector<std::u32string>& cmd)
{
    if (cmd.size() < 2) [[unlikely]]
    {
        this->writeLine(U"[log.error] \"playl\" Takes at least a flag argument!\n");
        return;
    }

    switch (hashString(cmd[1]))
    {
    case hashString(U"--next"):
    case hashString(U"-n"):
        this->commandNext(cmd);
        break;
    case hashString(U"--shuffle"):
    case hashString(U"-sh"):
        if (cmd.size() > 2)
        {
            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
            break;
        }
        MusicPlayer::type = PlaylistType::Shuffle;
        break;
    case hashString(U"--Sequential"):
    case hashString(U"--seq"):
    case hashString(U"-sq"):
        if (cmd.size() > 2)
        {
            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
            break;
        }
        MusicPlayer::type = PlaylistType::Sequential;
        break;
    case hashString(U"--queued"):
    case hashString(U"-q"):
        if (cmd.size() > 2)
        {
            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
            break;
        }
        MusicPlayer::type = PlaylistType::Queued;
        break;
    case hashString(U"--push"):
    case hashString(U"-p"):
        {
            if (cmd.size() < 3)
            {
                this->writeLine(U"[log.error] \"playl --push\" requires at least a one-word music track query!\n");
                break;
            }
            std::u32string lookupName = cmd[2];
            for (auto& word : std::span(++++++cmd.begin(), cmd.end()))
            {
                lookupName.push_back(U' ');
                lookupName.append(word);
            }
            fs::path path;
            std::u32string name = MusicPlayer::musicLookup(lookupName, path);
            MusicPlayer::queue.push_back(std::make_pair(name, std::move(path)));
            this->writeLine(std::u32string(U"[log.info] Adding \"").append(name).append(U"\" to playlist music queue.\n"));
        }
        break;
    case hashString(U"--list"):
    case hashString(U"-l"):
        {
            if (cmd.size() > 2)
            {
                this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                break;
            }

            if (!MusicPlayer::queue.empty())
            {
                std::u32string playlist;
                size_t i = 1;
                for (auto it = MusicPlayer::queue.begin(); it != MusicPlayer::queue.end(); ++it)
                {
                    std::wostringstream ss;
                    ss << i++;
                    std::wstring wiStr = std::move(ss).str();
                    std::u32string iStr; iStr.reserve(wiStr.size());
                    for (auto c : wiStr)
                        iStr.push_back(c);
                    playlist.append(iStr).append(U". ").append(it->first);
                    if (it == MusicPlayer::queuePos)
                        playlist.append(U" < You Are Here");
                    playlist.push_back(U'\n');
                }
                this->writeLine(std::u32string(U"[log.info]\n").append(playlist));
            }
            else this->writeLine(U"[log.info] <Empty Playlist>\n");
        }
        break;
    case hashString(U"--index"):
    case hashString(U"-i"):
        {
            if (cmd.size() > 3)
            {
                this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                break;
            }
            if (cmd.size() < 3)
            {
                this->writeLine(U"[log.error] \"playl --index\" requires an index argument!\n");
                break;
            }

            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
            size_t readCount = 0;
            int index = std::stoi(conv.to_bytes(cmd[2]), &readCount);
            if (readCount != cmd[2].size())
            {
                this->writeLine(U"[log.error] Invalid index argument given to \"playl --index\"!\n");
                break;
            }
            if (index < 1 || index > MusicPlayer::queue.size())
            {
                this->writeLine(U"[log.error] Index argument given to \"playl --index\" out of range!\n");
                break;
            }

            auto queryPos = MusicPlayer::queue.begin();
            std::advance(queryPos, index - 1);

            if (MusicPlayer::queuePos != queryPos)
            {
                MusicPlayer::queuePos = queryPos;
                bool wasPaused = MusicPlayer::paused;
                if (MusicPlayer::playing)
                    MusicPlayer::stopMusic();
                MusicPlayer::tryPlayNextQueued(MusicPlayer::paused);
            }
        }
        break;
    case hashString(U"--remove"):
    case hashString(U"-r"):
        {
            if (cmd.size() < 3)
            {
                this->writeLine(U"[log.error] \"playl --remove\" requires an index argument!\n");
                break;
            }
            if (MusicPlayer::queue.empty())
            {
                this->writeLine(U"[log.error] Playlist music queue is empty!\n");
                break;
            }

            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
            size_t readCount = 0;
            int index = std::stoi(conv.to_bytes(cmd[2]), &readCount);
            if (readCount != cmd[2].size())
            {
                this->writeLine(U"[log.error] Invalid index argument given to \"playl --remove\"!\n");
                break;
            }
            if (index < 1 || index > MusicPlayer::queue.size())
            {
                this->writeLine(U"[log.error] Index argument given to \"playl --index\" out of range!\n");
                break;
            }
            
            auto queryPos = MusicPlayer::queue.begin();
            std::advance(queryPos, index - 1);

            if (MusicPlayer::queuePos == queryPos)
            {
                MusicPlayer::queuePos = ++decltype(queryPos)(queryPos) != MusicPlayer::queue.end() ? queryPos : MusicPlayer::queue.begin();
                bool wasPaused = MusicPlayer::paused;
                if (MusicPlayer::playing)
                    MusicPlayer::stopMusic();
                MusicPlayer::tryPlayNextQueued(MusicPlayer::paused);
            }

            MusicPlayer::queue.erase(queryPos);
        }
        break;
    case hashString(U"--loop"):
    case hashString(U"-lp"):
        switch (cmd.size())
        {
        case 2:
            MusicPlayer::loop = !MusicPlayer::loop;
            goto LoopSet;
        case 3:
            switch (hashString(cmd[2]))
            {
            case hashString(U"true"):
            case hashString(U"t"):
            case hashString(U"1"):
                MusicPlayer::loop = true;
                goto LoopSet;
            case hashString(U"false"):
            case hashString(U"f"):
            case hashString(U"0"):
                MusicPlayer::loop = false;
                goto LoopSet;
            default:
                this->writeLine(U"[log.error] Unknown boolean argument given to \"playl\"!\n");
            }
            break;
        LoopSet:
            {
                std::wostringstream ss;
                ss << L"[log.info] Playlist queue looping set to " << (MusicPlayer::loop ? L"true" : L"false") << L'!';
                std::wstring outStr = ss.str();
                std::u32string out; out.reserve(outStr.size());
                for (auto c : outStr)
                    out.push_back(c);
                this->writeLine(out);
            }
            break;
        default:
            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
        }
        break;
    case hashString(U"--clear"):
    case hashString(U"-c"):
        if (cmd.size() > 2)
        {
            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
            break;
        }
        MusicPlayer::queue.clear();
        MusicPlayer::queuePos = MusicPlayer::queue.end();
        this->writeLine(U"[log.info] Clearing playlist queue.\n");
        break;
    default:
        this->writeLine(U"[log.warn] Unknown flag argument given to \"playl\".\n");
    }
}
void TacradCLI::commandExit(const std::vector<std::u32string>& cmd)
{
    if (MusicPlayer::playing)
        MusicPlayer::stopMusic();
    Application::quit();
}

void TacradCLI::onCommand(std::u32string_view command)
{
    constexpr auto split = [](std::u32string_view str) -> std::vector<std::u32string>
    {
        std::vector<std::u32string> ret;
        size_t beg = 0, end = str.find_first_of(U' ', beg);
        while (end != std::u32string_view::npos)
        {
            if (str.begin() + beg != str.begin() + end)
                ret.push_back(std::u32string(str.begin() + beg, str.begin() + end));

            beg = end + 1;
            end = str.find_first_of(' ', beg);
        }
        if (str.begin() + beg != str.end()) [[likely]]
            ret.push_back(std::u32string(str.begin() + beg, str.end()));
        return std::move(ret);
    };

    if (this->cmdHistory.empty() || this->cmdHistory.back() != command)
        this->cmdHistory.emplace_back(command.begin(), command.end());
    this->nextHistoryIndex = this->cmdHistory.size() - 1;

    std::vector<std::u32string> cmd = split(command);

    if (!cmd.empty())
    {
        auto it = TacradCLI::commands.begin();
        while (it != TacradCLI::commands.end() && it->first != hashString(cmd[0]))
            ++it;
        if (it == TacradCLI::commands.end())
        {
            for (char c : cmd[0])
            {
                if (!std::isspace(c))
                {
                    this->writeLine(U"[log.error] Unknown command! Enter \"help\" to view a list of available commands!\n");
                    break;
                }
            }
        }
        else (this->*it->second.execute)(cmd);
    }
}

static struct TacradCLISystem
{
    inline TacradCLISystem()
    {
        EngineEvent::OnInitialize += []
        {
            TacradCLI::font = file_cast<TrueTypeFontPackageFile>(PackageManager::lookupFileByPath(L"assets/Inconsolata/static/Inconsolata-Regular.ttf"));
            
            if (ma_result code = ma_engine_init(nullptr, &MusicPlayer::engine); code != MA_SUCCESS) [[unlikely]]
            {
                Debug::logError("Audio engine failed to initialize with code ", code, ".\n");
                Application::quit();
            }
            
            Input::beginQueryTextInput();
        };
        EngineEvent::OnQuit += []
        {
            Input::endQueryTextInput();

            EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
            {
                if (MusicPlayer::playing)
                    MusicPlayer::stopMusic();
            });
                
            ma_engine_uninit(&MusicPlayer::engine);
        };

        auto onTextInput = [](Key key)
        {
            switch (key)
            {
            case Key::Backspace:
                EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                {
                    if (std::u32string_view(cli->str.end() - TacradCLI::lineInit.size(), cli->str.end()) != TacradCLI::lineInit)
                    {
                        cli->str.pop_back();
                        cli->postEntry();
                    }
                });
                break;
            case Key::UpArrow:
                EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                {
                    if (!cli->cmdHistory.empty())
                    {
                        size_t i = cli->str.size() - TacradCLI::lineInit.size();
                        while (std::u32string_view(&cli->str[i], TacradCLI::lineInit.size()) != TacradCLI::lineInit)
                        {
                            cli->str.pop_back();
                            --i;
                        }
                        cli->str.append(cli->cmdHistory[cli->nextHistoryIndex]);
                        if (cli->nextHistoryIndex > 0)
                            --cli->nextHistoryIndex;

                        cli->postEntry();
                    }
                });
                break;
            case Key::DownArrow:
                EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                {
                    if (!cli->cmdHistory.empty())
                    {
                        if (cli->nextHistoryIndex + 1 < cli->cmdHistory.size())
                        {
                            size_t i = cli->str.size() - TacradCLI::lineInit.size();
                            while (std::u32string_view(&cli->str[i], TacradCLI::lineInit.size()) != TacradCLI::lineInit)
                            {
                                cli->str.pop_back();
                                --i;
                            }
                            ++cli->nextHistoryIndex;
                            cli->str.append(cli->cmdHistory[cli->nextHistoryIndex]);
                        }

                        cli->postEntry();
                    }
                });
                break;
            case Key::Return:
            case Key::SecondaryReturn:
            case Key::NumpadEnter:
                EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                {
                    size_t beg = cli->str.size(), end = cli->str.size();
                    bool allSpaces = true;
                    for (size_t i = cli->str.size() - TacradCLI::lineInit.size() + 1; i --> 0;) // downto operator lol
                    {
                        if (!std::isspace(cli->str[i + TacradCLI::lineInit.size() - 1]))
                            allSpaces = false;
                        if (std::u32string_view(&cli->str[i], TacradCLI::lineInit.size()) == TacradCLI::lineInit)
                        {
                            beg = i + TacradCLI::lineInit.size();
                            break;
                        }
                    }
                    if (!allSpaces)
                        cli->onCommand(std::u32string_view(&cli->str[beg], end - beg));
                    cli->pushLine();
                });
                break;
            }
        };
        EngineEvent::OnKeyDown += onTextInput;
        EngineEvent::OnKeyRepeat += onTextInput;
        EngineEvent::OnTextInput += [](const std::u32string& text)
        {
            for (auto it = text.begin(); it != text.end(); ++it)
            {
                if (*it != 0) [[likely]]
                {
                    EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                    {
                        cli->str.push_back(*it);
                        cli->postEntry();
                    });
                }
            }
        };
        
        EngineEvent::OnTick += []
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
            {
                ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&MusicPlayer::music);
                if (MusicPlayer::playing && curFrame != MusicPlayer::prevFrame)
                {
                    if (curFrame >= MusicPlayer::frameLen)
                    {
                        if (fs::exists("music/"))
                        {
                            switch (MusicPlayer::type)
                            {
                            case PlaylistType::Sequential:
                                goto TrackRestart;
                            case PlaylistType::Shuffle:
                                MusicPlayer::stopMusic();
                                MusicPlayer::tryPlayNextShuffle();
                                break;
                            case PlaylistType::Queued:
                                {
                                    MusicPlayer::stopMusic();

                                    auto prev = MusicPlayer::queuePos;
                                    MusicPlayer::incrQueuePos();
                                    if (MusicPlayer::queuePos != prev)
                                        MusicPlayer::tryPlayNextQueued();
                                    else cli->pushLine();
                                }
                                break;
                            }
                        }
                        else
                        {
                            TrackRestart:
                            MusicPlayer::paused = true;
                            ma_sound_seek_to_pcm_frame(&MusicPlayer::music, 0);
                        }
                    }
                    MusicPlayer::prevFrame = curFrame;
                }

                std::u32string displayStr = cli->str;
                // if (MusicPlayer::playing)
                // {
                //     constexpr auto sdFloat = [](float val, std::streamsize prec = 1) -> std::u32string
                //     {
                //         std::wostringstream ss;
                //         ss << std::fixed << std::setprecision(prec) << val;
                //         std::wstring wret = std::move(ss).str();
                //         std::u32string ret; ret.reserve(wret.size());
                //         for (auto c : wret)
                //             ret.push_back((char32_t)c);
                //         return ret;
                //     };
// 
                //     std::u32string playBarBeg = U"Now Playing: ";
                //     playBarBeg
                //     .append(MusicPlayer::paused ? U"# " : U"> ")
                //     .append(MusicPlayer::musicName)
                //     .append(U"  ")
                //     .append(sdFloat((float)ma_sound_get_time_in_pcm_frames(&MusicPlayer::music) / (float)ma_engine_get_sample_rate(&MusicPlayer::engine)))
                //     .append(U" / ")
                //     .append(sdFloat(MusicPlayer::musicLen))
                //     .append(U" (");
                //     if (int32_t mins = (int32_t)MusicPlayer::musicLen / 60; mins > 0)
                //     {
                //         std::wstring wminsStr = std::to_wstring(mins);
                //         std::u32string minsStr; minsStr.reserve(wminsStr.size());
                //         for (auto c : wminsStr)
                //             minsStr.push_back((char32_t)c);
                //         playBarBeg
                //         .append(minsStr)
                //         .append(U"min ");
                //     }
                //     std::wstring wsecsStr = std::to_wstring(int32_t(MusicPlayer::musicLen + 0.5f) % 60);
                //     std::u32string secsStr; secsStr.reserve(wsecsStr.size());
                //     for (auto c : wsecsStr)
                //         secsStr.push_back((char32_t)c);
                //     playBarBeg
                //     .append(secsStr)
                //     .append(U"s)  ")
                //     .append(sdFloat(ma_engine_get_volume(&MusicPlayer::engine), 2))
                //     .append(U"v |");
                //     std::u32string playBarEnd = U"|";
// 
                //     ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&MusicPlayer::music);
// 
                //     int64_t barLen = (int64_t)(cli->width()) - (int64_t)playBarBeg.size() - (int64_t)playBarEnd.size();
                //     int64_t played = barLen * (int64_t)curFrame / (int64_t)MusicPlayer::frameLen;
                //     std::u32string out;
                //     out
                //     .append(playBarBeg)
                //     .append(std::u32string(std::max((int64_t)0, played), '='))
                //     .append(U"+")
                //     .append(std::u32string(std::max((int64_t)0, barLen - played - 1), '-'))
                //     .append(playBarEnd);
// 
                //     if (out.size() > std::max(cli->width(), uint32_t(1)) && cli->width() > 3)
                //     {
                //         for (size_t i = out.size() - 1; i >= cli->width() - 3; i--)
                //             out.pop_back();
                //         out.append(U"...");
                //     }
// 
                //     size_t workingLineStart = displayStr.find_last_of(U'\n');
                //     std::u32string workingLine;
                //     if (workingLineStart != std::u32string::npos)
                //     {
                //         workingLine = displayStr.substr(workingLineStart + 1);
                //         displayStr.erase(workingLineStart + 1);
                //         displayStr.append(out);
                //         displayStr.push_back(U'\n');
                //         displayStr.append(workingLine);
                //     }
                // }
                if (cli->showCursorThisFrame)
                    displayStr.push_back(cursorCharacter);
                if (cli->nextTimePoint > 0.64f)
                {
                    cli->nextTimePoint -= 0.64f;
                    cli->showCursorThisFrame = !cli->showCursorThisFrame;
                }
                cli->_text->text = std::move(displayStr);

                cli->_text->rectTransform()->rect = RectFloat
                (
                    0, cli->rectTransform()->rect().right - CONSOLE_PADDING_SIDE,
                    -cli->_text->calculateBestFitHeight(), cli->rectTransform()->rect().left + CONSOLE_PADDING_SIDE
                );
                
                if (cli->seekIfOutOfFrame)
                {
                    if (cli->_text->rectTransform()->localPosition().y + cli->_text->rectTransform()->rect().bottom < -cli->rectTransform()->rect().height())
                    {
                        cli->_text->rectTransform()->localPosition = sysm::vector2(0.0f, -cli->rectTransform()->rect().height() - cli->_text->rectTransform()->rect().bottom);
                    }

                    cli->seekIfOutOfFrame = false;
                }

                cli->nextTimePoint += Time::deltaTime();
            });
        };
        EngineEvent::OnMouseScroll += [](sysm::vector2 scroll)
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
            {
                cli->_text->rectTransform()->localPosition = sysm::vector2(0.0f, std::max(-CONSOLE_PADDING_TOP, cli->_text->rectTransform()->localPosition().y - scroll.y * cli->_text->fontSize() * 3.0f));

                cli->nextTimePoint = 0.0f;
                cli->showCursorThisFrame = true;
            });
        };
    }
} init;
