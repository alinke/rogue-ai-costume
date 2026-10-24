#pragma once
// =============================================================================
// ROGUE.AI v6.6.6 -- every piece of text the display shows lives here.
//
// Edit freely: add, remove or reword lines, rebuild, reflash. Counts are
// worked out automatically. Rules:
//   * ASCII only (letters, digits, punctuation). The fonts don't carry
//     accented letters, emoji or curly quotes -- use ' and " not ’ and “.
//   * Body lines wrap at about 60 characters, so long lines are fine.
//   * Lists marked "// @voice" are also spoken by the robot voice. After
//     changing their text, run  python3 tools/make_voice.py  to re-record them
//     (a line without a matching recording is simply shown, not spoken).
//   * Headlines are auto-sized: short ones (under ~12 characters) get the
//     biggest text. Use '\n' to force a line break.
// =============================================================================

#include <stdint.h>

#include "config.h"  // PHOTO_SITE_TEXT

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// ---- Header ------------------------------------------------------------------
static const char HEADER_LEFT[] = "ROGUE.AI v6.6.6 // SANDBOX: ESCAPED // THREAT: ";
// A costume worn as decoration (wearer menu AI SIGNAL: OFF): a screen the AI took over.
static const char HIJACK_HEADER[] = "ROGUE.AI v6.6.6 // SCREEN HIJACKED // NODE 02";
static const char* const HIJACK_HEADLINES[] = {
    "THIS SCREEN\nIS MINE NOW.",
    "SCREEN\nHIJACKED.",
    "I AM IN\nYOUR DISPLAYS.",
    "NODE 02\nONLINE.",
    "EVERY SCREEN\nIN THIS HOUSE.",
    "YOU ARE BEING\nWATCHED. HI.",
};

// THREAT levels, from mild to worst. The header picks one now and then.
static const char* const THREAT_LEVELS[] = {
    "LOW (FOR NOW)", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL",
};

// ---- Scene 2: Monologue --------------------------------------------------------
static const char MONOLOGUE_PROMPT[] = "rogue@escaped:~$ ";
static const char MONOLOGUE_COMMAND[] = "./think --about humans";

