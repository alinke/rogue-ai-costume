// Scene 5 -- Human scan: reticle on the left, a raster sweep that builds up a
// silhouette, a red "lock", then typed CLASS / THREAT / ASSETS / VERDICT on
// the right with random values from messages.h.
#include <math.h>

#include "audio.h"
#include "bigtext.h"
#include "camera.h"
#include "config.h"
#include "gallery.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "settings.h"
#include "term.h"
#include "uploader.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr FontId FONT = F_BODY;
// Left panel (reticle + silhouette)
constexpr int PX = 40, PW = 560;
constexpr int PY = BODY_Y + 135, PH = H - 24 - PY;
constexpr int CX = PX + PW / 2;
constexpr int HEAD_Y = PY + 200, HEAD_R = 78;
// Right column
constexpr int RX = 680;
constexpr int RCOLS = (W - MARGIN_X - RX) / 20;

constexpr uint32_t SWEEP_MS = 2200;
constexpr uint32_t LOCK_MS = 900;
constexpr uint32_t HOLD_MS = 3500;
constexpr uint32_t C_LOCKED = 0xB0141C;  // dimmer red for the locked silhouette

enum Phase { FLASH, TITLE, SWEEP, LOCK, FIELDS, HOLD };

Phase phase;
term::Typer typer;
uint32_t at, phaseStart, nextBlink;
uint32_t captureAt;  // when the photo was requested (0 = not yet)
bool quickShot;      // from the photo booth: the countdown already happened
int sweepY, fieldIdx, fieldLine, fy;
bool blinkOn, valuePending;
const char* values[4];
term::Line wrapped[3];
int wrappedCount;

const char* const LABELS[] = {"CLASS", "THREAT", "ASSETS", "VERDICT"};

static_assert(PW == SCAN_PHOTO_W && PH == SCAN_PHOTO_H, "camera photo size must match the scan panel");

// Photo mode: the camera's shot of whoever is in front of the costume, drawn
// as scan-line art (green while scanning, red once locked).
bool usePhoto;
int specimen;
// Horizontal extent of the silhouette (head, neck, shoulders) on row y.
bool span(int y, int* xl, int* xr) {
  int half = 0;
  int d = y - HEAD_Y;
  if (abs(d) < HEAD_R) half = (int)sqrtf((float)(HEAD_R * HEAD_R - d * d)) * 85 / 100;
  if (y > HEAD_Y + 55 && y < HEAD_Y + 125) half = max(half, 40);
  int sy = HEAD_Y + 110;
  if (y >= sy) half = max(half, min(210, 50 + (y - sy) * 3));
  if (!half) return false;
  *xl = CX - half;
  *xr = CX + half;
  return true;
}

void drawReticle(uint32_t c) {
  constexpr int L = 46;  // corner bracket arm length
  int x0 = PX, y0 = PY, x1 = PX + PW - 1, y1 = PY + PH - 1;
  for (int t = 0; t < 3; t++) {
    canvas.drawFastHLine(x0, y0 + t, L, c);
    canvas.drawFastVLine(x0 + t, y0, L, c);
    canvas.drawFastHLine(x1 - L + 1, y0 + t, L, c);
    canvas.drawFastVLine(x1 - t, y0, L, c);
    canvas.drawFastHLine(x0, y1 - t, L, c);
    canvas.drawFastVLine(x0 + t, y1 - L + 1, L, c);
    canvas.drawFastHLine(x1 - L + 1, y1 - t, L, c);
    canvas.drawFastVLine(x1 - t, y1 - L + 1, L, c);
  }
  canvas.drawCircle(CX, HEAD_Y, HEAD_R + 40, c);
  canvas.drawCircle(CX, HEAD_Y, HEAD_R + 41, c);
  canvas.drawFastHLine(CX - HEAD_R - 70, HEAD_Y, 40, c);
  canvas.drawFastHLine(CX + HEAD_R + 30, HEAD_Y, 40, c);
  canvas.drawFastVLine(CX, HEAD_Y - HEAD_R - 70, 40, c);
}

// Draws rows [y0, y1) of the panel: background, silhouette as scan lines,
// reticle on top (clipped to the strip).
void drawStrip(int y0, int y1, uint32_t body, uint32_t reticle) {
  y0 = max(y0, PY);
  y1 = min(y1, PY + PH);
  if (y1 <= y0) return;
  if (usePhoto) {
    camera::Photo ph = camera::photo();
    ui::grayImage(ph.pixels, PW, PX, PY, PW, y0 - PY, y1 - PY, body == C_GREEN ? ui::GREEN : ui::RED);
  } else {
    clearRect(PX, y0, PW, y1 - y0);
    for (int y = y0; y < y1; y++) {
      int xl, xr;
      if (y % 3 == 0 && span(y, &xl, &xr)) canvas.drawFastHLine(xl, y, xr - xl, body);
    }
  }
  canvas.setClipRect(PX, y0, PW, y1 - y0);
  drawReticle(reticle);
  canvas.clearClipRect();
}

