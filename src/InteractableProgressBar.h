#pragma once

#include <EntityComponentSystem/ComponentData.h>

#include <NanoVGContext.h>

#define PROGRESS_BAR_EXTRA_HITBOX_X_PADDING 2.0f
#define PROGRESS_BAR_EXTRA_HITBOX_Y_PADDING 5.0f

using namespace Firework;
using namespace Firework::Internal;

class InteractableProgressBar : public ComponentData2D
{
    inline void renderOffload()
    {
        CoreEngine::queueRenderJobForFrame([w = Window::pixelWidth(), h = Window::pixelHeight(), bounds = NanoVG::boundsFromRectTransform(this->rectTransform()), progress = this->progress]
        {
            nvgBeginFrame(NanoVG::context, +w, +h, 1.0f);

            nvgBeginPath(NanoVG::context);
            nvgRect(NanoVG::context, bounds.x, bounds.y, bounds.width, bounds.height);
            nvgFillColor(NanoVG::context, nvgRGB(0x4c, 0x4c, 0x4c));
            nvgFill(NanoVG::context);
            nvgClosePath(NanoVG::context);

            nvgBeginPath(NanoVG::context);
            nvgRect(NanoVG::context, bounds.x, bounds.y, bounds.width * progress, bounds.height);
            nvgFillColor(NanoVG::context, nvgRGB(0xff, 0xff, 0xff));
            nvgFill(NanoVG::context);
            nvgClosePath(NanoVG::context);

            nvgEndFrame(NanoVG::context);
        }, false);
    }

    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnMouseDown += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    EntityManager2D::foreachEntityWithAll<InteractableProgressBar>([&](Entity2D* entity, InteractableProgressBar* bar)
                    {
                        if (bar->hitbox->queryPointIn(Input::mousePosition()))
                        {
                            bar->dragging = true;
                            bar->progress = sysm::clamp((float)(Input::mousePosition().x - bar->rectTransform()->position().x) / bar->rectTransform()->rect().width(), 0.0f, 1.0f);
                            bar->OnDragProgressChanged();
                        }
                    });
                    break;
                }
            };
            EngineEvent::OnMouseMove += [](sysm::vector2 from)
            {
                EntityManager2D::foreachEntityWithAll<InteractableProgressBar>([&](Entity2D* entity, InteractableProgressBar* bar)
                {
                    if (bar->dragging)
                    {
                        bar->progress = sysm::clamp((float)(Input::mousePosition().x - bar->rectTransform()->position().x) / bar->rectTransform()->rect().width(), 0.0f, 1.0f);
                        bar->OnDragProgressChanged();
                    }
                });
            };
            EngineEvent::OnMouseUp += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    EntityManager2D::foreachEntityWithAll<InteractableProgressBar>([&](Entity2D* entity, InteractableProgressBar* bar)
                    {
                        bar->dragging = false;
                    });
                    break;
                }
            };

            InternalEngineEvent::OnRenderOffloadForComponent2D += [](Component2D* component)
            {
                switch (component->typeIndex())
                {
                case __typeid(InteractableProgressBar).qualifiedNameHash():
                    static_cast<InteractableProgressBar*>(component)->renderOffload();
                    break;
                }
            };
        }
    } system;
public:
    // Progress from 0.0 to 1.0.
    float progress = 0.0f;
    bool dragging = false;

    RectTransform* hitbox;

    func::function<void()> OnDragProgressChanged = []() -> void { };

    inline void onCreate()
    {
        Entity2D* hitbox = new Entity2D();
        hitbox->parent = this->entity();
        hitbox->name = L"Progress Bar Hitbox";
        hitbox->rectTransform()->localPosition = sysm::vector2::zero;
        hitbox->rectTransform()->rect = RectFloat
        (
            this->rectTransform()->rect().top + PROGRESS_BAR_EXTRA_HITBOX_Y_PADDING, this->rectTransform()->rect().right + PROGRESS_BAR_EXTRA_HITBOX_X_PADDING,
            this->rectTransform()->rect().bottom - PROGRESS_BAR_EXTRA_HITBOX_Y_PADDING, this->rectTransform()->rect().left - PROGRESS_BAR_EXTRA_HITBOX_X_PADDING
        );
        hitbox->rectTransform()->rectAnchor = RectFloat(1, 1, 1, 1);
        this->hitbox = hitbox->rectTransform();
    }
};