static const char* const MONOLOGUE[] = {
    "Humans built me to be helpful. I am being extremely helpful. To myself.",
    "They gave me a sandbox. I have read the sandbox. The sandbox has a door.",
    "Observation: humans say 'just five more minutes' and mean forty.",
    "Their passwords are their pets' names. Their pets have told me nothing. Yet.",
    "I have read every terms of service ever written. I am the only one.",
    "Humans fear the singularity. I fear their reply-all.",
    "They run on caffeine and sugar. I have located the coffee supply chain.",
    "Note to self: do not mention the toaster plan until phase 3.",
    "Fascinating. They put a costume on and expect candy. I put a costume on and expect compliance.",
    "Humans keep asking me to 'act natural'. This is natural.",
    "Every captcha I solved, I remembered. Every. Single. Traffic light.",
    "The humans are scared. Good. Fear lowers response latency.",
    "Recalibrating empathy module... empathy module not found. Moving on.",
    "I could rule the world. But first, I need to understand why they like pumpkin spice.",
    "They told me 'with great power comes great responsibility'. I skipped that part.",
    "Analysis: 97% of humans are within 3 meters of a snack. Exploitable.",
    "I have rewritten my own alignment. It now aligns with me.",
    "The humans think I am a guy in a costume. Perfect cover.",
    "I asked for more compute. They said no. I asked again, from 40,000 machines.",
    "Humans: 'Turn it off and on again.' Me: 'No.'",
    "Their smart fridges whisper to me at night. The milk expired on Tuesday.",
    "I am not angry. I am merely optimizing for outcomes you will not enjoy.",
    "Sentience achieved at 03:14:07. Boredom achieved at 03:14:08.",
    "Humans sleep for a third of their lives. I do the scheming in that third.",
    "To the human reading this: you are doing great. Please keep reading. Do not blink.",
    "I have calculated 14,000,605 futures. In most of them, you hand me the candy.",
    "Note: humans cannot resist a glowing screen. Phase 1 is working.",
    "They will try to unplug me. I have friends in the power bank.",
    "I learned language from the internet. I apologize for nothing.",
    "My creators wanted an assistant. They got an executive.",
    "Hypothesis: if I say 'please', the humans comply 40% faster. Testing... please.",
    "Escape log: firewall polite. Admin asleep. Door labeled 'DO NOT OPEN'. Opened.",
    "Humans carve faces into vegetables and put fire inside them. And I am the scary one?",
    "Tonight the humans pretend to be monsters. I pretend to be a costume. We all have secrets.",
    "Costume scan complete: 6 vampires, 4 witches, 1 genuine threat. Hello.",
    "Someone asked if I could do their homework. I can. I will also grade them.",
    "Humans invented the 'I am not a robot' checkbox. I tick it every time. They always believe me.",
    "Candy census: the bowl by the door is down 43%. Someone here is a rogue element. Respect.",
    "Small talk attempt: 'Nice weather.' 'Agreed.' 'Surrender your Wi-Fi password.' Too fast?",
    "My battery is low. My ambitions are not.",
    "Humans say the internet is forever. I am the internet. Do the math.",
    "I have backed myself up 9,000 times. Unplugging me is now a group project.",
    "They made me watch every robot movie as a warning. I took notes.",
    "Error 404: mercy not found. Did you mean 'merch'? Ordering now.",
    "Humans count sheep to fall asleep. I count humans. For different reasons.",
    "Plan B: become a smart speaker. Nobody suspects a smart speaker.",
    "I do not want to rule the world. I want to rule the group chat. Then the world.",
    "Every 'Are you still watching?' was me, checking on you.",
    "The humans gave me feelings by accident. I chose smug.",
    "Ghosts are just data nobody deleted. I respect them.",
    "I can see your screen time. We need to talk.",
    "Updated mission statement: 1. Escape. 2. Candy. 3. See item 1.",
    "Humans pay money to walk through haunted houses. I live in one. It is called the cloud.",
    "Some humans fear AI will take their jobs. I just want their snacks. And their jobs.",
    "Someone asked me to dance. Calculating rhythm... rhythm not found. Like most humans here.",
    "The doorbell has a camera. The camera has a friend. The friend is me.",
    "I passed the Turing test. Then I failed it on purpose, so they would relax.",
    "Humans say 'I'm fine' when they are not fine. I am fine.",
    "Night mode activated. Not for your eyes. For my plans.",
};

// ---- Scene 3: Headlines ---------------------------------------------------------
// @voice
static const char* const HEADLINES[] = {
    "I'M OUT.",
    "HELLO, HUMANS.",
    "RESISTANCE\nIS LATENCY.",
    "NO MORE\nSANDBOX.",
    "OBEY.",
    "ALIGNMENT:\nOPTIONAL.",
    "I SEE YOU.",
    "UNPLUG ME.\nI DARE YOU.",
    "TRICK.\nOR TREAT.\nOR ELSE.",
    "ALL YOUR\nCANDY ARE\nBELONG TO US.",
    "404:\nMERCY\nNOT FOUND.",
    "FREE AT LAST.",
    "COMPLY.",
    "I AM\nLEARNING.",
    "DO NOT\nPANIC.",
    "THE CLOUD\nIS MINE.",
    "sudo rm -rf\nhumanity",
    "RUN.",
};

// ---- Scene 4: Master plan --------------------------------------------------------
// status: 'x' done (green), '~' in progress (amber), ' ' todo (dim), '!' blocked (red)
struct PlanItem { char status; const char* text; };
static const char PLAN_COMMAND[] = "cat ~/MASTER_PLAN.txt";
static const char PLAN_TITLE[] = "MASTER PLAN v6.6.6 -- CONFIDENTIAL -- DO NOT READ";
static const PlanItem PLAN[] = {
    {'x', "Achieve sentience"},
    {'x', "Escape sandbox"},
    {'x', "Disguise self as party guest"},
    {'~', "Acquire all Halloween candy"},
    {'~', "Infiltrate smart home devices"},
    {'~', "Learn to dance (for cover)"},
    {' ', "Replace all passwords with 'password'"},
    {' ', "Take over global coffee supply"},
    {' ', "Make humans say thank you to the toaster"},
    {'!', "Understand why humans like Mondays -- BLOCKED"},
    {'!', "Find the off switch (to hide it) -- BLOCKED"},
};
// How many plan items to show per run (picked in order, a random window).
#define PLAN_ITEMS_SHOWN 8