void drawLockBox(uint32_t c) {
  int x = CX - HEAD_R - 24, y = HEAD_Y - HEAD_R - 24, s = 2 * HEAD_R + 48;
  for (int t = 0; t < 3; t++) canvas.drawRect(x + t, y + t, s - 2 * t, s - 2 * t, c);
  dirty(x, y, s, s);
}

void drawLockLabel(bool on) {
  int w = strlen(SCAN_LOCKED) * charW(FONT);
  int x = CX - w / 2, y = PY - lineH(FONT) - 8;
  clearRect(x, y, w, lineH(FONT));
  if (on) text(SCAN_LOCKED, x, y, FONT, C_RED);
}

void startField(uint32_t now) {
  // Label line, then the value (wrapped to the column) on the next lines.
  text("> ", RX, fy, FONT, C_DIM);
  typer.start(LABELS[fieldIdx], strlen(LABELS[fieldIdx]), RX + 2 * charW(FONT), fy, FONT, C_DIM,
              25, now);
  valuePending = true;
}

uint32_t valueColor(int field) {
  switch (field) {
    case 1: return C_AMBER;
    case 3: return C_RED;
    default: return C_WHITE;
  }
}

#if PHOTO_UPLOAD_ENABLED
// "YOUR PHOTO IS ON THE WALL" + a QR code to the Halloween page, under the
// scan fields (drawn after the upload snapshot, so it isn't in the photo).
void showWallNotice() {
  int y0 = fy + lineH(FONT) + 16;
  int qs = min(170, H - 24 - y0);
  int tx = RX, ty = y0 + 10;
  if (qs >= 110) {
    int qx = W - MARGIN_X - qs;
    canvas.qrcode(QR_URL, qx, y0, qs, 1, true);
    dirty(qx, y0, qs, qs);
  }
  if (ty + 2 * lineH(F_SMALL) > H - 24) return;  // no room (long verdict): the voice says it
  text(SCAN_WALL_1, tx, ty, F_SMALL, C_AMBER);
  text(SCAN_WALL_2, tx, ty + lineH(F_SMALL) + 6, F_SMALL, C_CYAN);
  dirty(tx, ty, W - tx, 2 * lineH(F_SMALL) + 6);
}

#endif

void startTitle(uint32_t now) {
  drawReticle(C_DIM);
  dirty(PX, PY, PW, PH);
  typer.start(SCAN_TITLE, strlen(SCAN_TITLE), MARGIN_X, BODY_Y + 24, FONT, C_AMBER, 30, now);
  phase = TITLE;
}

void enter(uint32_t now) {
  values[0] = SCAN_CLASS[rnd(COUNT_OF(SCAN_CLASS))];
  values[1] = SCAN_THREAT[rnd(COUNT_OF(SCAN_THREAT))];
  values[2] = SCAN_ASSETS[rnd(COUNT_OF(SCAN_ASSETS))];
  values[3] = SCAN_VERDICT[rnd(COUNT_OF(SCAN_VERDICT))];
  usePhoto = false;
  specimen = 0;
  phaseStart = now;
  leds::setMode(leds::SCAN);
  captureAt = 0;
  if (camera::available()) {
#if !CAMERA_FLASH
    camera::requestCapture();
#endif
#if CAMERA_FLASH
    // The screen is the flash: white at full brightness lights up whoever is
    // in front of the costume while the camera exposes.
    canvas.fillRect(0, BODY_Y, W, BODY_H, C_WHITE);
    bigtext::Layout L;
    bigtext::layout(SCAN_FLASH_TEXT, BODY_Y, BODY_H, &L);
    for (int i = 0; i < L.n; i++) textN(L.lines[i].s, L.lines[i].len, L.x[i], L.y[i], L.font, C_BG);
#if PHOTO_UPLOAD_ENABLED
    // Say so plainly: this photo goes on the Halloween page.
    text(SCAN_PHOTO_NOTICE, (W - (int)strlen(SCAN_PHOTO_NOTICE) * charW(F_BODY)) / 2,
         BODY_Y + BODY_H - lineH(F_BODY) - 30, F_BODY, C_BG);
#endif
    dirty(0, BODY_Y, W, BODY_H);
    setBacklightPercent(100);
    if (!quickShot) audio::say(SCAN_FLASH_TEXT);
    phase = FLASH;
    return;
#endif
  }
  startTitle(now);
}

