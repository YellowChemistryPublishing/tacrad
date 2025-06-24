#pragma once

#include <miniaudio.h>
#include <algorithm>
#include <filesystem>
#include <list>
#include <random>
#include <vector>

namespace fs = std::filesystem;

enum class PlaylistType
{
    Sequential,
    Shuffle,
    Queued
};

struct MusicPlayer
{
    MusicPlayer() = delete;
    
    inline static std::random_device seeder;
    inline static std::mt19937 randEngine { seeder() };
    inline static std::uniform_real_distribution<float> dist { 0.0f, 1.0f };

    inline static ma_engine engine;
    inline static ma_sound music;
    
    inline static bool playing = false;
    inline static bool paused = false;
    inline static bool loop = false;
    inline static PlaylistType type = PlaylistType::Sequential;
    
    inline static ma_uint64 prevFrame = 0;
    inline static ma_uint64 frameLen;
    inline static float musicLen;
    inline static std::u32string musicName;

    inline static std::list<std::pair<std::u32string, fs::path>> queue;
    inline static decltype(queue)::iterator queuePos = queue.end();
    
    inline static std::u32string toLower(std::u32string_view str)
    {
        std::u32string ret(str.begin(), str.end());
        std::transform(ret.begin(), ret.end(), ret.begin(), [](char32_t c){ return (char32_t)std::tolower((int)c); });
        return ret;
    }
    static std::u32string musicLookup(std::u32string_view name, fs::path& file);

    static void startMusic(std::u32string_view query);
    static void tryPlayNextAlphabetical(std::u32string_view prev, bool wasPaused);
    static void stopMusic();
    static void next();
    static void tryPlayNextShuffle(bool wasPaused = false);
    static void incrQueuePos();
    static void tryPlayNextQueued(bool wasPaused = false);

    static void musicResume();
    static void musicPause();
};