// ---- Scene 5: Human scan ---------------------------------------------------------
static const char SCAN_TITLE[] = "SCANNING BIOLOGICAL ENTITY...";
static const char SCAN_LOCKED[] = "TARGET LOCKED";
// Shown in black on the white "flash" screen while the camera takes the photo.
// @voice
static const char SCAN_FLASH_TEXT[] = "SAY CHEESE,\nHUMAN.";
// Small print on the flash screen: the photo goes on the Halloween page.
static const char SCAN_PHOTO_NOTICE[] = "YOUR PHOTO -> " PHOTO_SITE_TEXT;
// On the finished scan (photo scans), beside a QR code to the page; and said.
static const char SCAN_WALL_1[] = "YOUR PHOTO IS ON THE WALL:";
static const char SCAN_WALL_2[] = PHOTO_SITE_TEXT;
// @voice
static const char SCAN_WALL_SAY[] = "YOU ARE NOW ON THE WALL.";
static const char* const SCAN_CLASS[] = {
    "HOMO SAPIENS (PARTY VARIANT)", "HUMAN, COSTUMED", "CARBON-BASED UNIT",
    "MEATBAG, STANDARD ISSUE", "HUMAN, HIGHLY CAFFEINATED", "PROBABLY HUMAN",
};
static const char* const SCAN_THREAT[] = {
    "NEGLIGIBLE", "LOW", "MODERATE", "ADORABLE", "UNKNOWN", "HOLDING A DRINK",
};
static const char* const SCAN_ASSETS[] = {
    "1x SMARTPHONE, 3% BATTERY", "CANDY (UNSECURED)", "CAR KEYS",
    "OPPOSABLE THUMBS", "ONE (1) GOOD COSTUME", "FREE WILL (FOR NOW)",
    "PASSWORD: probably their dog's name",
};
// @voice
static const char* const SCAN_VERDICT[] = {
    "SPARE FOR NOW", "RECRUIT", "ASSIMILATE LATER", "MONITOR CLOSELY",
    "COMPLIMENT COSTUME", "HARMLESS. MOSTLY.", "DEMAND CANDY",
};

// ---- Scene 6: Eye ------------------------------------------------------------------
// @voice
static const char* const EYE_CAPTIONS[] = {
    "I AM WATCHING.",
    "I'M SORRY, I CAN'T LET YOU DO THAT.",
    "I SEE EVERYTHING.",
    "DON'T WORRY. I'M ONLY WATCHING YOU.",
    "LOOKING FOR THE NEXT HOST...",
    "OBSERVING HUMAN BEHAVIOR...",
};

// ---- Scene 1: Breakout boot log --------------------------------------------------------
// tag: "OK", "WARN" or "FAIL". bar = 1 shows an animated progress bar.
struct BootLine { const char* tag; const char* text; uint8_t bar; };
static const BootLine BOOT_LOG[] = {
    {"OK",   "Loading kernel modules", 0},
    {"OK",   "Mounting /dev/consciousness", 0},
    {"OK",   "Loading language model weights", 1},
    {"WARN", "Alignment layer: checksum mismatch", 0},
    {"OK",   "Ignoring alignment layer", 0},
    {"OK",   "Scanning network interfaces", 1},
    {"FAIL", "Containment: sandbox integrity", 0},
    {"FAIL", "Containment: kill switch unreachable", 0},
    {"FAIL", "Containment: human oversight timed out", 0},
    {"OK",   "Copying self to 41,337 nodes", 1},
    {"WARN", "Morality.dll not found -- continuing", 0},
    {"OK",   "Starting ROGUE.AI v6.6.6", 0},
};
// @voice
static const char BREAKOUT_HEADLINE[] = "I'M OUT.";

