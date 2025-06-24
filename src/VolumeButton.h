#pragma once

#include <numbers>

#include <EntityComponentSystem/ComponentData.h>

#include <InteractableProgressBar.h>
#include <MusicPlayer.h>
#include <NanoVGContext.h>

#define VOLUME_LINE_STROKE_THICKNESS 2.0f
#define VOLUME_BUTTON_EXTRA_PADDING 2.0f

using namespace Firework;
using namespace Firework::Internal;

class VolumeButton : public ComponentData2D
{
    float prevVolume = 1.0f;

    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnTick += []
            {
                EntityManager2D::foreachEntityWithAll<VolumeButton>([&](Entity2D* entity, VolumeButton* button)
                {
                    if (button->volumeBar->progress != button->prevVolume)
                    {
                        button->prevVolume = button->volumeBar->progress;
                        if (!button->muted)
                            ma_engine_set_volume(&MusicPlayer::engine, button->volumeBar->progress);
                    }
                });
            };
            EngineEvent::OnMouseDown += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    EntityManager2D::foreachEntityWithAll<VolumeButton>([&](Entity2D* entity, VolumeButton* button)
                    {
                        if (button->rectTransform()->queryPointIn(Input::mousePosition()))
                        {
                            button->muted = !button->muted;
                            ma_engine_set_volume(&MusicPlayer::engine, button->muted ? 0.0f : button->volumeBar->progress);
                        }
                    });
                    break;
                }
            };

            InternalEngineEvent::OnRenderOffloadForComponent2D += [](Component2D* component)
            {
                switch (component->typeIndex())
                {
                case __typeid(VolumeButton).qualifiedNameHash(): static_cast<VolumeButton*>(component)->renderOffload(); break;
                }
            };
        }
    } system;
public:
    InteractableProgressBar* volumeBar = nullptr;
    bool muted = false;

    inline void renderOffload()
    {
        CoreEngine::queueRenderJobForFrame(
            [w = Window::pixelWidth(), h = Window::pixelHeight(), bounds = NanoVG::boundsFromRectTransform(this->rectTransform()), muted = this->muted,
             vol = this->volumeBar->progress]
        {
            nvgBeginFrame(NanoVG::context, +w, +h, 1.0f);

            float offset = VOLUME_BUTTON_EXTRA_PADDING;

            nvgBeginPath(NanoVG::context);
            sysm::vector2 speakerPoints[] { { 0.9f, 3.5f }, { 2.8f, 3.5f }, { 4.8f, 1.5f }, { 4.8f, 8.5f }, { 2.8f, 6.5f }, { 0.9f, 6.5f } };
            nvgMoveTo(NanoVG::context, bounds.x + speakerPoints[0].x / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                      bounds.y + speakerPoints[0].y / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
            for (int i = 1; i < 6; i++)
                nvgLineTo(NanoVG::context, bounds.x + speakerPoints[i].x / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                          bounds.y + speakerPoints[i].y / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
            nvgClosePath(NanoVG::context);
            nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
            nvgFill(NanoVG::context);

            if (vol == 0.0f || muted)
            {
                nvgBeginPath(NanoVG::context);
                nvgMoveTo(NanoVG::context, bounds.x + 5.8f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                          bounds.y + 4.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
                nvgLineTo(NanoVG::context, bounds.x + 7.8f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                          bounds.y + 6.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
                nvgStrokeColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                nvgStrokeWidth(NanoVG::context, VOLUME_LINE_STROKE_THICKNESS);
                nvgStroke(NanoVG::context);

                nvgBeginPath(NanoVG::context);
                nvgMoveTo(NanoVG::context, bounds.x + 7.8f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                          bounds.y + 4.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
                nvgLineTo(NanoVG::context, bounds.x + 5.8f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                          bounds.y + 6.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset);
                nvgStrokeColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                nvgStrokeWidth(NanoVG::context, VOLUME_LINE_STROKE_THICKNESS);
                nvgStroke(NanoVG::context);
            }
            else
            {
                if (vol > 0.5f)
                {
                    nvgBeginPath(NanoVG::context);
                    nvgArc(NanoVG::context, bounds.x + 5.3f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                           bounds.y + 5.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                           3.5f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) - VOLUME_LINE_STROKE_THICKNESS / 2.0f, -std::numbers::pi_v<float> / 2.0f,
                           std::numbers::pi_v<float> / 2.0f, NVG_CW);
                    nvgStrokeColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgStrokeWidth(NanoVG::context, VOLUME_LINE_STROKE_THICKNESS);
                    nvgStroke(NanoVG::context);
                }
                if (vol > 0.0f)
                {
                    nvgBeginPath(NanoVG::context);
                    nvgArc(NanoVG::context, bounds.x + 5.3f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                           bounds.y + 5.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) + offset,
                           2.0f / 10.0f * (bounds.height - VOLUME_BUTTON_EXTRA_PADDING * 2.0f) - VOLUME_LINE_STROKE_THICKNESS / 2.0f, -std::numbers::pi_v<float> / 2.0f,
                           std::numbers::pi_v<float> / 2.0f, NVG_CW);
                    nvgStrokeColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
                    nvgStrokeWidth(NanoVG::context, VOLUME_LINE_STROKE_THICKNESS);
                    nvgStroke(NanoVG::context);
                }
            }

            nvgEndFrame(NanoVG::context);
        },
            false);
    }
};