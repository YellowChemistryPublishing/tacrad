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

namespace fs = std::filesystem;

// theft https://stackoverflow.com/questions/48896142/is-it-possible-to-get-hash-values-as-compile-time-constants
constexpr uint64_t hashString(std::string_view toHash)
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
}
constexpr std::vector<std::string> split(std::string_view str)
{
    std::vector<std::string> ret;
    size_t beg = 0, end = str.find_first_of(' ', beg);
    while (end != std::string_view::npos)
    {
        if (str.begin() + beg != str.begin() + end)
            ret.push_back(std::string(str.begin() + beg, str.begin() + end));

        beg = end + 1;
        end = str.find_first_of(' ', beg);
    }
    if (str.begin() + beg != str.end()) [[likely]]
        ret.push_back(std::string(str.begin() + beg, str.end()));
    return std::move(ret);
}

std::string sdFloat(float val, std::streamsize prec = 1)
{
    std::ostringstream ret;
    ret.precision(prec);
    ret << std::fixed << val;
    return std::move(ret).str();
}

moodycamel::ConcurrentQueue<std::string> cmdQueue;
void asyncRead()
{
    while (true)
    {
        std::string read;
        std::getline(std::cin, read);
        cmdQueue.enqueue(read);
        if (read == "exit")
            break;
    }
}

std::string toLower(std::string_view str)
{
    std::string ret; ret.append(str);
    std::transform(ret.begin(), ret.end(), ret.begin(), [](char c){ return std::tolower(c); });
    return ret;
}

std::random_device seeder;
std::mt19937 randEngine(seeder());
std::uniform_real_distribution<float> dist(0.0f, 1.0f);

std::string musicLookup(std::string_view name, fs::path& file)
{
    std::string compare; compare.append(name);
    for (auto& c : compare)
        c = std::tolower(c);
    
    if (fs::exists("music/"))
    {
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.is_regular_file())
            {
                if (toLower(dir.path().stem().string()) == compare)
                {
                    file = dir.path();
                    return dir.path().stem().string();
                }
            }
        }
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.is_regular_file())
            {
                if (toLower(dir.path().stem().string()).starts_with(compare))
                {
                    file = dir.path();
                    return dir.path().stem().string();
                }
            }
        }
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.is_regular_file())
            {
                if (toLower(dir.path().stem().string()).contains(compare))
                {
                    file = dir.path();
                    return dir.path().stem().string();
                }
            }
        }
    }

    file = "err.";
    return "Error! (This will end poorly later.)";
}

