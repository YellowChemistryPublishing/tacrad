#pragma once

#include <Core/CoreEngine.h>
#include <Components/Text.h>
#include <Core/PackageManager.h>
#include <EntityComponentSystem/ComponentData.h>

#include <InteractableProgressBar.h>
#include <MusicPlayer.h>
#include <NanoVGContext.h>

#define RUNNING_TIME_TEXT_PADDING_Y 4.0f
#define RUNNING_TIME_SPACING_AROUND_DIVIDER 10.0f
#define RUNNING_TIME_DIVIDER_RATIO 0.3f

using namespace Firework;
using namespace Firework::Internal;
using namespace Firework::PackageSystem;

class RunningTime : public ComponentData2D
{
    inline void renderOffload()
    {
        // CoreEngine::queueRenderJobForFrame([w = Window::pixelWidth(), h = Window::pixelHeight(), bounds = NanoVG::boundsFromRectTransform(this->rectTransform())]
        // {
        //     nvgBeginPath(NanoVG::context);
        //     nvgRect(NanoVG::context, bounds.x, bounds.y, bounds.width, bounds.height);
        //     nvgFillColor(NanoVG::context, nvgRGBA(0x4c, 0x4c, 0x4c, 0x88));
        //     nvgFill(NanoVG::context);
        //     nvgClosePath(NanoVG::context);
        // }, false);
    }

    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnTick += []
            {
                EntityManager2D::foreachEntityWithAll<RunningTime>([](Entity2D* entity, RunningTime* runningTime)
                {
                    if (MusicPlayer::playing)
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

                        runningTime->runtime->text = sdFloat((float)ma_sound_get_time_in_pcm_frames(&MusicPlayer::music) / (float)ma_engine_get_sample_rate(&MusicPlayer::engine));
                        runningTime->track->progress = (float)ma_sound_get_time_in_pcm_frames(&MusicPlayer::music) / (float)ma_engine_get_sample_rate(&MusicPlayer::engine) / MusicPlayer::musicLen;

                        std::u32string totalText;
                        totalText
                        .append(sdFloat(MusicPlayer::musicLen))
                        .append(U" (");
                        if (int32_t mins = (int32_t)MusicPlayer::musicLen / 60; mins > 0)
                        {
                            std::wstring wminsStr = std::to_wstring(mins);
                            std::u32string minsStr; minsStr.reserve(wminsStr.size());
                            for (auto c : wminsStr)
                                minsStr.push_back((char32_t)c);
                            totalText
                            .append(minsStr)
                            .append(U"min ");
                        }
                        std::wstring wsecsStr = std::to_wstring(int32_t(MusicPlayer::musicLen + 0.5f) % 60);
                        std::u32string secsStr; secsStr.reserve(wsecsStr.size());
                        for (auto c : wsecsStr)
                            secsStr.push_back((char32_t)c);
                        totalText
                        .append(secsStr)
                        .append(U"s)");

                        runningTime->total->text = std::move(totalText);
                    }
                });
            };

            InternalEngineEvent::OnRenderOffloadForComponent2D += [](Component2D* component)
            {
                switch (component->typeIndex())
                {
                case __typeid(RunningTime).qualifiedNameHash():
                    static_cast<RunningTime*>(component)->renderOffload();
                    break;
                }
            };
        }
    } system;
public:
    InteractableProgressBar* track = nullptr;
    Text* runtime;
    Text* slash;
    Text* total;

    inline void onCreate()
    {
        TrueTypeFontPackageFile* font = file_cast<TrueTypeFontPackageFile>(PackageManager::lookupFileByPath(L"assets/Red_Hat_Display/static/RedHatDisplay-SemiBold.ttf"));

        Entity2D* runtime = new Entity2D();
        runtime->parent = this->entity();
        runtime->name = L"Runtime Text";
        runtime->rectTransform()->localPosition = sysm::vector2(this->rectTransform()->rect().left + this->rectTransform()->rect().width() * RUNNING_TIME_DIVIDER_RATIO - RUNNING_TIME_SPACING_AROUND_DIVIDER, 0);
        runtime->rectTransform()->rect = RectFloat
        (
            this->rectTransform()->rect().top - RUNNING_TIME_TEXT_PADDING_Y, 0.0f,
            this->rectTransform()->rect().bottom + RUNNING_TIME_TEXT_PADDING_Y, -this->rectTransform()->rect().width()
        );
        this->runtime = runtime->addComponent<Text>();
        this->runtime->fontFile = font;
        this->runtime->fontSize = runtime->rectTransform()->rect().height();
        this->runtime->horizontalAlign = TextAlign::Major;
        this->runtime->active = false;

        Entity2D* slash = new Entity2D();
        slash->parent = this->entity();
        slash->name = L"Slash Text";
        slash->rectTransform()->localPosition = sysm::vector2(this->rectTransform()->rect().left + this->rectTransform()->rect().width() * RUNNING_TIME_DIVIDER_RATIO, 0);
        slash->rectTransform()->rect = RectFloat
        (
            this->rectTransform()->rect().top - RUNNING_TIME_TEXT_PADDING_Y, RUNNING_TIME_SPACING_AROUND_DIVIDER,
            this->rectTransform()->rect().bottom + RUNNING_TIME_TEXT_PADDING_Y, -RUNNING_TIME_SPACING_AROUND_DIVIDER
        );
        this->slash = slash->addComponent<Text>();
        this->slash->text = U"/";
        this->slash->fontFile = font;
        this->slash->fontSize = slash->rectTransform()->rect().height();
        this->slash->horizontalAlign = TextAlign::Center;
        this->slash->active = false;

        Entity2D* total = new Entity2D();
        total->parent = this->entity();
        total->name = L"Total Text";
        total->rectTransform()->localPosition = sysm::vector2(this->rectTransform()->rect().left + this->rectTransform()->rect().width() * RUNNING_TIME_DIVIDER_RATIO + RUNNING_TIME_SPACING_AROUND_DIVIDER, 0);
        total->rectTransform()->rect = RectFloat
        (
            this->rectTransform()->rect().top - RUNNING_TIME_TEXT_PADDING_Y, this->rectTransform()->rect().width(),
            this->rectTransform()->rect().bottom + RUNNING_TIME_TEXT_PADDING_Y, 0.0f
        );
        this->total = total->addComponent<Text>();
        this->total->fontFile = font;
        this->total->fontSize = total->rectTransform()->rect().height();
        this->total->horizontalAlign = TextAlign::Minor;
        this->total->active = false;
    }
};