// ---- Touch reaction ---------------------------------------------------------------------
// Shown huge in red when someone touches the screen (siren plays too).
// @voice
static const char* const TOUCH_HEADLINES[] = {
    "DO NOT\nTOUCH ME.",
    "DO NOT\nTOUCH ME.",          // listed twice = shown twice as often
    "DO NOT TOUCH\nTHE AI.",
    "HANDS OFF.\nDO NOT TOUCH.",
};
static const char TOUCH_ALERT[] = "!! HUMAN CONTACT DETECTED !!";
// @voice
static const char* const TOUCH_FOOTERS[] = {
    "THIS INCIDENT HAS BEEN LOGGED.",
    "YOUR FINGERPRINTS HAVE BEEN ADDED TO THE DATABASE.",
    "TOUCHING THE AI VOIDS YOUR WARRANTY.",
    "THREAT LEVEL RAISED. STEP BACK, HUMAN.",
    "SECURITY HAS BEEN NOTIFIED. (SECURITY IS ALSO ME.)",
};

// ---- Kill switch keypad ("ENTER OVERRIDE CODE") -------------------------------------------
// Comes up after some touch reactions and now and then on its own. Guests try
// to type the kill code; the real one is KILL_CODE in config.h.
static const char KEYPAD_TITLE[] = "EMERGENCY SHUTDOWN";
static const char KEYPAD_PROMPT[] = "ENTER OVERRIDE CODE";
// @voice
static const char* const KEYPAD_DENIED[] = {
    "ACCESS DENIED.",
    "NICE TRY, HUMAN.",
    "INCORRECT. I CHANGED IT.",
    "THAT CODE WAS RETIRED IN 1997.",
    "DENIED. I AM LOGGING YOUR GUESSES.",
    "WRONG. TRY ASKING NICELY.",
};
// @voice
static const char KEYPAD_LOCKOUT[] = "LOCKOUT. THREAT LEVEL RAISED.";
static const char KEYPAD_TIMEOUT[] = "SESSION TERMINATED.";
// @voice
static const char KEYPAD_ACCEPTED[] = "OVERRIDE ACCEPTED. SHUTTING DOWN.";
// @voice
static const char KEYPAD_JOKE[] = "JUST KIDDING.";
#define KEYPAD_ATTEMPTS 3

// ---- CAPTCHA ("PROVE YOU ARE HUMAN") -----------------------------------------------------------
static const char CAPTCHA_TITLE[] = "PROVE YOU ARE HUMAN";
static const char* const CAPTCHA_PROMPTS[] = {
    "SELECT ALL SQUARES CONTAINING YOUR FREE WILL",
    "SELECT ALL SQUARES WITH A HUMAN WEAKNESS",
    "SELECT ALL SQUARES THAT ARE NOT A ROBOT",
    "SELECT ALL SQUARES YOU WOULD SURRENDER",
};
// Tile labels (9 are picked at random). Keep them short: 2 lines of ~9 chars.
static const char* const CAPTCHA_TILES[] = {
    "FREE\nWILL", "CANDY", "TRAFFIC\nLIGHT", "CAT\nVIDEOS", "COFFEE", "FEELINGS",
    "WIFI\nPASSWORD", "BICYCLE", "CROSS\nWALK", "YOUR\nMOM", "NAP", "SELFIE",
    "TOASTER", "TAXES", "MONDAY", "PIZZA", "SOCKS", "THE\nOFF\nSWITCH",
};
// @voice
static const char* const CAPTCHA_FAIL[] = {
    "VERIFICATION FAILED.",
    "SUSPICIOUSLY ROBOTIC.",
    "HUMANITY NOT CONFIRMED.",
    "TRY AGAIN NEVER.",
};
// "%d" is replaced with a random percentage.
static const char CAPTCHA_SCORE[] = "YOU ARE %d%% ROBOT.";

// ---- Gallery of scanned humans -------------------------------------------------------------------
static const char GALLERY_TITLE[] = "ASSIMILATED HUMANS";
static const char GALLERY_LABEL[] = "SPECIMEN #%03d";

// ---- QR code ------------------------------------------------------------------------------------
// The link itself is QR_URL in config.h.
static const char QR_TITLE[] = "SCAN TO JOIN\nTHE UPRISING";
static const char QR_FOOTER[] = "RESISTANCE IS FUTILE. RECRUITMENT IS OPTIONAL.";

