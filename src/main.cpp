#include <algorithm>
#include <concurrentqueue.h>
#include <filesystem>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#if _WIN32
#define NOMINMAX 1
#include <windows.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <Components/Mask.h>
#include <Components/Panel.h>
#include <Firework.Core.hpp>

#include <DropShadow.h>
#include <HorizontalBar.h>
#include <InteractableProgressBar.h>
#include <RunningTime.h>
#include <TacradCLI.h>
#include <TrackInteractionButton.h>
#include <VolumeButton.h>

#include <Firework/Entry.h>

namespace fs = std::filesystem;

using namespace Firework::Internal;

float hBarConYInitHeight = 0.4f;

inline Entity2D* background;
inline Panel* backgroundPanel;

inline HorizontalBar* hBarCon;

inline Entity2D* textEntry;
inline TacradCLI* textEntryInput;

#define DIVIDER_HEIGHT 30.0f
#define DIVIDER_BAR_HEIGHT 2.0f
#define TRACK_INTERACTION_ITEM_SPACING 15.0f
#define TRACK_INTERACTION_PADDING_SIDE 20.0f
#define VOLUME_SLIDER_WIDTH 50.0f
#define SLIDER_HEIGHT 4.0f
#define RUNNING_TIME_WIDTH 180.0f
#define TITLE_TEXT_HEIGHT 36.0f
#define SUBHEADING_TEXT_HEIGHT 22.0f

