#pragma once

#define NOMINMAX 1

#include <format>
#include <miniaudio.h>
#include <random>
#include <sstream>

#include <Components/Panel.h>
#include <Components/Text.h>
#include <Core/Application.h>
#include <Core/Display.h>
#include <Core/Time.h>
#include <EntityComponentSystem/ComponentData.h>
#include <EntityComponentSystem/EngineEvent.h>
#include <EntityComponentSystem/EntityManagement.h>
#include <Objects/Entity2D.h>

namespace fs = std::filesystem;

using namespace Firework;
using namespace Firework::Mathematics;
using namespace Firework::PackageSystem;

struct TacradCLISystem;

constexpr char32_t cursorCharacter = U'\x2582';

class TacradCLI : public ComponentData2D
{
    inline static TrueTypeFontPackageFile* font;
    inline static std::u32string_view lineInit = U"\n[tacrad] ";

    Text* _text;
    std::u32string str =
UR"(tacrad music player [Version 1.0.0]
(c) 2024 Yellow Chemistry Publishing. All rights reserved.

Welcome to the tacrad CLI! (Enter "help" for details.)
)";

    std::vector<std::u32string> cmdHistory;
    size_t nextHistoryIndex = SIZE_MAX;

    float nextTimePoint = 0.0f;
    bool showCursorThisFrame = true;

    bool seekIfOutOfFrame = false;
    float scrollTop = 0.0f;

    uint32_t width() // In **characters**.
    {
        Typography::Font& font = this->_text->fontFile()->fontHandle();
        Typography::GlyphMetrics metrics = font.getGlyphMetrics(font.getGlyphIndex(U' ')); // Monospace font or we're screwed.
        return uint32_t((this->rectTransform()->rect().right - this->rectTransform()->rect().left) / ((float)metrics.advanceWidth * (float)this->_text->fontSize / float(font.ascent - font.descent)));
    }

    void postEntry()
    {
        this->nextTimePoint = 0.0f;
        this->showCursorThisFrame = true;
        this->seekIfOutOfFrame = true;
    }
    void pushLine()
    {
        this->str.append(TacradCLI::lineInit);
        this->postEntry();
    }
    void writeLine(std::u32string_view line)
    {
        this->str.push_back(U'\n');
        this->str.append(line);
        this->postEntry();
    }

    // CLI ^ / Music Player v

    inline static std::random_device seeder;
    inline static std::mt19937 randEngine { seeder() };
    inline static std::uniform_real_distribution<float> dist { 0.0f, 1.0f };

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
        std::transform(ret.begin(), ret.end(), ret.begin(), [](char c){ return std::tolower(c); });
        return ret;
    }
    inline static std::u32string musicLookup(std::u32string_view name, fs::path& file)
    {
        std::u32string compare; compare.append(name);
        for (auto& c : compare)
            c = std::tolower(c);
        
        if (fs::exists("music/"))
        {
            for (auto& dir : fs::recursive_directory_iterator("music/"))
            {
                if (dir.is_regular_file())
                {
                    if (toLower(dir.path().stem().u32string()) == compare)
                    {
                        file = dir.path();
                        return dir.path().stem().u32string();
                    }
                }
            }
            for (auto& dir : fs::recursive_directory_iterator("music/"))
            {
                if (dir.is_regular_file())
                {
                    if (toLower(dir.path().stem().u32string()).starts_with(compare))
                    {
                        file = dir.path();
                        return dir.path().stem().u32string();
                    }
                }
            }
            for (auto& dir : fs::recursive_directory_iterator("music/"))
            {
                if (dir.is_regular_file())
                {
                    if (toLower(dir.path().stem().u32string()).contains(compare))
                    {
                        file = dir.path();
                        return dir.path().stem().u32string();
                    }
                }
            }
        }

        file = "err.";
        return U"Error! (This will end poorly later.)";
    }

    void startMusic(std::u32string_view query)
    {
        fs::path musicFile;
        std::u32string _musicName = musicLookup(query, musicFile);
        if (ma_sound_init_from_file(&engine, musicFile.string().c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
        {
            ma_sound_start(&music);
            ma_sound_get_length_in_pcm_frames(&music, &frameLen);
            ma_sound_get_length_in_seconds(&music, &musicLen);
            musicName = _musicName;
            playing = true;
        }
        else this->writeLine(U"[log.error] Music query doesn't exist!\n");
    }
    void tryPlayNextAlphabetical(std::u32string_view prev, bool wasPaused)
    {
        fs::path file;
        std::u32string name(1, U'z');
        if (prev.empty() && fs::exists("music/"))
        {
            for (auto it : fs::recursive_directory_iterator("music/"))
            {
                if (it.path().stem().u32string() < name)
                {
                    file = it.path();
                    name = file.stem().u32string();
                }
            }
        }
        else
        {
            std::vector<std::pair<std::u32string, fs::path>> tracks;
            if (fs::exists("music/"))
                for (auto it : fs::recursive_directory_iterator("music/"))
                    if (it.is_regular_file())
                        tracks.push_back(std::make_pair(it.path().stem().u32string(), it.path()));

            if (tracks.empty())
            {
                this->writeLine(U"[log.info] No music to play. (Add some!)\n");
                return;
            }

            std::sort(tracks.begin(), tracks.end(), [](auto a, auto b)
            {
                return a.second < b.second;
            });
            auto next = tracks.end();
            for (auto it = tracks.begin(); it != tracks.end(); ++it)
            {
                if (it->first == prev)
                {
                    next = it;
                    break;
                }
            }
            ++next;
            if (next == tracks.end())
            {
                this->writeLine(U"[log.info] End of playlist!\n");
                return;
            }
            name = next->first;
            file = next->second;
        }
        if (ma_sound_init_from_file(&engine, file.string().c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
        {
            ma_sound_get_length_in_pcm_frames(&music, &frameLen);
            ma_sound_get_length_in_seconds(&music, &musicLen);
            musicName = std::move(name);
            playing = true;
            if (!wasPaused)
                ma_sound_start(&music);
            else paused = true;
        }
        else Debug::logError("[log.error] Music query doesn't exist! [dev] name: ", name, ", file: ", file, '\n');
    }
    void stopMusic()
    {
        ma_sound_stop(&music);
        ma_sound_uninit(&music);
        playing = false;
        paused = false;
    }
    void tryPlayNextShuffle(bool wasPaused = false)
    {
        size_t ct = 0;
        if (fs::exists("music/"))
            for (auto& dir : fs::recursive_directory_iterator("music/"))
                if (dir.path().stem().u32string() != musicName)
                    ++ct;
        if (ct != 0)
        {
            More:
            for (auto& dir : fs::recursive_directory_iterator("music/"))
            {
                if (dir.is_regular_file() && dir.path().stem().u32string() != musicName && dist(randEngine) < 1.0f / (float)ct)
                {
                    fs::path musicFile;
                    std::u32string _musicName = musicLookup(dir.path().stem().u32string(), musicFile);
                    auto t = dir.path().stem().string();
                    if (ma_sound_init_from_file(&engine, musicFile.string().c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
                    {
                        ma_sound_get_length_in_pcm_frames(&music, &frameLen);
                        ma_sound_get_length_in_seconds(&music, &musicLen);
                        musicName = std::move(_musicName);
                        playing = true;
                        if (!wasPaused)
                            ma_sound_start(&music);
                        else paused = true;

                        return;
                    }
                    else this->writeLine(U"[log.error] Failed to load next track, shuffling for new one.\n");
                }
            }
            goto More;
        }
        else this->writeLine(U"[log.info] No music to play. (Add some!)\n");
    }
    void incrQueuePos()
    {
        if (queuePos == queue.end())
            queuePos = queue.begin();
        else if (++decltype(queuePos)(queuePos) == queue.end())
        {
            if (!loop)
            {
                this->writeLine(U"[log.info] End of playlist!\n");
                return;
            }
            else queuePos = queue.begin();
        }
        else ++queuePos;
    }
    void tryPlayNextQueued(bool wasPaused = false)
    {
        if (queue.empty())
        {
            this->writeLine(U"[log.warn] Empty playlist!\n");
            return;
        }

        goto FirstTry;
        {
            Retry:
            incrQueuePos();
        }
        FirstTry:
        
        if (ma_sound_init_from_file(&engine, queuePos->second.string().c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
        {
            ma_sound_get_length_in_pcm_frames(&music, &frameLen);
            ma_sound_get_length_in_seconds(&music, &musicLen);
            musicName = queuePos->first;
            playing = true;
            if (!wasPaused)
                ma_sound_start(&music);
            else paused = true;
        }
        else
        {
            Debug::logError("[log.error] Couldn't load next music track in queue, skipping! [dev] name: ", queuePos->first, ", path: ", queuePos->second, '\n');
            goto Retry;
        }
    }
    
    void onCommand(std::u32string_view command)
    {
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
            switch (hashString(cmd[0]))
            {
            case hashString(U"help"):
                this->writeLine
(UR"(Help Text
play [alias: p, >]
    args: [trackName]
        trackName: The name of the music item to play, or a query for one.
    desc:
    Plays a track.
pause [alias: ps, p, #]
    desc:
    Pauses playback.
resume [alias: res, p, >]
    desc:
    Resumes playback.
seek
    args: [seconds]
        seconds: Point in the track to seek to.
    desc:
    Seeks to a point in the music track currently playing.
volume [alias: vol]
    args: [linVolume]
        linVolume: Volume as a linear quantity. 1.0 is as-is. > 1.0 will amplify.
    desc:
    Set the volume of playback.
stop
    desc:
    Stops playback of the current track.
playl [alias: playlist, pl]
    args: [flag]
        flag:
        Flag is one of -
        --next [alias: -n cmdalias: next, n, >>]: Play the next track on the list.
        --shuffle [alias: -sh]: Set the playlist to shuffle mode.
        --sequential [alias: --seq, -sq]: Set the playlist to sequential mode.
    desc:
        Playlist related commands.
exit
    desc:
    Exit this interface.
)");
                break;
            case hashString(U"p"):
                if (cmd.size() == 1 && playing)
                {
                    if (paused)
                        goto MusicResume;
                    else goto MusicPause;
                }
                goto MusicPlay;
            case hashString(U">"):
                if (cmd.size() == 1 && paused)
                    goto MusicResume;
                [[fallthrough]];
            case hashString(U"play"):
                {
                    MusicPlay:
                    if (cmd.size() < 2) [[unlikely]]
                    {
                        this->writeLine(U"[log.error] Track title argument must be given to \"play\"!\n");
                        break;
                    }

                    if (playing)
                        stopMusic();
                
                    std::u32string lookupName = cmd[1];
                    for (auto& word : std::span(++++cmd.begin(), cmd.end()))
                    {
                        lookupName.push_back(U' ');
                        lookupName.append(word);
                    }
                    startMusic(lookupName);
                }
                break;
            case hashString(U"resume"):
            case hashString(U"res"):
                if (cmd.size() > 1) [[unlikely]]
                {
                    this->writeLine(U"[log.error] \"resume\" takes no arguments!\n");
                    break;
                }

                if (playing)
                {
                    MusicResume:
                    ma_sound_start(&music);
                    paused = false;
                }
                else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
                break;
            case hashString(U"pause"):
            case hashString(U"ps"):
            case hashString(U"#"):
                if (cmd.size() > 1) [[unlikely]]
                {
                    this->writeLine(U"[log.error] \"pause\" takes no arguments!\n");
                    break;
                }

                if (playing)
                {
                    MusicPause:
                    ma_sound_stop(&music);
                    paused = true;
                }
                else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
                break;
            case hashString(U"seek"):
                if (cmd.size() < 2) [[unlikely]]
                {
                    this->writeLine(U"[log.error] Seek position argument (in seconds) must be given to \"seek\"!\n");
                    break;
                }
                if (cmd.size() > 2) [[unlikely]]
                {
                    this->writeLine(U"[log.error] Extra arguments given to \"seek\"!\n");
                    break;
                }

                if (playing)
                {
                    std::wstring qStr; qStr.reserve(cmd[1].size());
                    for (auto c : cmd[1])
                        qStr.push_back(c);
                    std::wstringstream ss(std::move(qStr));
                    float q; ss >> q;
                    double seekQuery = (double)ma_engine_get_sample_rate(&engine) * q;
                    if (seekQuery >= 0.0 && seekQuery <= frameLen)
                    {
                        ma_sound_seek_to_pcm_frame(&music, (ma_uint64)seekQuery);
                    }
                    else this->writeLine(U"[log.error] Seek query out of duration of media!\n");
                }
                else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n");
                break;
            case hashString(U"volume"):
            case hashString(U"vol"):
                {
                    if (cmd.size() < 2) [[unlikely]]
                    {
                        this->writeLine(U"[log.error] Volume argument (linear) must be given to \"vol\"!\n");
                        break;
                    }
                    if (cmd.size() > 2) [[unlikely]]
                    {
                        this->writeLine(U"[log.error] Extra arguments given to \"vol\"!\n");
                        break;
                    }

                    std::u32string vStr = cmd[1];
                    std::wstring wvStr; wvStr.reserve(vStr.size());
                    for (auto c : vStr)
                        wvStr.push_back(c);
                    std::wistringstream ss(std::move(wvStr));
                    float v; ss >> v;
                    ma_engine_set_volume(&engine, v);
                }
                break;
            case hashString(U"stop"):
                if (playing)
                    stopMusic();
                else this->writeLine(U"[log.error] Not currently playing music! Use \"play\" to start media.\n");
                break;
            case hashString(U"next"):
            case hashString(U"n"):
            case hashString(U">>"):
                goto PlaylistNext;
            case hashString(U"playlist"):
            case hashString(U"playl"):
            case hashString(U"pl"):
                if (cmd.size() < 2) [[unlikely]]
                {
                    this->writeLine(U"[log.error] \"playl\" Takes at least a flag argument!\n");
                    break;
                }

                switch (hashString(cmd[1]))
                {
                case hashString(U"--next"):
                case hashString(U"-n"):
                    {
                        if (cmd.size() > 2)
                            this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                        PlaylistNext:
                        bool wasPaused = paused;
                        if (playing)
                            stopMusic();
                        switch (type)
                        {
                        case PlaylistType::Sequential:
                            tryPlayNextAlphabetical(musicName, wasPaused);
                            break;
                        case PlaylistType::Shuffle:
                            tryPlayNextShuffle(wasPaused);
                            break;
                        case PlaylistType::Queued:
                            incrQueuePos();
                            tryPlayNextQueued(wasPaused);
                            break;
                        }
                    }
                    break;
                case hashString(U"--shuffle"):
                case hashString(U"-sh"):
                    if (cmd.size() > 2)
                    {
                        this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                        break;
                    }
                    type = PlaylistType::Shuffle;
                    break;
                case hashString(U"--Sequential"):
                case hashString(U"--seq"):
                case hashString(U"-sq"):
                    if (cmd.size() > 2)
                    {
                        this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                        break;
                    }
                    type = PlaylistType::Sequential;
                    break;
                case hashString(U"--queued"):
                case hashString(U"-q"):
                    if (cmd.size() > 2)
                    {
                        this->writeLine(U"[log.error] Extra arguments given to \"playl\"!\n");
                        break;
                    }
                    type = PlaylistType::Queued;
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
                        std::u32string name = musicLookup(lookupName, path);
                        queue.push_back(std::make_pair(name, std::move(path)));
                        Debug::logError("[log.info] Adding \"", name, "\" to playlist music queue.\n");
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

                        if (!queue.empty())
                        {
                            std::u32string playlist;
                            size_t i = 1;
                            for (auto it = queue.begin(); it != queue.end(); ++it)
                            {
                                std::wostringstream ss;
                                ss << i++;
                                std::wstring wiStr = std::move(ss).str();
                                std::u32string iStr; iStr.reserve(wiStr.size());
                                for (auto c : wiStr)
                                    iStr.push_back(c);
                                playlist.append(iStr).append(U". ").append(it->first);
                                if (it == queuePos)
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

                        std::wstring diffStr; diffStr.reserve(cmd[2].size());
                        for (auto c : cmd[2])
                            diffStr.push_back(c);
                        std::wstringstream ss(std::move(diffStr));
                        int diff; size_t pos; ss >> diff; pos = ss.tellg();
                        if (pos != cmd[2].size() || pos < 1)
                        {
                            this->writeLine(U"[log.error] Invalid index argument given to \"playl --index\"!\n");
                            break;
                        }
                        if (pos > queue.size())
                        {
                            this->writeLine(U"[log.error] Index argument given to \"playl --index\" out of range!\n");
                            break;
                        }

                        auto queryPos = queue.begin();
                        std::advance(queryPos, diff - 1);

                        if (queuePos != queryPos)
                        {
                            queuePos = queryPos;
                            bool wasPaused = paused;
                            if (playing)
                                stopMusic();
                            tryPlayNextQueued(paused);
                        }
                    }
                    break;
                case hashString(U"--loop"):
                case hashString(U"-lp"):
                    switch (cmd.size())
                    {
                    case 2:
                        loop = !loop;
                        goto LoopSet;
                    case 3:
                        switch (hashString(cmd[2]))
                        {
                        case hashString(U"true"):
                        case hashString(U"t"):
                        case hashString(U"1"):
                            loop = true;
                            goto LoopSet;
                        case hashString(U"false"):
                        case hashString(U"f"):
                        case hashString(U"0"):
                            loop = false;
                            goto LoopSet;
                        default:
                            this->writeLine(U"[log.error] Unknown boolean argument given to \"playl\"!\n");
                        }
                        break;
                    LoopSet:
                        {
                            std::wostringstream ss;
                            ss << L"[log.info] Playlist queue looping set to " << (loop ? L"true" : L"false") << L'!';
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
                    queue.clear();
                    queuePos = queue.end();
                    this->writeLine(U"[log.info] Clearing playlist queue.\n");
                    break;
                default:
                    this->writeLine(U"[log.warn] Unknown flag argument given to \"playl\".\n");
                }
                break;
            case hashString(U"exit"):
                if (playing)
                    stopMusic();
                Application::quit();
            default:
                for (char c : cmd[0])
                {
                    if (!std::isspace(c))
                    {
                        this->writeLine(U"[log.error] Unknown command! Enter \"help\" to view a list of available commands!\n");
                        break;
                    }
                }
            }
        }
    }
public:
    inline TacradCLI() = default;

    inline void onCreate()
    {
        this->rectTransform()->position = Vector2(0.0f, Window::pixelHeight() / 2);
        this->rectTransform()->rect = RectFloat
        (
            0, Window::pixelWidth() / 2,
            -Window::pixelHeight(), -Window::pixelWidth() / 2
        );

        this->entity()->addComponent<Panel>()->color = Color(0.0f, 0.0f, 0.0f, 1.0f);

        Entity2D* textEntity = new Entity2D();
        textEntity->parent = this->entity();

        textEntity->rectTransform()->rect = this->rectTransform()->rect();
        this->_text = textEntity->addComponent<Text>();
        this->_text->rectTransform()->localPosition = Vector2(0.0f, 0.0f);
        this->_text->fontFile = TacradCLI::font;
        this->_text->horizontalAlign = TextAlign::Minor;
        this->_text->verticalAlign = TextAlign::Minor;

        this->pushLine();
    }

    const Property<float, float> fontSize
    {{
        [this]() -> float { return this->_text->fontSize; },
        [this](float value) { this->_text->fontSize = value; }
    }};

    friend struct ::TacradCLISystem;
};

extern struct TacradCLISystem
{
    inline TacradCLISystem()
    {
        EngineEvent::OnInitialize += []
        {
            TacradCLI::font = file_cast<TrueTypeFontPackageFile>(PackageManager::getCorePackageFileByPath(L"assets/dejavu-fonts-ttf-2.37/ttf/DejaVuSansMono.ttf"));
            
            if (ma_result code = ma_engine_init(nullptr, &TacradCLI::engine); code != MA_SUCCESS) [[unlikely]]
            {
                Debug::logError("Audio engine failed to initialize with code ", code, ".\n");
                Application::quit();
            }
        };
        auto onTextInput = [](Key key)
        {
            char32_t c = 0;
            if (std::to_underlying(key) >= std::to_underlying(Key::LetterA) && std::to_underlying(key) <= std::to_underlying(Key::LetterZ))
            {
                if (Input::keyHeld(Key::LeftShift) || Input::keyHeld(Key::RightShift))
                    c = U'A' + (std::to_underlying(key) - std::to_underlying(Key::LetterA));
                else c = U'a' + (std::to_underlying(key) - std::to_underlying(Key::LetterA));
            }
            if (std::to_underlying(key) >= std::to_underlying(Key::Number1) && std::to_underlying(key) <= std::to_underlying(Key::Number9))
                c = U'1' + (std::to_underlying(key) - std::to_underlying(Key::Number1));
            if (std::to_underlying(key) >= std::to_underlying(Key::Numpad1) && std::to_underlying(key) <= std::to_underlying(Key::Numpad9))
                c = U'1' + (std::to_underlying(key) - std::to_underlying(Key::Numpad1));
            if (key == Key::Number0 || key == Key::Numpad0)
                c = U'0';
            if (key == Key::Space || key == Key::NumpadSpace)
                c = U' ';
            switch (key)
            {
            case Key::BackQuote:
                c = U'`';
                break;
            case Key::ExclamationMark:
            case Key::NumpadExclamationMark:
                c = U'!';
                break;
            case Key::AtSign:
            case Key::NumpadAtSign:
                c = U'@';
                break;
            case Key::Hashtag:
            case Key::NumpadHashtag:
                c = U'#';
                break;
            case Key::Dollar:
                c = U'$';
                break;
            case Key::Percent:
            case Key::NumpadPercent:
                c = U'%';
                break;
            case Key::Caret:
                c = U'^';
                break;
            case Key::Ampersand:
            case Key::NumpadAmpersand:
                c = U'&';
                break;
            case Key::Asterisk:
                c = U'*';
                break;
            case Key::LeftParenthesis:
            case Key::NumpadLeftParenthesis:
                c = U'(';
                break;
            case Key::RightParenthesis:
            case Key::NumpadRightParenthesis:
                c = U')';
                break;
            case Key::Minus:
            case Key::NumpadMinus:
                c = U'-';
                break;
            case Key::Underscore:
                c = U'_';
                break;
            case Key::Plus:
            case Key::NumpadPlus:
                c = U'+';
                break;
            case Key::Equals:
            case Key::NumpadEquals:
                c = U'=';
                break;
            case Key::Comma:
            case Key::NumpadComma:
                c = U',';
                break;
            case Key::Period:
            case Key::NumpadPeriod:
                c = U'.';
                break;
            }

            if (c != 0) [[likely]]
            {
                EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
                {
                    cli->str.push_back(c);
                    cli->postEntry();
                });
            }
            else switch (key)
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
        EngineEvent::OnTick += []
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
            {
                ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&cli->music);
                if (cli->playing && curFrame != cli->prevFrame)
                {
                    if (curFrame >= cli->frameLen)
                    {
                        if (fs::exists("music/"))
                        {
                            switch (cli->type)
                            {
                            case TacradCLI::PlaylistType::Sequential:
                                goto TrackRestart;
                            case TacradCLI::PlaylistType::Shuffle:
                                cli->stopMusic();
                                cli->tryPlayNextShuffle();
                                break;
                            case TacradCLI::PlaylistType::Queued:
                                cli->stopMusic();
                                cli->incrQueuePos();
                                cli->tryPlayNextQueued();
                                break;
                            }
                        }
                        else
                        {
                            TrackRestart:
                            cli->paused = true;
                            ma_sound_seek_to_pcm_frame(&cli->music, 0);
                        }
                    }
                    cli->prevFrame = curFrame;
                }

                std::u32string displayStr = cli->str;
                if (cli->playing)
                {
                    constexpr auto sdFloat = [](float val, std::streamsize prec = 1) -> std::u32string
                    {
                        std::wostringstream ss;
                        ss << std::fixed << std::setprecision(prec) << val;
                        std::wstring wret = std::move(ss).str();
                        std::u32string ret; ret.reserve(wret.size());
                        for (auto c : wret)
                            ret.push_back((char32_t)c);
                        return ret;
                    };

                    std::u32string playBarBeg = U"Now Playing: ";
                    playBarBeg
                    .append(cli->paused ? U"# " : U"> ")
                    .append(cli->musicName)
                    .append(U"  ")
                    .append(sdFloat((float)ma_sound_get_time_in_pcm_frames(&cli->music) / (float)ma_engine_get_sample_rate(&TacradCLI::engine)))
                    .append(U" / ")
                    .append(sdFloat(cli->musicLen))
                    .append(U" (");
                    if (int32_t mins = (int32_t)cli->musicLen / 60; mins > 0)
                    {
                        std::wstring wminsStr = std::to_wstring(mins);
                        std::u32string minsStr; minsStr.reserve(wminsStr.size());
                        for (auto c : wminsStr)
                            minsStr.push_back((char32_t)c);
                        playBarBeg
                        .append(minsStr)
                        .append(U"min ");
                    }
                    std::wstring wsecsStr = std::to_wstring(int32_t(cli->musicLen + 0.5f) % 60);
                    std::u32string secsStr; secsStr.reserve(wsecsStr.size());
                    for (auto c : wsecsStr)
                        secsStr.push_back((char32_t)c);
                    playBarBeg
                    .append(secsStr)
                    .append(U"s)  ")
                    .append(sdFloat(ma_engine_get_volume(&TacradCLI::engine), 2))
                    .append(U"v |");
                    std::u32string playBarEnd = U"|";

                    ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&cli->music);

                    int64_t barLen = (int64_t)(cli->width()) - (int64_t)playBarBeg.size() - (int64_t)playBarEnd.size();
                    int64_t played = barLen * (int64_t)curFrame / (int64_t)cli->frameLen;
                    std::u32string out;
                    out
                    .append(playBarBeg)
                    .append(std::u32string(std::max((int64_t)0, played), '='))
                    .append(U"+")
                    .append(std::u32string(std::max((int64_t)0, barLen - played - 1), '-'))
                    .append(playBarEnd);

                    if (out.size() - 17 > cli->width() && out.size() - 17 > 3)
                    {
                        for (size_t i = out.size() - 17 - cli->width() + 3 + 3; i--;)
                            out.pop_back();
                        out.append(U"...");
                    }

                    size_t workingLineStart = displayStr.find_last_of(U'\n');
                    std::u32string workingLine;
                    if (workingLineStart != std::u32string::npos)
                    {
                        workingLine = displayStr.substr(workingLineStart + 1);
                        displayStr.erase(workingLineStart + 1);
                        displayStr.append(out);
                        displayStr.push_back(U'\n');
                        displayStr.append(workingLine);
                    }
                }
                if (cli->showCursorThisFrame)
                    displayStr.push_back(cursorCharacter);
                if (cli->nextTimePoint > 0.64f)
                {
                    cli->nextTimePoint -= 0.64f;
                    cli->showCursorThisFrame = !cli->showCursorThisFrame;
                }
                cli->_text->text = std::move(displayStr);
                
                if (cli->seekIfOutOfFrame)
                {
                    if (cli->_text->rectTransform()->position().y + cli->_text->rectTransform()->rect().bottom > (float)Window::pixelHeight() / 2.0f - 16.0f)
                    {
                        cli->scrollTop = -(cli->_text->rectTransform()->rect().top - cli->_text->rectTransform()->rect().bottom - 16.0f) / 16.0f / 3.0f;
                        cli->_text->rectTransform()->rect = RectFloat
                        (
                            0, cli->rectTransform()->rect().right,
                            -cli->_text->calculateBestFitHeight(), cli->rectTransform()->rect().left
                        );
                    }
                    else
                    {
                        cli->_text->rectTransform()->rect = RectFloat
                        (
                            0, cli->rectTransform()->rect().right,
                            -cli->_text->calculateBestFitHeight(), cli->rectTransform()->rect().left
                        );
                        if (cli->_text->rectTransform()->position().y + cli->_text->rectTransform()->rect().bottom < -Window::pixelHeight() / 2)
                            cli->scrollTop = (Window::pixelHeight() - (cli->_text->rectTransform()->rect().top - cli->_text->rectTransform()->rect().bottom)) / 16.0f / 3.0f;
                    }
                    cli->rectTransform()->position = Vector2(0.0f, Window::pixelHeight() / 2 - cli->scrollTop * 16.0f * 3.0f);

                    cli->seekIfOutOfFrame = false;
                }

                cli->nextTimePoint += Time::deltaTime();
            });
        };
        EngineEvent::OnWindowResize += [](Vector2Int prev)
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
            {
                cli->rectTransform()->position = Vector2(0.0f, Window::pixelHeight() / 2 - cli->scrollTop * 16.0f * 3.0f);
                cli->rectTransform()->rect = RectFloat
                (
                    0, Window::pixelWidth() / 2,
                    -Window::pixelHeight(), -Window::pixelWidth() / 2
                );
                cli->_text->rectTransform()->rect = RectFloat
                (
                    0, cli->rectTransform()->rect().right,
                    -cli->_text->calculateBestFitHeight(), cli->rectTransform()->rect().left
                );
            });
        };
        EngineEvent::OnMouseScroll += [](Vector2 scroll)
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
            {
                cli->scrollTop = std::min(0.0f, cli->scrollTop + scroll.y);
                cli->rectTransform()->position = Vector2(0.0f, Window::pixelHeight() / 2 - cli->scrollTop * 16.0f * 3.0f);
                cli->_text->rectTransform()->position = Vector2(0.0f, Window::pixelHeight() / 2 - cli->scrollTop * 16.0f * 3.0f);

                cli->nextTimePoint = 0.0f;
                cli->showCursorThisFrame = true;
            });
        };
    }
} init;
