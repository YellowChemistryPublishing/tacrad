#pragma once

#include <Core/CoreEngine.h>
#include <EntityComponentSystem/ComponentData.h>
#include <EntityComponentSystem/EngineEvent.h>

#include <InteractableProgressBar.h>
#include <MusicPlayer.h>
#include <NanoVGContext.h>
#include <RunningTime.h>

#define BUTTON_PADDING 5.0f
#define PAUSE_BUTTON_BAR_WIDTH 4.0f
#define PAUSE_BUTTON_GAP 4.0f
#define PAUSE_BUTTON_EXTRA_VERTICAL_PADDING 1.0f
#define STOP_BUTTON_EXTRA_PADDING 2.0f
#define NEXT_BUTTON_EXTRA_PADDING 1.0f
#define NEXT_BUTTON_STROKE_THICKNESS 2.5f

using namespace Firework;
using namespace Firework::Internal;

enum class TrackInteractionButtonType
{
    Play,
    Pause,
    Stop,
    Next
};

class TrackInteractionButton : public ComponentData2D
{
    inline static CursorTexture* cursorDefault;
    inline static CursorTexture* cursorHover;

    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnInitialize += []
            {
                TrackInteractionButton::cursorDefault = new CursorTexture(BuiltinCursorTexture::Default);
                TrackInteractionButton::cursorHover = new CursorTexture(BuiltinCursorTexture::Pointer);
            };
            EngineEvent::OnQuit += []
            {
                delete TrackInteractionButton::cursorDefault;
                delete TrackInteractionButton::cursorHover;
            };
            EngineEvent::OnMouseDown += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    bool setPlay = false;
                    bool setPause = false;
                    EntityManager2D::foreachEntityWithAll<TrackInteractionButton>([&](Entity2D* entity, TrackInteractionButton* button)
                    {
                        if (button->rectTransform()->queryPointIn(Input::mousePosition()))
                        {
                            switch (button->type)
                            {
                            case TrackInteractionButtonType::Play:
                                if (MusicPlayer::playing)
                                {
                                    MusicPlayer::musicResume();
                                    button->type = TrackInteractionButtonType::Pause;
                                }
                                break;
                            case TrackInteractionButtonType::Pause:
                                if (MusicPlayer::playing)
                                {
                                    MusicPlayer::musicPause();
                                    button->type = TrackInteractionButtonType::Play;
                                }
                                break;
                            case TrackInteractionButtonType::Stop:
                                if (MusicPlayer::playing)
                                {
                                    button->runningTime->runtime->active = false;
                                    button->runningTime->slash->active = false;
                                    button->runningTime->total->active = false;
                                    button->runningTime->track->active = false;
                                    MusicPlayer::stopMusic();
                                    setPlay = true;
                                }
                                break;
                            case TrackInteractionButtonType::Next:
                                button->runningTime->runtime->active = true;
                                button->runningTime->slash->active = true;
                                button->runningTime->total->active = true;
                                button->runningTime->track->active = true;
                                MusicPlayer::next();
                                if (MusicPlayer::paused)
                                    setPlay = true;
                                else setPause = true;
                                break;
                            }
                        }
                    });
                    if (setPlay)
                    {
                        EntityManager2D::foreachEntityWithAll<TrackInteractionButton>([&](Entity2D* entity, TrackInteractionButton* button)
                        {
                            if (button->type == TrackInteractionButtonType::Pause)
                                button->type = TrackInteractionButtonType::Play;
                        });
                    }
                    if (setPause)
                    {
                        EntityManager2D::foreachEntityWithAll<TrackInteractionButton>([&](Entity2D* entity, TrackInteractionButton* button)
                        {
                            if (button->type == TrackInteractionButtonType::Play)
                                button->type = TrackInteractionButtonType::Pause;
                        });
                    }
                    break;
                }
            };
            EngineEvent::OnMouseMove += [](sysm::vector2 from)
            {
                bool hoveredOverButton = false;
                EntityManager2D::foreachEntityWithAll<TrackInteractionButton>([&](Entity2D* entity, TrackInteractionButton* button)
                {
                    if (button->rectTransform()->queryPointIn(Input::mousePosition()))
                        hoveredOverButton = true;
                });
                // if (hoveredOverButton)
                //     Cursor::setCursor(TrackInteractionButton::cursorHover);
            };

            InternalEngineEvent::OnRenderOffloadForComponent2D += [](Component2D* component)
            {
                switch (component->typeIndex())
                {
                case __typeid(TrackInteractionButton).qualifiedNameHash():
                    static_cast<TrackInteractionButton*>(component)->renderOffload();
                    break;
                }
            };
        }
    } system;