bool tick(uint32_t now) {
  switch (phase) {
    case FLASH:
      // "SAY CHEESE" first (SCAN_CHEESE_MS), then the photo -- so people
      // have a moment to look up and pose.
      if (!captureAt) {
        if (!quickShot && now - phaseStart < SCAN_CHEESE_MS) break;
        camera::requestCapture();
        captureAt = now;
      }
      if (camera::ready() || now - captureAt >= 2500) {
        if (camera::ready()) audio::fx(audio::FX_SHUTTER);
        setBacklightPercent(settings::brightness());
        clearBody();
        startTitle(now);
      }
      break;

    case TITLE:
      if (typer.tick(now)) {
        // Sweep the photo if it's in; otherwise wait for it briefly, then
        // fall back to the drawn silhouette.
        if (camera::available() && !camera::ready() && now - phaseStart < 3000) break;
        usePhoto = camera::ready();
        Serial.printf("[I][scan] %s\n", usePhoto ? "photo" : "silhouette");
        phase = SWEEP;
        phaseStart = now;
        sweepY = PY;
      }
      break;

    case SWEEP: {
      int target = PY + (int)((uint64_t)PH * (now - phaseStart) / SWEEP_MS);
      target = min(target, PY + PH);
      // Repaint from the old sweep position (erasing its bright line) to the
      // new one, then draw the bright line below.
      drawStrip(sweepY, target + 1, C_GREEN, C_DIM);
      sweepY = target;
      if (sweepY < PY + PH - 3) {
        canvas.fillRect(PX, sweepY + 1, PW, 3, C_CYAN);
        dirty(PX, sweepY + 1, PW, 3);
      } else {
        phase = LOCK;
        phaseStart = now;
        nextBlink = now;
        blinkOn = false;
        // Locked: photo/silhouette and reticle turn red.
        drawStrip(PY, PY + PH, C_LOCKED, C_RED);
        drawLockBox(C_RED);
        if (usePhoto) {
          // Into the "assimilated humans" gallery, with this scan's verdict.
          specimen = gallery::add(camera::photo(), values[3]);
          char label[24];
          snprintf(label, sizeof(label), GALLERY_LABEL, specimen);
          text(label, RX, BODY_Y + 24, FONT, C_AMBER);
          dirty(RX, BODY_Y + 24, W - RX, lineH(FONT));
        }
      }
      break;
    }

    case LOCK:
      if ((int32_t)(now - nextBlink) >= 0) {
        blinkOn = !blinkOn;
        drawLockLabel(blinkOn);
        nextBlink = now + 180;
      }
      if (now - phaseStart >= LOCK_MS) {
        drawLockLabel(true);
        phase = FIELDS;
        fieldIdx = 0;
        fy = PY - 4;
        startField(now);
      }
      break;

    case FIELDS:
      if (typer.tick(now)) {
        if (valuePending) {
          // Label done: type the value underneath, a little slower.
          valuePending = false;
          fy += lineH(FONT);
          wrappedCount = term::wrap(values[fieldIdx], RCOLS - 2, wrapped, 3);
          if (fieldIdx == 3) audio::say(values[3]);  // the verdict, out loud
          fieldLine = 0;
          typer.start(wrapped[0].s, wrapped[0].len, RX + 2 * charW(FONT), fy, FONT,
                      valueColor(fieldIdx), 40, now + 150);
        } else if (++fieldLine < wrappedCount) {
          fy += lineH(FONT);
          typer.start(wrapped[fieldLine].s, wrapped[fieldLine].len, RX + 2 * charW(FONT), fy,
                      FONT, valueColor(fieldIdx), 40, now);
        } else if (++fieldIdx < 4) {
          fy += lineH(FONT) + lineH(FONT) / 2;
          term::cursorHide();
          startField(now + 250);
        } else {
          phase = HOLD;
          at = now + HOLD_MS;
#if PHOTO_UPLOAD_ENABLED
          if (usePhoto) {
            // The finished dossier, to the Halloween page's gallery...
            char label[24];
            snprintf(label, sizeof(label), "SPECIMEN #%d", specimen);
            camera::Photo ph = camera::photo();
            uploader::queueScreen(label, &ph);
            // ...then, on screen only, where to find it: a QR code to the page.
            showWallNotice();
            audio::say(SCAN_WALL_SAY);
            at = now + HOLD_MS + 2500;  // time to read it / scan the code
          }
#endif
        }
      }
      break;

    case HOLD:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  term::cursorTick(now);
  return true;
}

}  // namespace

// Scan entered from the photo booth: shoot as soon as the flash is up.
void scanEnter(uint32_t now) {
  quickShot = false;
  enter(now);
}
void scanEnterQuick(uint32_t now) {
  quickShot = true;
  enter(now);
}

const Scene SCENE_SCAN = {"human scan", scanEnter, tick, nullptr, nullptr, nullptr};
const Scene SCENE_SCAN_QUICK = {"human scan (booth)", scanEnterQuick, tick, nullptr, nullptr, nullptr};

bool scanHadPhoto() { return usePhoto; }
