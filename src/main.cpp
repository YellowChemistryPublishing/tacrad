#include <algorithm>
#include <concurrentqueue.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <span>
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
        ret.push_back(std::string(str.begin() + beg, str.begin() + end));

        beg = end + 1;
        end = str.find_first_of(' ', beg);
    }
    ret.push_back(std::string(str.begin() + beg, str.end()));
    return std::move(ret);
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
                if (toLower(dir.path().root_name().string()) == compare)
                {
                    file = dir.path();
                    return dir.path().string();
                }
            }
        }
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.is_regular_file())
            {
                if (toLower(dir.path().filename().string()).starts_with(compare))
                {
                    file = dir.path();
                    return dir.path().string();
                }
            }
        }
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.is_regular_file())
            {
                if (toLower(dir.path().filename().string()).contains(compare))
                {
                    file = dir.path();
                    return dir.path().string();
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

    srand(time(nullptr));

    ma_sound music;

    bool playing = false;
    bool paused = false;
    bool shuffle = false;

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
        if (ma_sound_init_from_file(&engine, musicLookup(query, musicFile).c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
        {
            ma_sound_start(&music);
            ma_sound_get_length_in_pcm_frames(&music, &frameLen);
            ma_sound_get_length_in_seconds(&music, &musicLen);
            musicName = musicFile.stem().string();
            playing = true;
        }
        else std::cerr << "[tacrad] [log.error] Music query doesn't exist!\n";
    };
    auto stopMusic = [&]
    {
        ma_sound_stop(&music);
        ma_sound_uninit(&music);
        playing = false;
        paused = false;
    };
    auto tryPlayNextShuffle = [&]
    {
        size_t ct = 0;
        for (auto& dir : fs::recursive_directory_iterator("music/"))
            if (dir.path().stem().string() != musicName)
                ++ct;
        size_t i = 0; int val = rand() % ct;
        for (auto& dir : fs::recursive_directory_iterator("music/"))
        {
            if (dir.path().stem().string() != musicName && i++ == val)
            {
                fs::path musicFile;
                if (ma_sound_init_from_file(&engine, musicLookup(dir.path().stem().string(), musicFile).c_str(), 0, nullptr, nullptr, &music) == MA_SUCCESS)
                {
                    ma_sound_start(&music);
                    ma_sound_get_length_in_pcm_frames(&music, &frameLen);
                    ma_sound_get_length_in_seconds(&music, &musicLen);
                    musicName = musicFile.stem().string();
                    playing = true;
                }

                break;
            }
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

            switch (hashString(cmd[0]))
            {
            case hashString("clear"):
                break;
            case hashString("play"):
            case hashString("p"):
                if (cmd.size() < 2) [[unlikely]]
                {
                    std::cerr << "[tacrad] [log.error] Track title argument must be given to \"play\"!\n";
                    break;
                }

                if (!playing) [[likely]]
                {
                    std::string lookupName = cmd[1];
                    for (auto& word : std::span(++++cmd.begin(), cmd.end()))
                    {
                        lookupName.push_back(' ');
                        lookupName.append(word);
                    }
                    startMusic(lookupName);
                }
                else std::cerr << "[tacrad] [log.error] Already playing! Use \"pause\" and \"resume\" to control active media.\n";
                break;
            case hashString("resume"):
                if (playing)
                {
                    ma_sound_start(&music);
                    paused = false;
                }
                else std::cerr << "[tacrad] [log.error] Not currently playing music! Use \"play\" and \"stop\" to change media.\n";
                break;
            case hashString("pause"):
                if (playing)
                {
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
                goto PlaylistNext;
            case hashString("playlist"):
            case hashString("playl"):
            case hashString("pl"):
                if (cmd.size() < 2) [[unlikely]]
                {
                    std::cerr << "[tacrad] [log.error] Playlist flag argument must be given to \"playl\"!\n";
                    break;
                }
                if (cmd.size() > 2) [[unlikely]]
                {
                    std::cerr << "[tacrad] [log.error] Extra arguments given to \"playl\"!\n";
                    break;
                }

                switch (hashString(cmd[1]))
                {
                case hashString("--next"):
                case hashString("-n"):
                    PlaylistNext:
                    if (shuffle)
                    {
                        if (playing)
                            stopMusic();
                        tryPlayNextShuffle();
                    }
                    else std::cerr << "[tacrad] [log.warn] unimplemented\n";
                    break;
                case hashString("--shuffle"):
                case hashString("-sh"):
                    shuffle = true;
                    break;
                case hashString("--seqential"):
                case hashString("--seq"):
                case hashString("-sq"):
                    std::cerr << "[tacrad] [log.warn] unimplemented\n";
                    break;
                case hashString("--clear"):
                case hashString("-c"):
                    shuffle = false;
                    break;
                default:
                    std::cerr << "[tacrad] [log.warn] Unknown flag argument given to \"playl\".\n";
                }
                break;
            case hashString("exit"):
                readThread.join();
                goto Break;
            default:
                std::cerr << "[tacrad] [log.error] Unknown command! Enter \"help\" to view a list of available commands!\n";
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
            .append(std::to_string((float)ma_sound_get_time_in_pcm_frames(&music) / (float)ma_engine_get_sample_rate(&engine)))
            .append(" / ")
            .append(std::to_string(musicLen))
            .append("  ")
            .append(std::to_string(ma_engine_get_volume(&engine)))
            .append("v |");
            std::string playBarEnd = "|";

            ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&music);

            int64_t barLen = (int64_t)columns - (int64_t)playBarBeg.size() - (int64_t)playBarEnd.size();
            int64_t played = barLen * (int64_t)curFrame / (int64_t)frameLen;
            std::string out = "\33[s\r\33[1A\r\33[2K";
            out
            .append(playBarBeg)
            .append(std::string(std::max((int64_t)0, played - 1), '='))
            .append("+")
            .append(std::string(std::max((int64_t)0, barLen - played), '-'))
            .append(playBarEnd)
            .append("\33[u");

            if (out.size() - 17 > columns && out.size() - 17 > 3)
            {
                for (size_t i = out.size() - 17 - columns + 3 + 3; i--;)
                    out.pop_back();
                out.append("...\33[u");
            }

            std::cout << out;
            nextDraw += std::chrono::milliseconds(17);
        }
        ma_uint64 curFrame = ma_sound_get_time_in_pcm_frames(&music);
        if (playing && curFrame != prevFrame)
        {
            if (curFrame >= frameLen)
            {
                if (shuffle && fs::exists("music/"))
                {
                    stopMusic();
                    tryPlayNextShuffle();
                }
                else
                {
                    paused = true;
                    ma_sound_seek_to_pcm_frame(&music, 0);
                }
            }
            prevFrame = curFrame;
        }
    }
    Break:

    return EXIT_SUCCESS;
}