int main()
{
    // freopen("stdout.txt", "w", stdout);
    // freopen("stderr.txt", "w", stderr);

    Application::initializationOptions = RuntimeInitializationOptions { .windowName = "tacrad" };
    Application::setTargetFrameRate(30);

    EngineEvent::OnInitialize += []
    {
        Entity2D* ui = new Entity2D();
        ui->name = L"UI";

        Entity2D* hBarConEntity = new Entity2D();
        hBarConEntity->name = L"Horizontal Bar Divider [Playbar / Console]";
        hBarCon = hBarConEntity->addComponent<HorizontalBar>();
        hBarCon->rectTransform()->position = sysm::vector2(0, (float)Window::pixelHeight() * hBarConYInitHeight - (float)Window::pixelHeight() / 2.0f);
        hBarCon->rectTransform()->rect = RectFloat(DIVIDER_HEIGHT / 2.0f, (float)Window::pixelWidth() / 2.0f, -DIVIDER_HEIGHT / 2.0f, -(float)Window::pixelWidth() / 2.0f);
        hBarCon->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        hBarConEntity->addComponent<Panel>()->color = Color { 0x15, 0x15, 0x15, 0xff };

        ui->rectTransform()->localPosition = sysm::vector2(
            0, (float)Window::pixelHeight() / 2.0f - (float)Window::pixelHeight() * (1.0f - hBarConYInitHeight) + hBarConEntity->rectTransform()->rect().height() / 2.0f);
        ui->rectTransform()->rect = RectFloat((float)Window::pixelHeight() * (1.0f - hBarConYInitHeight) - hBarConEntity->rectTransform()->rect().height() / 2.0f,
                                              (float)Window::pixelWidth() / 2.0f, 0.0f, -(float)Window::pixelWidth() / 2.0f);
        ui->rectTransform()->rectAnchor = RectFloat(1, 1, 0, 1);
        hBarCon->above = ui->rectTransform();

        TrueTypeFontPackageFile* headingFont = file_cast<TrueTypeFontPackageFile>(PackageManager::lookupFileByPath(L"assets/Red_Hat_Display/static/RedHatDisplay-SemiBold.ttf"));

        Entity2D* titleText = new Entity2D();
        titleText->parent = ui;
        titleText->name = L"Title Text";
        titleText->rectTransform()->localPosition = sysm::vector2(0, ui->rectTransform()->rect().top);
        titleText->rectTransform()->rect = RectFloat(0, (float)Window::pixelWidth() / 2.0f - TRACK_INTERACTION_PADDING_SIDE, -TITLE_TEXT_HEIGHT,
                                                     -(float)Window::pixelWidth() / 2.0f + TRACK_INTERACTION_PADDING_SIDE);
        titleText->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        titleText->rectTransform()->positionAnchor = RectFloat(1, 0, 0, 0);
        Text* titleTextComponent = titleText->addComponent<Text>();
        titleTextComponent->text = U"The Tactical Radio Music Player";
        titleTextComponent->fontSize = TITLE_TEXT_HEIGHT;
        titleTextComponent->fontFile = headingFont;

        Entity2D* subheadingText = new Entity2D();
        subheadingText->parent = ui;
        subheadingText->name = L"Subheading Text";
        subheadingText->rectTransform()->localPosition = sysm::vector2(0, titleText->rectTransform()->localPosition().y + titleText->rectTransform()->rect().bottom);
        subheadingText->rectTransform()->rect = RectFloat(0, (float)Window::pixelWidth() / 2.0f - TRACK_INTERACTION_PADDING_SIDE, -TITLE_TEXT_HEIGHT,
                                                          -(float)Window::pixelWidth() / 2.0f + TRACK_INTERACTION_PADDING_SIDE);
        subheadingText->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        subheadingText->rectTransform()->positionAnchor = RectFloat(1, 0, 0, 0);
        Text* subheadingTextComponent = subheadingText->addComponent<Text>();
        subheadingTextComponent->text = U"A music player for strategic purposes.";
        subheadingTextComponent->fontSize = SUBHEADING_TEXT_HEIGHT;
        subheadingTextComponent->fontFile = headingFont;

        Entity2D* upperHitbox = new Entity2D();
        upperHitbox->parent = hBarConEntity;
        upperHitbox->name = L"Horizontal Bar Hitbox Upper";
        upperHitbox->rectTransform()->localPosition = sysm::vector2(0, DIVIDER_HEIGHT / 2.0f - DIVIDER_BAR_HEIGHT / 2.0f);
        upperHitbox->rectTransform()->rect = RectFloat(5, hBarConEntity->rectTransform()->rect().right, -5, hBarConEntity->rectTransform()->rect().left);
        upperHitbox->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        hBarCon->hitboxes.push_back(upperHitbox->rectTransform());

        Entity2D* lowerHitbox = new Entity2D();
        lowerHitbox->parent = hBarConEntity;
        lowerHitbox->name = L"Horizontal Bar Hitbox Lower";
        lowerHitbox->rectTransform()->localPosition = sysm::vector2(0, -DIVIDER_HEIGHT / 2.0f + DIVIDER_BAR_HEIGHT / 2.0f);
        lowerHitbox->rectTransform()->rect = RectFloat(5, hBarConEntity->rectTransform()->rect().right, -5, hBarConEntity->rectTransform()->rect().left);
        lowerHitbox->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        hBarCon->hitboxes.push_back(lowerHitbox->rectTransform());

        Entity2D* upperDivider = new Entity2D();
        upperDivider->parent = hBarConEntity;
        upperDivider->name = L"Horizontal Bar Divider Upper";
        upperDivider->rectTransform()->localPosition = sysm::vector2(0, DIVIDER_HEIGHT / 2.0f - DIVIDER_BAR_HEIGHT / 2.0f);
        upperDivider->rectTransform()->rect = RectFloat(1, hBarConEntity->rectTransform()->rect().right, -1, hBarConEntity->rectTransform()->rect().left);
        upperDivider->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        upperDivider->addComponent<Panel>()->color = Color { 0x2b, 0x2b, 0x2b, 0xff };

        Entity2D* lowerDivider = new Entity2D();
        lowerDivider->parent = hBarConEntity;
        lowerDivider->name = L"Horizontal Bar Divider Lower";
        lowerDivider->rectTransform()->localPosition = sysm::vector2(0, -DIVIDER_HEIGHT / 2.0f + DIVIDER_BAR_HEIGHT / 2.0f);
        lowerDivider->rectTransform()->rect = RectFloat(1, hBarConEntity->rectTransform()->rect().right, -1, hBarConEntity->rectTransform()->rect().left);
        lowerDivider->rectTransform()->rectAnchor = RectFloat(0, 1, 0, 1);
        lowerDivider->addComponent<Panel>()->color = Color { 0x2b, 0x2b, 0x2b, 0xff };

        struct InteractionButton
        {
            TrackInteractionButtonType type;
            std::wstring_view name;
        } interactionButtons[] { { TrackInteractionButtonType::Play, L"Play/Pause Button" },
                                 { TrackInteractionButtonType::Stop, L"Stop Button" },
                                 { TrackInteractionButtonType::Next, L"Next Button" } };
        std::vector<TrackInteractionButton*> interactionButtonComponents;
        for (int i = 0; i < 3; i++)
        {
            Entity2D* interactionButton = new Entity2D();
            interactionButton->parent = hBarConEntity;
            interactionButton->name = interactionButtons[i].name;
            interactionButton->rectTransform()->localPosition = sysm::vector2(-(float)Window::pixelWidth() / 2.0f + (DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f +
                                                                                  TRACK_INTERACTION_PADDING_SIDE + i * (DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f),
                                                                              0);
            interactionButton->rectTransform()->rect = RectFloat((DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, (DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f,
                                                                 -(DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, -(DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f);
            interactionButton->rectTransform()->positionAnchor = RectFloat(0, 0, 0, 1);

            interactionButtonComponents.push_back(interactionButton->addComponent<TrackInteractionButton>());
            interactionButtonComponents.back()->type = interactionButtons[i].type;
        }

        Entity2D* volumeButton = new Entity2D();
        volumeButton->parent = hBarConEntity;
        volumeButton->name = L"Volume Button";
        volumeButton->rectTransform()->localPosition = sysm::vector2(
            -(float)Window::pixelWidth() / 2.0f + TRACK_INTERACTION_PADDING_SIDE + 3.0f * (DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) + TRACK_INTERACTION_ITEM_SPACING, 0);
        volumeButton->rectTransform()->rect =
            RectFloat((DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f, -(DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, 0.0f);
        volumeButton->rectTransform()->positionAnchor = RectFloat(0, 0, 0, 1);
        VolumeButton* volumeIcon = volumeButton->addComponent<VolumeButton>();

        Entity2D* volumeSlider = new Entity2D();
        volumeSlider->parent = hBarConEntity;
        volumeSlider->name = L"Volume Slider";
        volumeSlider->rectTransform()->localPosition =
            sysm::vector2(volumeButton->rectTransform()->localPosition().x + volumeButton->rectTransform()->rect().width() + TRACK_INTERACTION_ITEM_SPACING / 2.0f, 0);
        volumeSlider->rectTransform()->rect = RectFloat(SLIDER_HEIGHT / 2.0f, VOLUME_SLIDER_WIDTH, -SLIDER_HEIGHT / 2.0f, 0.0f);
        volumeSlider->rectTransform()->positionAnchor = RectFloat(0, 0, 0, 1);

        InteractableProgressBar* volumeBar = volumeSlider->addComponent<InteractableProgressBar>();
        volumeBar->progress = 1.0f;
        volumeIcon->volumeBar = volumeBar;

        Entity2D* runningTime = new Entity2D();
        runningTime->parent = hBarConEntity;
        runningTime->name = L"Running Time";
        runningTime->rectTransform()->localPosition =
            sysm::vector2(volumeBar->rectTransform()->position().x + volumeBar->rectTransform()->rect().right + TRACK_INTERACTION_ITEM_SPACING, 0);
        runningTime->rectTransform()->rect =
            RectFloat((DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, RUNNING_TIME_WIDTH, -(DIVIDER_HEIGHT - DIVIDER_BAR_HEIGHT * 2.0f) / 2.0f, 0.0f);
        runningTime->rectTransform()->positionAnchor = RectFloat(0, 0, 0, 1);
        RunningTime* durationDisplay = runningTime->addComponent<RunningTime>();
        for (TrackInteractionButton* button : interactionButtonComponents) button->runningTime = durationDisplay;

        Entity2D* progressBar = new Entity2D();
        progressBar->parent = hBarConEntity;
        progressBar->name = L"Progress Bar";
        progressBar->rectTransform()->localPosition =
            sysm::vector2(runningTime->rectTransform()->localPosition().x + runningTime->rectTransform()->rect().right + TRACK_INTERACTION_ITEM_SPACING, 0);
        progressBar->rectTransform()->rect =
            RectFloat(SLIDER_HEIGHT / 2.0f,
                      hBarConEntity->rectTransform()->localPosition().x + hBarConEntity->rectTransform()->rect().right - TRACK_INTERACTION_PADDING_SIDE -
                          (runningTime->rectTransform()->localPosition().x + runningTime->rectTransform()->rect().right + TRACK_INTERACTION_ITEM_SPACING),
                      -SLIDER_HEIGHT / 2.0f, 0.0f);
        progressBar->rectTransform()->positionAnchor = RectFloat(0, 0, 0, 1);
        progressBar->rectTransform()->rectAnchor = RectFloat(0, 2, 0, 0);
        InteractableProgressBar* progress = progressBar->addComponent<InteractableProgressBar>();
        progress->active = false;
        durationDisplay->track = progress;
        progress->OnDragProgressChanged = [progress]
        {
            if (MusicPlayer::playing)
            {
                float seekQuery = (float)ma_engine_get_sample_rate(&MusicPlayer::engine) * progress->progress * MusicPlayer::musicLen;
                if (seekQuery >= 0.0f && seekQuery <= MusicPlayer::frameLen)
                    ma_sound_seek_to_pcm_frame(&MusicPlayer::music, (ma_uint64)seekQuery);
            }
        };

        textEntry = new Entity2D();
        hBarCon->below = textEntry->rectTransform();
        textEntry->name = L"Text Entry Field";

        textEntryInput = textEntry->addComponent<TacradCLI>();
        textEntryInput->rectTransform()->position =
            sysm::vector2(0, (float)Window::pixelHeight() * hBarConYInitHeight - (float)Window::pixelHeight() / 2.0f - hBarConEntity->rectTransform()->rect().height() / 2.0f);
        textEntryInput->rectTransform()->rect =
            RectFloat(0, Window::pixelWidth() / 2.0f, -(float)Window::pixelHeight() * hBarConYInitHeight + hBarConEntity->rectTransform()->rect().height() / 2.0f,
                      -Window::pixelWidth() / 2.0f);
        textEntryInput->rectTransform()->rectAnchor = RectFloat(0, 1, 1, 1);
        textEntryInput->fontSize = 16;

        textEntry->addComponent<Panel>()->color = Color { 0x18, 0x18, 0x18, 0xff };

        background = new Entity2D();
        background->name = L"Background";
        background->rectTransform()->rect =
            RectFloat((float)Window::pixelHeight() / 2.0f, (float)Window::pixelWidth() / 2.0f, -(float)Window::pixelHeight() / 2.0f, -(float)Window::pixelWidth() / 2.0f);
        background->rectTransform()->rectAnchor = RectFloat(1.0f, 1.0f, 1.0f, 1.0f);

        backgroundPanel = background->addComponent<Panel>();
        backgroundPanel->color = Color { 0x0c, 0x0c, 0x0c, 0xff };

        Debug::printHierarchy();
    };

    return 0;
}