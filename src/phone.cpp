#include <M5Unified.h>
#include <LGFX_PPA.hpp>
#include <math.h>

static constexpr int16_t SCREEN_W = 720;
static constexpr int16_t SCREEN_H = 1280;
static constexpr int16_t GESTURE_H = 180;
static constexpr int16_t ICON_SIZE = 184;
static constexpr uint32_t FRAME_MS = 50;
static constexpr uint32_t CLOSE_MS = 480;

PPA_Sprite frame(&M5.Display);

struct AppTile {
  const char* label;
  uint16_t color;
};

const AppTile apps[] = {
  {"Notes", 0xFD60}, {"Photos", 0xF9A6}, {"Music", 0x8D5F},
  {"Weather", 0x05FF}, {"Settings", 0x8410}, {"Timer", 0xF800},
};
static constexpr uint8_t APP_COUNT = sizeof(apps) / sizeof(apps[0]);

bool portraitGesture = false;
bool rawLandscapeGesture = false;
int16_t portraitStartY = 0;
int16_t rawStartX = 0;
bool closing = false;
int8_t activeApp = -1;
uint32_t closeStarted = 0;
uint32_t lastFrame = 0;

static float easeInOutSine(float value) {
  value = constrain(value, 0.0f, 1.0f);
  return -(cosf(PI * value) - 1.0f) * 0.5f;
}

static void drawIcon(int16_t x, int16_t y, const AppTile& app, float exitProgress) {
  const int16_t offset = (int16_t)(exitProgress * 150.0f);
  const int16_t alphaY = y - offset;
  frame.fillRoundRect(x, alphaY, ICON_SIZE, ICON_SIZE, 18, app.color);
  frame.setFont(&fonts::Font4);
  frame.setTextSize(1);
  frame.setTextColor(0x2124, TFT_WHITE);
  frame.setTextDatum(top_center);
  frame.drawString(app.label, x + ICON_SIZE / 2, alphaY + ICON_SIZE + 16);
}

static void renderLauncher(float closeProgress) {
  frame.fillScreen(TFT_WHITE);

  if (activeApp >= 0) {
    for (uint8_t i = 0; i < APP_COUNT; ++i) {
      const int16_t x = 64 + (i % 3) * 224;
      const int16_t y = 278 + (i / 3) * 316;
      drawIcon(x, y, apps[i], 0.0f);
    }

    const int16_t tileX = 64 + (activeApp % 3) * 224;
    const int16_t tileY = 278 + (activeApp / 3) * 316;
    const float remain = 1.0f - closeProgress;
    const int16_t cardW = (int16_t)(ICON_SIZE + (592 - ICON_SIZE) * remain);
    const int16_t cardH = (int16_t)(ICON_SIZE + (360 - ICON_SIZE) * remain);
    const int16_t cardX = (int16_t)(tileX + (64 - tileX) * remain);
    const int16_t cardY = (int16_t)(tileY + (250 - tileY) * remain);
    const int16_t radius = (int16_t)(18 + 6 * remain);
    frame.fillRoundRect(cardX, cardY, cardW, cardH, radius, apps[activeApp].color);

    if (remain > 0.45f) {
      frame.setFont(&fonts::Font4);
      frame.setTextSize(2);
      frame.setTextColor(TFT_WHITE, apps[activeApp].color);
      frame.setTextDatum(middle_center);
      frame.drawString(apps[activeApp].label, cardX + cardW / 2, cardY + cardH / 2 - 24);
      frame.setTextSize(1);
      frame.drawString("Tap to return", cardX + cardW / 2, cardY + cardH / 2 + 50);
    }
  } else {
    for (uint8_t i = 0; i < APP_COUNT; ++i) {
      const int16_t x = 64 + (i % 3) * 224;
      const int16_t y = 278 + (i / 3) * 316;
      drawIcon(x, y, apps[i], closeProgress);
    }
  }

  const int16_t barY = SCREEN_H - GESTURE_H;
  frame.fillRoundRect(150, barY + 62, 420, 18, 9, 0x2124);
  frame.pushSprite(&M5.Display, 0, 0);
}

static void updateTouch() {
  const auto detail = M5.Touch.getDetail();
  if (detail.wasPressed()) {
    portraitGesture = detail.y >= SCREEN_H - GESTURE_H;
    rawLandscapeGesture = false;
    portraitStartY = detail.y;
    rawStartX = detail.x;
  }
  if (detail.wasReleased()) {
    const bool portraitSwipe = portraitGesture && (portraitStartY - detail.y > 70);
    if (!closing && portraitSwipe) {
      closing = true;
      closeStarted = millis();
      lastFrame = 0;
    } else if (!portraitGesture && !closing) {
      const int16_t moved = abs(detail.y - portraitStartY) + abs(detail.x - rawStartX);
      if (moved < 36) {
        if (activeApp >= 0) {
          activeApp = -1;
        } else {
          for (uint8_t i = 0; i < APP_COUNT; ++i) {
            const int16_t x = 64 + (i % 3) * 224;
            const int16_t y = 278 + (i / 3) * 316;
            if (detail.x >= x && detail.x < x + ICON_SIZE &&
                detail.y >= y && detail.y < y + ICON_SIZE) {
              activeApp = i;
              break;
            }
          }
        }
        renderLauncher(0.0f);
      }
    }
    portraitGesture = false;
    rawLandscapeGesture = false;
  }
}

void setup() {
  auto config = M5.config();
  M5.begin(config);
  M5.Display.setRotation(0);
  M5.Display.setColorDepth(16);

  frame.setColorDepth(16);
  frame.setPsram(true);
  if (frame.createSprite(SCREEN_W, SCREEN_H) == nullptr) {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("PSRAM frame allocation failed", 20, 20);
    while (true) delay(1000);
  }
  renderLauncher(0.0f);
}

void loop() {
  M5.update();
  updateTouch();

  const uint32_t now = millis();
  if (!closing || now - lastFrame < FRAME_MS) return;
  lastFrame = now;

  const float closeProgress = easeInOutSine((float)(now - closeStarted) / CLOSE_MS);
  renderLauncher(closeProgress);
  if (closeProgress >= 1.0f) {
    closing = false;
    activeApp = -1;
    renderLauncher(0.0f);
  }
}
