#pragma once

#include <Core/Display.h>
#include <Drawing/Core.h>
#include <EntityComponentSystem/ComponentData.h>
#include <EntityComponentSystem/EngineEvent.h>
#include <Library/TypeInfo.h>

#include <NanoVGContext.h>

using namespace Firework;
using namespace Firework::Internal;

class ComponentCoreStaticInit;

class DropShadow : public ComponentData2D
{
    inline void renderOffload()
    {
        CoreEngine::queueRenderJobForFrame(
            [w = Window::pixelWidth(), h = Window::pixelHeight(), bounds = NanoVG::boundsFromRectTransform(this->rectTransform()), topColor = this->topColor,
             bottomColor = this->bottomColor]
        {
            nvgBeginFrame(NanoVG::context, +w, +h, 1.0f);

            nvgBeginPath(NanoVG::context);
            nvgRect(NanoVG::context, bounds.x, bounds.y, bounds.width, bounds.height);
            nvgFillPaint(NanoVG::context,
                         nvgLinearGradient(NanoVG::context, bounds.x, bounds.y, bounds.x, bounds.y + bounds.height, nvgRGBA(topColor.r, topColor.g, topColor.b, topColor.a),
                                           nvgRGBA(bottomColor.r, bottomColor.g, bottomColor.b, bottomColor.a)));
            nvgFill(NanoVG::context);
            nvgClosePath(NanoVG::context);

            nvgEndFrame(NanoVG::context);
        },
            false);
    }

    inline static struct System
    {
        inline System()
        {
            InternalEngineEvent::OnRenderOffloadForComponent2D += [](Component2D* component)
            {
                switch (component->typeIndex())
                {
                case __typeid(DropShadow).qualifiedNameHash(): static_cast<DropShadow*>(component)->renderOffload(); break;
                }
            };
        }
    } system;
public:
    Color topColor = Color(0, 0, 0, 128);
    Color bottomColor = Color(0, 0, 0, 0);

    friend class ::ComponentCoreStaticInit;
};