int main()
{
    #if _WIN32
    HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode;
    GetConsoleMode(outHandle, &dwMode);
    dwMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(outHandle, dwMode);
    #endif
    
    ma_engine engine;
    if (ma_result code = ma_engine_init(nullptr, &engine); code != MA_SUCCESS) [[unlikely]]
    {
        std::cerr << "Audio engine failed to initialize with code " << code << ".\n";
        return EXIT_FAILURE;
    }

    std::cout << "[tacrad] Welcome to the tacrad CLI! (Enter \"help\" for details.)\n[tacrad] ";
    std::thread readThread(asyncRead);

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
    std::list<std::pair<std::string, fs::path>> queue;
    decltype(queue)::iterator queuePos = queue.end();

    ma_uint64 prevFrame = 0;
    ma_uint64 frameLen;
    float musicLen;
    std::string musicName;

    std::string read;
    std::chrono::high_resolution_clock::time_point nextDraw = std::chrono::high_resolution_clock::now();
    short oldColumns = 0;

    auto startMusic = [&](std::string_view query)
    {
        fs::path musicFile;
        std::string _musicName = musicLookup(query, musicFile);
        if (ma_sound_init_from_file(&engine, musicFile.string().c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
        {
            ma_sound_start(&music);
            ma_sound_get_length_in_pcm_frames(&music, &frameLen);
            ma_sound_get_length_in_seconds(&music, &musicLen);
            musicName = _musicName;
            playing = true;
        }
        else std::cerr << "[tacrad] [log.error] Music query doesn't exist!\n";
    };
    auto tryPlayNextAlphabetical = [&](std::string_view prev, bool wasPaused) -> void
    {
        fs::path file;
        std::string name(1, 'z');
        if (prev.empty() && fs::exists("music/"))
        {
            for (auto it : fs::recursive_directory_iterator("music/"))
            {
                if (it.path().stem().string() < name)
                {
                    file = it.path();
                    name = file.stem().string();
                }
            }
        }
        else
        {
            std::vector<std::pair<std::string, fs::path>> tracks;
            if (fs::exists("music/"))
                for (auto it : fs::recursive_directory_iterator("music/"))
                    if (it.is_regular_file())
                        tracks.push_back(std::make_pair(it.path().stem().string(), it.path()));
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
                std::cerr << "[tacrad] [log.info] End of playlist!\n";
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
        else std::cerr << "[tacrad] [log.error] Music query doesn't exist! [dev] name: " << name << ", file: " << file << '\n';
    };
    auto stopMusic = [&]
    {
        ma_sound_stop(&music);
        ma_sound_uninit(&music);
        playing = false;
        paused = false;
    };
    auto tryPlayNextShuffle = [&](bool wasPaused = false) -> void
    {
        size_t ct = 0;
        if (fs::exists("music/"))
            for (auto& dir : fs::recursive_directory_iterator("music/"))
                if (dir.path().stem().string() != musicName)
                    ++ct;
        if (ct != 0)
        {
            More:
            for (auto& dir : fs::recursive_directory_iterator("music/"))
            {
                if (dir.is_regular_file() && dir.path().stem().string() != musicName && dist(randEngine) < 1.0f / (float)ct)
                {
                    fs::path musicFile;
                    std::string _musicName = musicLookup(dir.path().stem().string(), musicFile);
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
                    else std::cerr << "[tacrad] [log.error] Failed to load next track, shuffling for new one.\n";
                }
            }
            goto More;
        }
    };
    auto incrQueuePos = [&]
    {
        if (queuePos == queue.end())
            queuePos = queue.begin();
        else if (++auto(queuePos) == queue.end())
        {
            if (!loop)
            {
                std::cerr << "[tacrad] [log.info] End of playlist!\n";
                return;
            }
            else queuePos = queue.begin();
        }
        else ++queuePos;
    };
    auto tryPlayNextQueued = [&](bool wasPaused = false)
    {
        if (queue.empty())
        {
            std::cerr << "[tacrad] [log.warn] Empty playlist!\n";
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
            std::cerr << "[tacrad] [log.error] Couldn't load next music track in queue, skipping! [dev] name: " << queuePos->first << ", path: " << queuePos->second << '\n';
            goto Retry;
        }
    };

    while (true)
    {
        #if _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi; SHORT columns;
        if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
            columns = 128;
        else columns = csbi.srWindow.Right - csbi.srWindow.Left;
        if (columns < oldColumns)
        {
            if (playing)
                std::cout << "\r\33[1A\r\n\33[2K[tacrad] ";
        }
        else if (columns > oldColumns)
        {
            if (playing)
                std::cout << "\r\33[2K[tacrad] ";
        }
        oldColumns = columns;
        #else
        constexpr int16_t columns = 128;
        #endif
    
        if (cmdQueue.try_dequeue(read))
        {
            std::vector<std::string> cmd = split(read);

            if (playing)
                std::cout << "\33[2A\33[J";

            if (!cmd.empty())
            {
                switch (hashString(cmd[0]))
                {
                case hashString("help"):
                    std::cout <<
R"(Help Text
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
)";
                    break;
                case hashString("p"):
                    if (cmd.size() == 1 && playing)
                    {
                        if (paused)
                            goto MusicResume;
                        else goto MusicPause;
                    }
                    goto MusicPlay;
                case hashString(">"):
                    if (cmd.size() == 1 && paused)
                        goto MusicResume;
                    [[fallthrough]];
                case hashString("play"):
                    {
                        MusicPlay:
                        if (cmd.size() < 2) [[unlikely]]
                        {
                            std::cerr << "[tacrad] [log.error] Track title argument must be given to \"play\"!\n";
                            break;
                        }

                        if (playing)
                            stopMusic();
                    
                        std::string lookupName = cmd[1];
                        for (auto& word : std::span(++++cmd.begin(), cmd.end()))
                        {
                            lookupName.push_back(' ');
                            lookupName.append(word);
                        }
                        startMusic(lookupName);
                    }
                    break;
                case hashString("resume"):
                case hashString("res"):
                    if (cmd.size() > 1) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] \"resume\" takes no arguments!\n";
                        break;
                    }

                    if (playing)
                    {
                        MusicResume:
                        ma_sound_start(&music);
                        paused = false;
                    }
                    else std::cerr << "[tacrad] [log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n";
                    break;
                case hashString("pause"):
                case hashString("ps"):
                case hashString("#"):
                    if (cmd.size() > 1) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] \"pause\" takes no arguments!\n";
                        break;
                    }

                    if (playing)
                    {
                        MusicPause:
                        ma_sound_stop(&music);
                        paused = true;
                    }
                    else std::cerr << "[tacrad] [log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n";
                    break;
                case hashString("seek"):
                    if (cmd.size() < 2) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] Seek position argument (in seconds) must be given to \"seek\"!\n";
                        break;
                    }
                    if (cmd.size() > 2) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] Extra arguments given to \"seek\"!\n";
                        break;
                    }

                    if (playing)
                    {
                        double seekQuery = (double)ma_engine_get_sample_rate(&engine) * std::atof(cmd[1].c_str());
                        if (seekQuery >= 0.0 && seekQuery <= frameLen)
                        {
                            ma_sound_seek_to_pcm_frame(&music, (ma_uint64)seekQuery);
                        }
                        else std::cerr << "[tacrad] [log.error] Seek query out of duration of media!\n";
                    }
                    else std::cerr << "[tacrad] [log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n";
                    break;
                case hashString("volume"):
                case hashString("vol"):
                    if (cmd.size() < 2) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] Volume argument (linear) must be given to \"vol\"!\n";
                        break;
                    }
                    if (cmd.size() > 2) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] Extra arguments given to \"vol\"!\n";
                        break;
                    }

                    ma_engine_set_volume(&engine, std::atof(cmd[1].c_str()));
                    break;
                case hashString("stop"):
                    if (playing)
                        stopMusic();
                    else std::cerr << "[tacrad] [log.error] Not currently playing music! Use \"play\" to start media.\n";
                    break;
                case hashString("next"):
                case hashString("n"):
                case hashString(">>"):
                    goto PlaylistNext;
                case hashString("playlist"):
                case hashString("playl"):
                case hashString("pl"):
                    if (cmd.size() < 2) [[unlikely]]
                    {
                        std::cerr << "[tacrad] [log.error] \"playl\" Takes at least a flag argument!\n";
                        break;
                    }

                    switch (hashString(cmd[1]))
                    {
                    case hashString("--next"):
                    case hashString("-n"):
                        {
                            if (cmd.size() > 2)
                                std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
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
                    case hashString("--shuffle"):
                    case hashString("-sh"):
                        if (cmd.size() > 2)
                        {
                            std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                            break;
                        }
                        type = PlaylistType::Shuffle;
                        break;
                    case hashString("--Sequential"):
                    case hashString("--seq"):
                    case hashString("-sq"):
                        if (cmd.size() > 2)
                        {
                            std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                            break;
                        }
                        type = PlaylistType::Sequential;
                        break;
                    case hashString("--queued"):
                    case hashString("-q"):
                        if (cmd.size() > 2)
                        {
                            std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                            break;
                        }
                        type = PlaylistType::Queued;
                        break;
                    case hashString("--push"):
                    case hashString("-p"):
                        {
                            if (cmd.size() < 3)
                            {
                                std::cerr << "[tacrad] [log.error] \"playl --push\" requires at least a one-word music track query!\n";
                                break;
                            }
                            std::string lookupName = cmd[2];
                            for (auto& word : std::span(++++++cmd.begin(), cmd.end()))
                            {
                                lookupName.push_back(' ');
                                lookupName.append(word);
                            }
                            fs::path path;
                            std::string name = musicLookup(lookupName, path);
                            queue.push_back(std::make_pair(name, std::move(path)));
                            std::cout << "[tacrad] [log.info] Adding \"" << name << "\" to playlist music queue.\n";
                        }
                        break;
                    case hashString("--list"):
                    case hashString("-l"):
                        {
                            if (cmd.size() > 2)
                            {
                                std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                                break;
                            }

                            if (!queue.empty())
                            {
                                std::string playlist;
                                size_t i = 1;
                                for (auto it = queue.begin(); it != queue.end(); ++it)
                                {
                                    playlist.append(std::to_string(i++)).append(". ").append(it->first);
                                    if (it == queuePos)
                                        playlist.append(" < You Are Here");
                                    playlist.push_back('\n');
                                }
                                std::cout << "[tacrad] [log.info]\n" << playlist;
                            }
                            else std::cout << "[tacrad] [log.info] <Empty Playlist>\n";
                        }
                        break;
                    case hashString("--index"):
                    case hashString("-i"):
                        {
                            if (cmd.size() > 3)
                            {
                                std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                                break;
                            }
                            if (cmd.size() < 3)
                            {
                                std::cerr << "[tacrad] [log.error] \"playl --index\" requires an index argument!\n";
                                break;
                            }

                            size_t pos;
                            int diff = std::stoi(cmd[2], &pos);
                            if (pos != cmd[2].size() || pos < 1)
                            {
                                std::cerr << "[tacrad] [log.error] Invalid index argument given to \"playl --index\"!\n";
                                break;
                            }
                            if (pos > queue.size())
                            {
                                std::cerr << "[tacrad] [log.error] Index argument given to \"playl --index\" out of range!\n";
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
                    case hashString("--loop"):
                    case hashString("-lp"):
                        switch (cmd.size())
                        {
                        case 2:
                            loop = !loop;
                            goto LoopSet;
                        case 3:
                            switch (hashString(cmd[2]))
                            {
                            case hashString("true"):
                            case hashString("t"):
                            case hashString("1"):
                                loop = true;
                                goto LoopSet;
                            case hashString("false"):
                            case hashString("f"):
                            case hashString("0"):
                                loop = false;
                                goto LoopSet;
                            default:
                                std::cerr << "[tacrad] [log.error] Unknown boolean argument given to \"playl\"!\n";
                            }
                            break;
                        LoopSet:
                            std::cout << "[tacrad] [log.info] Playlist queue looping set to " << (loop ? "true" : "false") << "!\n";
                            break;
                        default:
                            std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                        }
                        break;
                    case hashString("--clear"):
                    case hashString("-c"):
                        if (cmd.size() > 2)
                        {
                            std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                            break;
                        }
                        queue.clear();
                        queuePos = queue.end();
                        std::cout << "[tacrad] [log.info] Clearing playlist queue.\n";
                        break;
                    default:
                        std::cerr << "[tacrad] [log.warn] Unknown flag argument given to \"playl\".\n";
                    }
                    break;
                case hashString("exit"):
                    if (playing)
                        stopMusic();
                    readThread.join();
                    goto Break;
                default:
                    for (char c : cmd[0])
                    {
                        if (!std::isspace(c))
                        {
                            std::cerr << "[tacrad] [log.error] Unknown command! Enter \"help\" to view a list of available commands!\n";
                            break;
                        }
                    }
                }
            }

            if (playing)
                std::cout << '\n';
            std::cout << "[tacrad] ";
        }
        if (playing && std::chrono::high_resolution_clock::now() >= nextDraw)
        {
            std::string playBarBeg = "Now Playing: ";
            playBarBeg
            .append(paused ? "# " : "> ")
            .append(musicName)
            .append("  ")
            .append(sdFloat((float)ma_sound_get_time_in_pcm_frames(&music) / (float)ma_engine_get_sample_rate(&engine)))
            .append(" / ")
            .append(sdFloat(musicLen))
            .append(" (");
            if (int32_t mins = (int32_t)musicLen / 60; mins > 0)
            {
                playBarBeg
                .append(std::to_string(mins))
                .append("min ");
            }
            playBarBeg
            .append(std::to_string(int32_t(musicLen + 0.5f) % 60))
            .append("s)  ")
            .append(sdFloat(ma_engine_get_volume(&engine), 2))
            .append("v |");
            std::string playBarEnd = "|";

            ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&music);

            int64_t barLen = (int64_t)columns - (int64_t)playBarBeg.size() - (int64_t)playBarEnd.size();
            int64_t played = barLen * (int64_t)curFrame / (int64_t)frameLen;
            std::string out = "\33[s\r\33[1A\r\33[2K";
            out
            .append(playBarBeg)
            .append(std::string(std::max((int64_t)0, played), '='))
            .append("+")
            .append(std::string(std::max((int64_t)0, barLen - played - 1), '-'))
            .append(playBarEnd)
            .append("\33[u");

            if (out.size() - 17 > columns && out.size() - 17 > 3)
            {
                for (size_t i = out.size() - 17 - columns + 3 + 3; i--;)
                    out.pop_back();
                out.append("...\33[u");
            }

            std::cout << out;
            nextDraw += std::chrono::milliseconds(100);
        }
        ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&music);
        if (playing && curFrame != prevFrame)
        {
            if (curFrame >= frameLen)
            {
                if (fs::exists("music/"))
                {
                    switch (type)
                    {
                    case PlaylistType::Sequential:
                        goto TrackRestart;
                    case PlaylistType::Shuffle:
                        stopMusic();
                        tryPlayNextShuffle();
                        break;
                    case PlaylistType::Queued:
                        stopMusic();
                        incrQueuePos();
                        tryPlayNextQueued();
                        break;
                    }
                }
                else
                {
                    TrackRestart:
                    paused = true;
                    ma_sound_seek_to_pcm_frame(&music, 0);
                }
            }
            prevFrame = curFrame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Break:

    return EXIT_SUCCESS;
}