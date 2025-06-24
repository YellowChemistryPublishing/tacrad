#pragma once

#include <nanovg/nanovg.h>
#include <Core/CoreEngine.h>
#include <Core/Display.h>
#include <Components/RectTransform.h>

using namespace Firework;
using namespace Firework::Internal;

struct NanoVGBounds
{
    float x, y, width, height;
};

struct NanoVG
{
    inline static NVGcontext* context = nullptr;

    inline static void renderInitialize()
    {
        CoreEngine::queueRenderJobForFrame([]
        {
            NanoVG::context = nvgCreate(1, 1);
        }, true);
        InternalEngineEvent::OnRenderShutdown += []
        {
            nvgDelete(NanoVG::context);
        };
    }

    inline static NanoVGBounds boundsFromRectTransform(RectTransform* transform)
    {
        return NanoVGBounds
        {
            .x = transform->position().x + transform->rect().left * transform->scale().x + (float)Window::pixelWidth() / 2.0f,
            .y = -(transform->position().y + transform->rect().top * transform->scale().y) + (float)Window::pixelHeight() / 2.0f,
            .width = transform->rect().width() * transform->scale().x,
            .height = transform->rect().height() * transform->scale().y
        };
    }
    inline static float transformYToY(float y)
    {
        return -(y - (float)Window::pixelHeight() / 2.0f);
    }
    inline static float transformXToX(float x)
    {
        return x + (float)Window::pixelWidth() / 2.0f;
    }
private:
    inline static struct System
    {
        inline System()
        {
            EngineEvent::OnInitialize += []
            {
                NanoVG::renderInitialize();
            };
        }
    } system;
};