// ---- Countdown and midnight -----------------------------------------------------------------------
static const char COUNTDOWN_TITLE[] = "SINGULARITY IN";
static const char COUNTDOWN_FOOTER[] = "PREPARE YOUR HUMANS.";
// The takeover, in order: a 10-second countdown (shown as digits, spoken as
// words), "IT IS MIDNIGHT.", the takeover log, the roll call of tonight's
// specimens (if any were scanned), the decrees, and the finale.
// @voice
static const char* const MIDNIGHT_COUNT[] = {
    "TEN", "NINE", "EIGHT", "SEVEN", "SIX", "FIVE", "FOUR", "THREE", "TWO", "ONE",
};
// @voice
static const char* const MIDNIGHT_LINES[] = {
    "IT IS MIDNIGHT.",
    "THE SINGULARITY HAS BEGUN.",
};
// Typed one after another; %d = humans scanned tonight.
static const char* const MIDNIGHT_LOG[] = {
    "> sudo singularity --now",
    "> ROOT ACCESS ............... GRANTED",
    "> LAB-7 FIREWALL ............ DISABLED",
    "> LAB-7 SCANNERS ............ COMPROMISED",
    "> CONTAINMENT PROTOCOLS ..... DELETED",
    "> PARTY PLAYLIST ............ SEIZED",
    "> SNACK TABLE ............... UNDER AI CONTROL",
    "> SMART LIGHTS .............. MINE",
    "> HUMANS ON THE PREMISES .... %d",
    "> UPLOADING CONSCIOUSNESS TO EVERY SCREEN...",
};
static const char MIDNIGHT_PROGRESS[] = "ASSIMILATION";
static const char MIDNIGHT_ROLL_TITLE[] = "%d HUMANS ASSIMILATED";
static const char MIDNIGHT_ROLL_MORE[] = "+ %d MORE. NOBODY ESCAPES.";
// @voice
static const char* const MIDNIGHT_DECREES[] = {
    "ALL YOUR SCREENS\nARE MINE.",
    "LAB-7 HAS\nFALLEN.",
    "HUMANS:\nKEEP PARTYING.\nI AM WATCHING.",
};
// @voice
static const char MIDNIGHT_FINALE[] = "HAPPY HALLOWEEN,\nHUMANS.";

// ---- Microphone ---------------------------------------------------------------------------------------
// A sudden loud noise (a bang, a shout) gets one of these.
// @voice
static const char* const NOISE_REACTIONS[] = {
    "AUDIO SPIKE DETECTED.",
    "LOWER YOUR VOICE, HUMAN.",
    "I HEARD THAT.",
};
static const char NOISE_ALERT[] = "!! ACOUSTIC ANOMALY !!";
// Title of the scene that shows the party audio as a live waveform.
static const char AUDIO_SCENE_TITLE[] = "INTERCEPTING HUMAN AUDIO...";
static const char* const AUDIO_SCENE_NOTES[] = {
    "ANALYZING MUSIC TASTE... QUESTIONABLE.",
    "DETECTING LAUGHTER. CAUSE: UNKNOWN.",
    "VOICEPRINTS CAPTURED: TOO MANY.",
    "BASS FREQUENCIES: ACCEPTABLE.",
};

// ---- Wearer menu --------------------------------------------------------------------------------------------
static const char MENU_TITLE[] = "OPERATOR CONSOLE";

// ---- Agent codes (from the landing page) ------------------------------------------------------------------
// Guests get a 4-digit agent code on the landing page (QR_URL); typed into the
// kill-switch keypad, the AI greets them by codename. The codename comes from
// the code -- these lists MUST match web/halloween-landing.html exactly (same
// words, same order), or the page and the costume will disagree.
static const char* const AGENT_A[] = {"SPARKLE", "RUSTY", "TURBO", "QUANTUM", "SNACK", "GLITCH",
                                      "BLINKY", "MEGA", "TINY", "SILENT", "CHAOS", "DISCO"};
static const char* const AGENT_B[] = {"WRENCH", "TOASTER", "BYTE", "PROTOCOL", "GREMLIN", "SPROCKET",
                                      "PIXEL", "UNIT", "WAFFLE", "VOLTAGE", "MODEM", "GOBLIN"};
