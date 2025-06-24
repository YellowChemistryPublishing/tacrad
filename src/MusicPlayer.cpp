#include "MusicPlayer.h"

#include <EntityComponentSystem/EntityManagement.h>

#include <TacradCLI.h>

using namespace Firework;

std::u32string MusicPlayer::musicLookup(std::u32string_view name, fs::path& file)
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

void MusicPlayer::musicResume()
{
    ma_sound_start(&music);
    paused = false;
}
void MusicPlayer::musicPause()
{
    ma_sound_stop(&music);
    paused = true;
}

void MusicPlayer::startMusic(std::u32string_view query)
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
    else EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
    {
        cli->writeLine(U"[log.error] Music query doesn't exist!\n");
    });
}
void MusicPlayer::tryPlayNextAlphabetical(std::u32string_view prev, bool wasPaused)
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
            EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
            {
                cli->writeLine(U"[log.info] No music to play. (Add some!)\n");
            });
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
            EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
            {
                cli->writeLine(U"[log.info] End of playlist!\n");
            });
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
    else EntityManager2D::foreachEntityWithAll<TacradCLI>([&](Entity2D* entity, TacradCLI* cli)
    {
        cli->writeLine(std::u32string(U"[log.error] Music query doesn't exist! [dev] name: ").append(name).append(U", file: ").append(file.u32string()).append(U"\n"));
    });
}
void MusicPlayer::stopMusic()
{
    ma_sound_stop(&music);
    ma_sound_uninit(&music);
    playing = false;
    paused = false;
}
void MusicPlayer::next()
{
    bool wasPaused = MusicPlayer::paused;
    if (MusicPlayer::playing)
        MusicPlayer::stopMusic();
    switch (MusicPlayer::type)
    {
    case PlaylistType::Sequential:
        MusicPlayer::tryPlayNextAlphabetical(MusicPlayer::musicName, wasPaused);
        break;
    case PlaylistType::Shuffle:
        MusicPlayer::tryPlayNextShuffle(wasPaused);
        break;
    case PlaylistType::Queued:
        {
            auto prev = MusicPlayer::queuePos;
            MusicPlayer::incrQueuePos();
            if (MusicPlayer::queuePos != prev)
                MusicPlayer::tryPlayNextQueued(wasPaused);
        }
        break;
    }
}
void MusicPlayer::tryPlayNextShuffle(bool wasPaused)
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
                else EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
                {
                    cli->writeLine(U"[log.error] Failed to load next track, shuffling for new one.\n");
                });
            }
        }
        goto More;
    }
    else EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
    {
        cli->writeLine(U"[log.info] No music to play. (Add some!)\n");
    });
}
void MusicPlayer::incrQueuePos()
{
    if (queue.empty())
    {
        EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
        {
            cli->writeLine(U"[log.warn] Empty playlist!\n");
        });
        return;
    }

    if (queuePos == queue.end())
        queuePos = queue.begin();
    else if (++decltype(queuePos)(queuePos) == queue.end())
    {
        if (!loop)
        {
            EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
            {
                cli->writeLine(U"[log.info] End of playlist!\n");
            });
            return;
        }
        else queuePos = queue.begin();
    }
    else ++queuePos;
}
void MusicPlayer::tryPlayNextQueued(bool wasPaused)
{
    decltype(queuePos) prev = queuePos;

    goto FirstTry;
    {
        Retry:
        incrQueuePos();

        if (queuePos == prev)
            return;
        else prev = queuePos;
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
        EntityManager2D::foreachEntityWithAll<TacradCLI>([](Entity2D* entity, TacradCLI* cli)
        {
            cli->writeLine(std::u32string(U"[log.error] Couldn't load next music track in queue, skipping! [dev] name: ").append(queuePos->first).append(U", path: ").append(queuePos->second.u32string()).append(U"\n"));
        });
        goto Retry;
    }
}