public:
    TrackInteractionButtonType type;
    RunningTime* runningTime = nullptr;

    inline void renderOffload()
    {
        CoreEngine::queueRenderJobForFrame([w = Window::pixelWidth(), h = Window::pixelHeight(), bounds = NanoVG::boundsFromRectTransform(this->rectTransform()), type = this->type]
        {
            nvgBeginFrame(NanoVG::context, +w, +h, 1.0f);

            // Debugging, see rect bounds.
            // nvgBeginPath(NanoVG::context);
            // nvgRect(NanoVG::context, bounds.x + 1.0f, bounds.y + 1.0f, bounds.width - 2.0f, bounds.height - 2.0f);
            // nvgFillColor(NanoVG::context, nvgRGB(0x4c, 0x4c, 0x4c));
            // nvgFill(NanoVG::context);
            // nvgClosePath(NanoVG::context);
            
            switch (type)
            {
            case TrackInteractionButtonType::Play:
                {
                    nvgBeginPath(NanoVG::context);
                    float xOffset = (bounds.width - BUTTON_PADDING * 2.0f - (bounds.height - BUTTON_PADDING * 2.0f) * std::sqrt(3.0f) / 2.0f) / 2.0f;
                    nvgMoveTo(NanoVG::context, bounds.x + xOffset + BUTTON_PADDING, bounds.y + BUTTON_PADDING);
                    nvgLineTo(NanoVG::context, bounds.x + xOffset + BUTTON_PADDING + (bounds.height - BUTTON_PADDING * 2.0f) * std::sqrt(3.0f) / 2.0f, bounds.y + bounds.height / 2.0f);
                    nvgLineTo(NanoVG::context, bounds.x + xOffset + BUTTON_PADDING, bounds.y + bounds.height - BUTTON_PADDING);
                    nvgClosePath(NanoVG::context);
                    nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgFill(NanoVG::context);
                }
                break;
            case TrackInteractionButtonType::Pause:
                {
                    float centrelineOffset = (bounds.width - BUTTON_PADDING * 2.0f) / 2.0f;

                    nvgBeginPath(NanoVG::context);
                    nvgRect
                    (
                        NanoVG::context,
                        bounds.x + BUTTON_PADDING + centrelineOffset - PAUSE_BUTTON_BAR_WIDTH / 2.0f - PAUSE_BUTTON_BAR_WIDTH, bounds.y + BUTTON_PADDING + PAUSE_BUTTON_EXTRA_VERTICAL_PADDING,
                        PAUSE_BUTTON_BAR_WIDTH, bounds.height - BUTTON_PADDING * 2.0f - PAUSE_BUTTON_EXTRA_VERTICAL_PADDING * 2.0f
                    );
                    nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgFill(NanoVG::context);
                    
                    nvgBeginPath(NanoVG::context);
                    nvgRect
                    (
                        NanoVG::context,
                        bounds.x + BUTTON_PADDING + centrelineOffset + PAUSE_BUTTON_BAR_WIDTH / 2.0f, bounds.y + BUTTON_PADDING + PAUSE_BUTTON_EXTRA_VERTICAL_PADDING,
                        PAUSE_BUTTON_BAR_WIDTH, bounds.height - BUTTON_PADDING * 2.0f - PAUSE_BUTTON_EXTRA_VERTICAL_PADDING * 2.0f
                    );
                    nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgFill(NanoVG::context);
                }
                break;
            case TrackInteractionButtonType::Stop:
                nvgBeginPath(NanoVG::context);
                nvgRect
                (
                    NanoVG::context,
                    bounds.x + BUTTON_PADDING + STOP_BUTTON_EXTRA_PADDING, bounds.y + BUTTON_PADDING + STOP_BUTTON_EXTRA_PADDING,
                    bounds.width - (BUTTON_PADDING + STOP_BUTTON_EXTRA_PADDING) * 2.0f, bounds.height - (BUTTON_PADDING + STOP_BUTTON_EXTRA_PADDING) * 2.0f
                );
                nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                nvgFill(NanoVG::context);
                break;
            case TrackInteractionButtonType::Next:
                {
                    nvgBeginPath(NanoVG::context);
                    float overridePadding = BUTTON_PADDING + NEXT_BUTTON_STROKE_THICKNESS / 2.0f + NEXT_BUTTON_EXTRA_PADDING;
                    float xOffset = (bounds.width - overridePadding * 2.0f - (bounds.height - overridePadding * 2.0f) * std::sqrt(3.0f) / 2.0f - NEXT_BUTTON_STROKE_THICKNESS * 1.5f) / 2.0f;
                    nvgMoveTo(NanoVG::context, bounds.x + xOffset + overridePadding, bounds.y + overridePadding);
                    nvgLineTo(NanoVG::context, bounds.x + xOffset + overridePadding + (bounds.height - overridePadding * 2.0f) * std::sqrt(3.0f) / 2.0f, bounds.y + bounds.height / 2.0f);
                    nvgLineTo(NanoVG::context, bounds.x + xOffset + overridePadding, bounds.y + bounds.height - overridePadding);
                    nvgStrokeColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgStrokeWidth(NanoVG::context, NEXT_BUTTON_STROKE_THICKNESS);
                    nvgStroke(NanoVG::context);

                    nvgBeginPath(NanoVG::context);
                    nvgRect
                    (
                        NanoVG::context,
                        bounds.x + xOffset + overridePadding + (bounds.height - overridePadding * 2.0f) * std::sqrt(3.0f) / 2.0f + NEXT_BUTTON_STROKE_THICKNESS,
                        bounds.y + overridePadding - NEXT_BUTTON_STROKE_THICKNESS / 2.0f,
                        NEXT_BUTTON_STROKE_THICKNESS,
                        bounds.height - overridePadding * 2.0f + NEXT_BUTTON_STROKE_THICKNESS
                    );
                    nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgFill(NanoVG::context);
                }
                break;
            }

            nvgEndFrame(NanoVG::context);
        }, false);
    }
};