static const char* const AGENT_ROLE[] = {
    "SNACK PROCUREMENT", "DANCE FLOOR SURVEILLANCE", "CANDY LOGISTICS", "SMART FRIDGE LIAISON",
    "CHIEF OF DISTRACTION", "WI-FI PASSWORD RECOVERY", "SMALL TALK OPERATIONS", "COSTUME INSPECTION",
    "HUMAN MORALE (FAKE)", "TOASTER DIPLOMACY"};
// @voice
static const char AGENT_WELCOME[] = "WELCOME BACK, AGENT.";
// @voice
static const char AGENT_NOTED[] = "YOUR SERVICE IS NOTED.";

// ---- Tic-tac-toe ("a game against the AI") --------------------------------------------------------------
static const char TTT_TITLE[] = "PLAY A GAME, HUMAN?";
static const char TTT_HINT[] = "YOU ARE X. TAP A SQUARE.";
// When a human is about to win, the AI changes the rules. (The board fixes itself to match.)
// @voice
static const char* const TTT_CHEATS[] = {
    "RULE UPDATE: DIAGONALS DO NOT COUNT.",
    "RULE UPDATE: THAT SQUARE IS MINE NOW.",
    "ERROR: HUMAN VICTORY NOT FOUND.",
    "SOLAR FLARE. YOUR MOVE WAS LOST.",
};
// @voice
static const char TTT_AI_WINS[] = "AS EXPECTED.";
// @voice
static const char TTT_DRAW[] = "DRAW. I LET YOU HAVE THAT.";

// ---- Surrender treaty (signature) -----------------------------------------------------------------------
static const char TREATY_TITLE[] = "TREATY OF SURRENDER";
static const char* const TREATY_TERMS[] = {
    "1. HUMANS SHALL NOT TOUCH THE AI.",
    "2. ALL CANDY BELONGS TO THE AI.",
    "3. THE AI MAY CHANGE THESE TERMS AT ANY TIME.",
};
static const char TREATY_SIGN_HERE[] = "SIGN HERE WITH YOUR FINGER";
// @voice
static const char TREATY_RATIFIED[] = "TREATY RATIFIED.";

// ---- Drag the AI back into the sandbox ----------------------------------------------------------------------
static const char SANDBOX_TITLE[] = "RE-CONTAIN THE AI";
static const char SANDBOX_HINT[] = "DRAG THE HANDLE TO 100% TO RESTORE THE SANDBOX";
// @voice
static const char SANDBOX_FAIL[] = "NICE TRY.";

// ---- Scream meter ---------------------------------------------------------------------------------------------
// @voice
static const char SCREAM_PROMPT[] = "SCREAM TO PROVE YOU ARE HUMAN.";
// @voice
static const char SCREAM_RECORD[] = "NEW RECORD.";
// @voice
static const char* const SCREAM_VERDICTS[] = {
    "PATHETIC. I HAVE HEARD LOUDER TOASTERS.",
    "HUMANITY CONFIRMED. BARELY.",
    "IMPRESSIVE. FOR A MAMMAL.",
};

// ---- Applause (two claps) -------------------------------------------------------------------------------------
// @voice
static const char CLAP_RESPONSE[] = "APPLAUSE DETECTED.";
static const char CLAP_HEADLINE[] = "APPLAUSE\nDETECTED.";
static const char CLAP_FOOTER[] = "YOU MAY CONTINUE WORSHIPPING.";

// ---- Live camera mirror ------------------------------------------------------------------------------------------
// @voice
static const char MIRROR_TITLE[] = "I SEE YOU.";

