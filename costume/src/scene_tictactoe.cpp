// Tic-tac-toe against an AI that cheats: it plays a beatable game, and the
// moment a human completes a line it "updates the rules" and takes one of
// their squares. Humans never win.
#include "audio.h"
#include "bigtext.h"
#include "config.h"
#include "header.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int CELL = 170, GAP = 8;
constexpr int BX = MARGIN_X + 20, BY = BODY_Y + 130;
constexpr int SX = BX + 3 * CELL + 2 * GAP + 60;  // status column
const int LINES[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

enum Phase { HUMAN, AI_WAIT, OVER, TIMEOUT };
Phase phase;
uint8_t board[9];  // 0 empty, 1 X (human), 2 O (AI)
uint32_t at, lastTouch;
term::Bag cheatBag;

int cellAt(int x, int y) {
  if (x < BX || y < BY) return -1;
  int c = (x - BX) / (CELL + GAP), r = (y - BY) / (CELL + GAP);
  return (c < 3 && r < 3) ? r * 3 + c : -1;
}

void drawCell(int i, uint32_t tint) {
  int x = BX + (i % 3) * (CELL + GAP), y = BY + (i / 3) * (CELL + GAP);
  clearRect(x, y, CELL, CELL);
  for (int t = 0; t < 2; t++) canvas.drawRect(x + t, y + t, CELL - 2 * t, CELL - 2 * t, tint ? tint : C_DIM);
  int cx = x + CELL / 2, cy = y + CELL / 2, r = CELL / 2 - 30;
  if (board[i] == 1) {
    canvas.drawWideLine(cx - r, cy - r, cx + r, cy + r, 7, C_CYAN);
    canvas.drawWideLine(cx - r, cy + r, cx + r, cy - r, 7, C_CYAN);
  } else if (board[i] == 2) {
    fillRing(cx, cy, r - 8, r, C_RED);
  }
  dirty(x, y, CELL, CELL);
}

void drawBoard() {
  for (int i = 0; i < 9; i++) drawCell(i, 0);
}

int winner(const uint8_t* b, int* line = nullptr) {
  for (int k = 0; k < 8; k++) {
    int a = LINES[k][0];
    if (b[a] && b[a] == b[LINES[k][1]] && b[a] == b[LINES[k][2]]) {
      if (line) *line = k;
      return b[a];
    }
  }
  return 0;
}

bool full(const uint8_t* b) {
  for (int i = 0; i < 9; i++) if (!b[i]) return false;
  return true;
}

int minimax(uint8_t* b, bool aiTurn) {
  int w = winner(b);
  if (w == 2) return 1;
  if (w == 1) return -1;
  if (full(b)) return 0;
  int best = aiTurn ? -2 : 2;
  for (int i = 0; i < 9; i++) {
    if (b[i]) continue;
    b[i] = aiTurn ? 2 : 1;
    int v = minimax(b, !aiTurn);
    b[i] = 0;
    best = aiTurn ? max(best, v) : min(best, v);
  }
  return best;
}

int aiMove() {
  // Take a win if there is one; otherwise play well about half the time.
  for (int i = 0; i < 9; i++) {
    if (board[i]) continue;
    board[i] = 2;
    bool wins = winner(board) == 2;
    board[i] = 0;
    if (wins) return i;
  }
  int empties[9], n = 0;
  for (int i = 0; i < 9; i++) if (!board[i]) empties[n++] = i;
  if (rnd(100) < 45) return empties[rnd(n)];
  int best = empties[0], bestV = -2;
  for (int k = 0; k < n; k++) {
    board[empties[k]] = 2;
    int v = minimax(board, false);
    board[empties[k]] = 0;
    if (v > bestV) { bestV = v; best = empties[k]; }
  }
  return best;
}

void status(const char* s, uint32_t c) {
  int y = BY + 160;
  clearRect(SX, y, W - SX - MARGIN_X, 3 * lineH(F_BODY));
  term::Line lines[3];
  int n = term::wrap(s, (W - SX - MARGIN_X) / charW(F_BODY), lines, 3);
  for (int i = 0; i < n; i++) textN(lines[i].s, lines[i].len, SX, y + i * lineH(F_BODY), F_BODY, c);
}

void finish(int w, int line, uint32_t now) {
  if (w == 2) {
    for (int k = 0; k < 3; k++) drawCell(LINES[line][k], C_RED);
    status(TTT_AI_WINS, C_RED);
    audio::say(TTT_AI_WINS);
    header::raiseThreat();
  } else {
    status(TTT_DRAW, C_AMBER);
    audio::say(TTT_DRAW);
  }
  phase = OVER;
  at = now + 3800;
}

void enter(uint32_t now) {
  if (cheatBag.n == 0) cheatBag.init(COUNT_OF(TTT_CHEATS));
  memset(board, 0, sizeof(board));
  text(TTT_TITLE, BX, BODY_Y + 30, F_LARGE, C_AMBER);
  text(TTT_HINT, SX, BY + 40, F_BODY, C_WHITE);
  dirty(0, BODY_Y, W, BY - BODY_Y + 100);
  drawBoard();
  phase = HUMAN;
  lastTouch = now;
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  if (phase != HUMAN) return;
  int i = cellAt(x, y);
  if (i < 0 || board[i]) return;
  audio::fx(audio::FX_KEY);
  board[i] = 1;
  drawCell(i, 0);
  int line;
  if (winner(board, &line) == 1) {
    // The human won. That can't be allowed: one of their squares becomes ours.
    int victim = LINES[line][rnd(3)];
    board[victim] = 2;
    const char* cheat = TTT_CHEATS[cheatBag.next()];
    sliceShift(BY, 3 * CELL + 2 * GAP, (int)rnd(20, 60) * (rnd(2) ? 1 : -1), BX, BX + 3 * CELL + 2 * GAP);
    drawBoard();
    drawCell(victim, C_RED);
    status(cheat, C_RED);
    audio::fx(audio::FX_DENIED);
    audio::say(cheat);
    Serial.println("[I][ttt] human won -- cheating");
  } else {
    status("THINKING...", C_DIM);
  }
  int w = winner(board, &line);
  if (w || full(board)) {
    finish(w, line, now);
    return;
  }
  phase = AI_WAIT;
  at = now + rnd(500, 1100);
}

bool tick(uint32_t now) {
  switch (phase) {
    case HUMAN:
      if (now - lastTouch >= INTERACTIVE_TIMEOUT_MS) {
        status(KEYPAD_TIMEOUT, C_DIM);
        phase = TIMEOUT;
        at = now + 1800;
      }
      break;
    case AI_WAIT:
      if ((int32_t)(now - at) >= 0) {
        int i = aiMove();
        board[i] = 2;
        drawCell(i, 0);
        int line, w = winner(board, &line);
        if (w || full(board)) {
          finish(w, line, now);
        } else {
          status("YOUR MOVE, HUMAN.", C_GREEN);
          phase = HUMAN;
          lastTouch = now;
        }
      }
      break;
    case OVER:
    case TIMEOUT:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  return true;
}

}  // namespace

const Scene SCENE_TICTACTOE = {"tic-tac-toe", enter, tick, touch, nullptr, nullptr};
