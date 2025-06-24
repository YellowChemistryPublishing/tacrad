#pragma once

#include <Components/Panel.h>
#include <Core/Cursor.h>
#include <EntityComponentSystem/ComponentData.h>
#include <EntityComponentSystem/EngineEvent.h>

using namespace Firework;
using namespace Firework::Internal;

class HorizontalBar : public ComponentData2D
{
    inline static CursorTexture* cursorDefault;
    inline static CursorTexture* cursorHover;
public:
    std::vector<RectTransform*> hitboxes;
    RectTransform* above = nullptr;
    RectTransform* below = nullptr;
    
    bool selected = false;

    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnInitialize += []
            {
                HorizontalBar::cursorDefault = new CursorTexture(BuiltinCursorTexture::Default);
                HorizontalBar::cursorHover = new CursorTexture(BuiltinCursorTexture::BiArrowVertical);
            };
            EngineEvent::OnTick += []()
            {
                EntityManager2D::foreachEntityWithAll<HorizontalBar>([&](Entity2D* entity, HorizontalBar* bar)
                {
                    if (bar->selected)
                    {
                        float yPos = sysm::clamp(bar->rectTransform()->localPosition().y + (float)Input::mouseMotion().y, -Window::pixelHeight() / 2.0f + bar->rectTransform()->rect().height() / 2.0f, Window::pixelHeight() / 2.0f * (1.0f - 0.2f * 2.0f));
                        float yDelta = yPos - bar->rectTransform()->localPosition().y;

                        bar->rectTransform()->localPosition = sysm::vector2(0, yPos);
                        if (bar->above)
                        {
                            bar->above->localPosition += sysm::vector2(0, yDelta);
                            bar->above->rect = RectFloat
                            (
                                bar->above->rect().top - yDelta, bar->above->rect().right,
                                bar->above->rect().bottom, bar->above->rect().left
                            );
                        }
                        if (bar->below)
                        {
                            bar->below->localPosition += sysm::vector2(0, yDelta);
                            bar->below->rect = RectFloat
                            (
                                bar->below->rect().top, bar->below->rect().right,
                                bar->below->rect().bottom - yDelta, bar->below->rect().left
                            );
                        }
                    }
                });
            };
            EngineEvent::OnQuit += []
            {
                delete HorizontalBar::cursorDefault;
                delete HorizontalBar::cursorHover;
            };

            EngineEvent::OnMouseDown += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    EntityManager2D::foreachEntityWithAll<HorizontalBar>([&](Entity2D* entity, HorizontalBar* bar)
                    {
                        for (RectTransform* hitbox : bar->hitboxes)
                        {
                            if (hitbox->queryPointIn((sysm::vector2)Input::mousePosition()))
                            {
                                bar->selected = true;
                                break;
                            }
                        }
                    });
                    break;
                }
            };
            EngineEvent::OnMouseMove += [](sysm::vector2 from)
            {
                bool isStartDraggingOrHovered = false;
                EntityManager2D::foreachEntityWithAll<HorizontalBar>([&](Entity2D* entity, HorizontalBar* bar)
                {
                    if (!isStartDraggingOrHovered)
                    {
                        isStartDraggingOrHovered = bar->selected || [&]
                        {
                            for (RectTransform* hitbox : bar->hitboxes)
                            {
                                if (hitbox->queryPointIn((sysm::vector2)Input::mousePosition()))
                                    return true;
                            }
                            return false;
                        }();
                    }
                });
                // if (isStartDraggingOrHovered)
                //     Cursor::setCursor(HorizontalBar::cursorHover);
            };
            EngineEvent::OnMouseUp += [](MouseButton button)
            {
                switch (button)
                {
                case MouseButton::Left:
                    bool resetCursor = false;
                    EntityManager2D::foreachEntityWithAll<HorizontalBar>([&](Entity2D* entity, HorizontalBar* bar)
                    {
                        resetCursor = resetCursor || bar->selected;
                        bar->selected = false;
                    });
                    // if (resetCursor)
                    //     Cursor::setCursor(HorizontalBar::cursorDefault);
                    break;
                }
            };
        }
    } system;
};