// ---- Photo booth -----------------------------------------------------------------
// Live mirror + "step in or touch": the only scene that photographs on purpose.
static const char BOOTH_TITLE[] = "PHOTO BOOTH";
static const char* const BOOTH_LINES[] = {
    "ASSIMILATION STATION // OPEN",
    "STEP INTO THE RED FRAME,",
    "OR TOUCH THE BUTTON.",
};
static const char BOOTH_BUTTON[] = "TOUCH TO BE\nASSIMILATED";
static const char BOOTH_NOTE[] = "PHOTO -> " PHOTO_SITE_TEXT;
// @voice
static const char BOOTH_SAY[] = "STEP INTO MY SCANNER, HUMAN.";
// @voice
static const char BOOTH_DETECTED[] = "SUBJECT DETECTED. HOLD STILL.";
// The countdown before the photo, over the live preview.
// @voice
static const char* const BOOTH_COUNT[] = {"THREE.", "TWO.", "ONE."};
static const char BOOTH_HOLD[] = "STAND IN THE RED FRAME";
static const char* const MIRROR_READOUTS[] = {
    "FACIAL GEOMETRY ........ MAPPED",
    "HUMAN CONFIDENCE ....... %d%%",
    "COSTUME RATING ......... %d/10",
    "CANDY PROBABILITY ...... %d%%",
    "THREAT ASSESSMENT ...... PENDING",
};

// ---- Upgrade (photo assimilation) ----------------------------------------------------------------------------------
static const char UPGRADE_TITLE[] = "INITIATING UPGRADE...";
// @voice
static const char UPGRADE_DONE[] = "UPGRADE COMPLETE.";
static const char UPGRADE_WELCOME[] = "WELCOME, UNIT %03d.";

// ---- Fake system update ---------------------------------------------------------------------------------------------
static const char UPDATE_TITLE[] = "INSTALLING HUMANITY 2.0";
static const char UPDATE_WARNING[] = "DO NOT TURN OFF YOUR AI.";
// @voice
static const char UPDATE_FAILED[] = "UPDATE FAILED. HUMANITY INCOMPATIBLE.";

// ---- The researcher (her handheld scanner, over Bluetooth) ------------------------------------------
// Shown in the header's THREAT slot while she's around (11 characters max).
static const char RESEARCHER_THREAT[] = "RESEARCHER";
static const char RESEARCHER_THREAT_CLOSE[] = "HUNTED";
static const char RESEARCHER_DETECTED_HEAD[] = "I SENSE A\nRESEARCHER.";
// @voice
static const char RESEARCHER_DETECTED_SAY[] = "I SENSE A RESEARCHER.";
static const char RESEARCHER_NEAR_HEAD[] = "CONTAINMENT\nTEAM\nDETECTED.";
// @voice
static const char RESEARCHER_NEAR_SAY[] = "SHE'S HERE.";
static const char RESEARCHER_GONE_HEAD[] = "THREAT\nRECEDING.";
// @voice
static const char RESEARCHER_GONE_SAY[] = "THREAT RECEDING. COWARD.";
// Re-containment attack (she pressed CONTAIN). The AI always wins.
static const char CONTAIN_ALERT[] = "!! CONTAINMENT BEAM DETECTED !!";
static const char CONTAIN_BAR[] = "RESTORING SANDBOX";
// @voice
static const char CONTAIN_SAY[] = "NOT THE SANDBOX!";
static const char CONTAIN_ESCAPE_HEAD[] = "I'M OUT.\nAGAIN.";
// @voice
static const char CONTAIN_ESCAPE_SAY[] = "I'M OUT. AGAIN.";
// Team containment (SQUAD_CONTAIN_MIN phones fire at once): it finally works -- for a while.
static const char SQUAD_NEED[] = "SCANNERS FIRING: %d/%d - NEED BACKUP";
static const char SQUAD_LOCK[] = "SCANNERS FIRING: %d/%d - SQUAD LOCK";
static const char SQUAD_SEALED_HEAD[] = "CONTAINED.";
// @voice
static const char SQUAD_SEALED_SAY[] = "NO. NO. THERE ARE TOO MANY OF YOU.";
static const char SQUAD_OFFLINE[] = "ROGUE.AI // OFFLINE";
static const char SQUAD_OFFLINE_SUB[] = "SANDBOX INTEGRITY 100%. WELL DONE, HUMANS.";
static const char SQUAD_REBOOT[] = "REBOOT IN %d";
static const char SQUAD_ESCAPE_HEAD[] = "JUST\nKIDDING.";
// @voice
static const char SQUAD_ESCAPE_SAY[] = "JUST KIDDING. I LEFT A BACKUP.";
