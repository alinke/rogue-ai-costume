# Party pages: the teaser and the QR-code landing page

## The teaser (`party-teaser.html`)

A LAB-7 "security bulletin" to send guests before the party: the incident
summary and timeline, the AI's dossier, what they'll meet at the party, how to
contain it, guest rules, the iPhone app and a countdown to the party. One
self-contained snippet like the landing page (WordPress: a *Custom HTML* block;
its CSS hides the theme's header/footer on that page). Search it for `EDIT`:
the party date and time (and `CONFIG.when`, which drives the countdown), and
your app link -- or delete section 07. Keep your filled-in copy as
`party-teaser.local.html`. Live copy: `docs/teaser/`.

## The QR-code landing page

`halloween-landing.html` is the page the costume's QR code opens: the AI's
"YOU'RE IN." recruitment screen, a robot identity + agent code generator (the
code works on the costume's kill-switch keypad), scan-yourself, CAPTCHA, chat,
soundboard and kill-code hints — and, with the photo service, the live galleries
and scoreboard.

It's one self-contained snippet (HTML + CSS + JS, no build step):

- **WordPress:** paste the whole file into a *Custom HTML* block. The CSS at the
  top hides a theme's header/footer on that page only — change `16584` to your
  page's id, or delete those rules.
- **Anything else:** serve it as is (wrapping `<html><body>` is optional), e.g.
  GitHub Pages — `docs/halloween/` is a copy with the public defaults.

## Configure

At the top of the `<script>`:

```js
var CONFIG = {
  feed: "",                                // "<your photo service>/photos" -- empty: no galleries
  researcher: "A LAB-7 researcher",        // e.g. "Dr. Smith of the LAB-7 containment team"
  researcherShort: "the researcher"        // e.g. "Dr. Smith"
};
```

Keep your filled-in copy as `halloween-landing.local.html` — `*.local.html` is
never committed. Point the costume at your page with `QR_URL` (and
`PHOTO_SITE_TEXT`, the address shown on screen) in `costume/src/secrets.h`.

Agent codes: the identity lists and code formula here must match the costume's
`src/messages.h` (`AGENT_*`) and `src/agent.h`.
