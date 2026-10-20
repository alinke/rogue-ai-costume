#pragma once
// Agent codes, shared with the landing page (web/halloween-landing.html):
// "6" + a two-digit agent id (00-99) + a check digit, e.g. 6 4 2 c. The page
// hands one out; typed into the kill-switch keypad, the AI greets that agent.
// The codename is worked out from the id the same way on both sides -- keep
// this file and the page's agentFromId() in step.
#include <Arduino.h>

namespace agent {

// The 4-digit code is an agent code (valid check digit): returns the id 0-99, or -1.
inline int parse(const char* code) {
  if (strlen(code) != 4 || code[0] != '6') return -1;
  int d1 = code[1] - '0', d2 = code[2] - '0', c = code[3] - '0';
  if (d1 < 0 || d1 > 9 || d2 < 0 || d2 > 9) return -1;
  return (3 * d1 + 7 * d2 + 6) % 10 == c ? d1 * 10 + d2 : -1;
}

inline int nameA(int id) { return (id * 7) % 12; }
inline int nameB(int id) { return (id * 5 + 3) % 12; }
inline int role(int id) { return (id * 3 + 1) % 10; }
inline int unit(int id) { return 100 + (id * 37) % 900; }

}  // namespace agent
