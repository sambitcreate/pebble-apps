# Pebble development is thriving again in 2026

**Pebble smartwatch development has experienced a remarkable renaissance.** After Pebble Technology shut down in 2016 and its servers went dark in 2018, the community-driven Rebble project kept the ecosystem alive — and then Google open-sourced the entire PebbleOS in January 2025. Original Pebble founder Eric Migicovsky launched Core Devices to ship new Pebble hardware, and the SDK has been fully modernized with Python 3 support, a revived CloudPebble IDE, a VS Code extension, and a brand-new JavaScript/TypeScript framework called Alloy. With **15,000+ existing apps**, new watches shipping, and the entire software stack now open source, this is the best time to start Pebble development since 2015.

## The road from shutdown to open-source revival

Fitbit acquired Pebble's intellectual property in December 2016 for roughly **$23 million**, keeping servers running temporarily before shutting them down on **June 30, 2018**. The app store, CloudPebble IDE, dictation services, weather APIs, and firmware distribution all went offline that day. Google then acquired Fitbit for $2.1 billion in January 2021, inheriting Pebble's code.

The Rebble Alliance — a community project launched just two days after Pebble's shutdown announcement — built replacement web services that went live in mid-2018. Rebble Web Services (RWS) replaced authentication, the app store, timeline sync, and firmware distribution. A **$3/month subscription** adds voice dictation (via Google) and weather services. Rebble preserved roughly 15,000 watchfaces and apps from the original store and continues to accept new submissions through its developer portal at dev-portal.rebble.io.

The pivotal moment came on **January 27, 2025**, when Google released PebbleOS source code on GitHub. Migicovsky's new company Core Devices then announced new Pebble hardware: the **Pebble 2 Duo** ($149, B&W e-paper, shipping since July 2025), the **Pebble Time 2** ($225, 64-color display, shipping early 2026), and the **Pebble Round 2** ($199, announced January 2026). As of November 2025, **100% of the Pebble software stack is open source**, including hardware schematics for the new watches.

One important dynamic to understand: a public dispute erupted in November 2025 between the Rebble Foundation (now a Michigan non-profit) and Core Devices over app store control and credit for community contributions. Both parties have expressed commitment to the ecosystem's survival, and the situation has stabilized, but developers should be aware that two parallel but interconnected organizations — Rebble (rebble.io) and Core Devices (repebble.com) — maintain overlapping infrastructure. Apps published through Rebble's developer portal appear in both stores.

## Setting up a development environment takes minutes, not hours

The painful Python 2 / Ubuntu VM setup of the old SDK is gone. The modernized **pebble-tool** (v5.0.29+) runs on **Python 3.10–3.13** and installs cleanly via the `uv` package manager:

```bash
# macOS
brew install node
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool --python 3.13
pebble sdk install latest

# Ubuntu/Debian
sudo apt install python3-pip python3-venv nodejs npm libsdl1.2debian libfdt1
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool --python 3.13
pebble sdk install latest
```

Windows users must work through WSL2 with Ubuntu. The latest SDK version is **4.9.127**, which supports all platforms including the new Pebble Round 2 ("gabbro").

Three browser-based options eliminate local setup entirely. **CloudPebble** has been revived at cloudpebble.repebble.com — a full IDE with built-in emulator, resource management, and compilation. **GitHub Codespaces** at cloud.repebble.com provides VS Code in the browser with the SDK pre-installed. And a **WebAssembly QEMU emulator** at ericmigi.github.io/pebble-qemu-wasm boots real PebbleOS firmware directly in your browser.

For local development, the official **VS Code extension** (coredevices.pebble-vscode) adds run buttons, project scaffolding, and command palette integration. Docker images exist from both Rebble (`rebble/pebble-sdk`) and the community (`bboehmke/pebble-dev`) for containerized builds. The built-in QEMU-based emulator supports all platforms — aplite, basalt, chalk, diorite, emery, and gabbro — letting you test without physical hardware.

Deploying to a real watch works over WiFi through the companion app: enable Developer Mode in settings, note the phone's IP address, and run `pebble install --phone <IP>`. The newer CloudPebble method uses `pebble install --cloudpebble` after authenticating with GitHub.

## Four ways to write Pebble apps, including TypeScript

**C remains the primary language** for Pebble development and targets all platforms. Apps use an event-driven architecture built around `app_event_loop()`, with tick handlers for time updates and layer update callbacks for custom drawing. The C API provides a complete graphics toolkit: primitives for lines, rectangles, circles, arcs, and bitmaps; text rendering with built-in and custom TrueType fonts; a full animation framework with easing curves; and direct framebuffer access for pixel-level manipulation. Memory management is manual — every `_create()` needs a matching `_destroy()`.

**Alloy is the exciting new option**, announced February 2026 as a developer preview. Built on Moddable's XS JavaScript engine, Alloy runs modern **ES2025 JavaScript or TypeScript** natively on the watch. It offers two UI frameworks: Piu (declarative, component-based) and Poco (procedural graphics). Web-standard APIs like `fetch()` and `WebSocket()` are available. Create a project with `pebble new-project --alloy my-app`. Alloy currently supports emery and gabbro platforms only, but this is the path forward for developers who prefer TypeScript over C.

**PebbleKit JS** runs JavaScript on the phone as a companion to watch apps (C or Alloy). It handles web requests, geolocation, and configuration pages — any task requiring internet access or phone capabilities. The code lives in `src/pkjs/index.js` and communicates with the watch via AppMessage dictionaries.

**Rocky.js** (SDK 4.0) ran JavaScript on the watch using JerryScript with a Canvas-like API, but it was never completed and is now superseded by Alloy. **Pebble.js** was a higher-level framework where JS ran entirely on the phone controlling fixed UI elements on the watch — legacy and limited, but simple for basic apps.

## Building watchfaces means working within tight, creative constraints

Every Pebble model presents distinct hardware boundaries that shape design decisions:

| Platform | Models | Resolution | Colors | App memory limit |
|----------|--------|-----------|--------|-----------------|
| Aplite | Pebble Classic/Steel | 144 × 168 | 2 (B&W) | **24 KB** |
| Basalt | Pebble Time/Time Steel | 144 × 168 | 64 | **64 KB** |
| Chalk | Pebble Time Round | 180 × 180 (round) | 64 | **64 KB** |
| Diorite | Pebble 2 | 144 × 168 | 2 (B&W) | **64 KB** |
| Emery | Pebble Time 2 | 200 × 228 | 64 | **128 KB** |

The 64-color palette on color platforms uses **2 bits per ARGB channel** — four levels per channel yielding 64 possible colors. Platform-conditional macros like `PBL_IF_COLOR_ELSE()` and `PBL_IF_ROUND_ELSE()` let you target multiple form factors from a single codebase. Resource files use filename tags (`image~color.png`, `image~bw.png`, `image~round.png`) for platform-specific assets.

A watchface's core is the `TickTimerService`. Subscribe to `MINUTE_UNIT` for battery efficiency (the single biggest battery impact), only dropping to `SECOND_UNIT` when displaying seconds. Custom drawing happens in `LayerUpdateProc` callbacks where you receive a `GContext` for all rendering operations. Call `layer_mark_dirty()` only when content actually changes. The animation framework supports property animations with easing curves, sequences, parallel spawning, and even APNG-based bitmap sequences.

For getting started quickly, watchface-generator.de lets you create watchfaces without code — pick a background, clock style, and indicators, then download a .pbw file. For code-based development, the pebble_watchface_framework repo on GitHub provides a bare-bones template supporting all platforms with configuration via Rebble Clay. CloudPebble offers the fastest path from idea to running watchface: build and test entirely in the browser.

## Games on a wristwatch demand clever engineering

Pebble has no traditional game loop — it's event-driven. The standard pattern uses **AppTimer** for frame scheduling:

```c
#define FRAME_MS 33  // ~30 FPS target

static void game_loop(void *data) {
    update_game_state();
    layer_mark_dirty(s_game_layer);
    s_timer = app_timer_register(FRAME_MS, game_loop, NULL);
}
```

Achievable frame rates range from **15–30 FPS** depending on drawing complexity, which is sufficient for the e-paper display's refresh characteristics. For turn-based or slower games, the tick timer service at longer intervals (400ms for snake-style games) works well and saves battery.

Four physical buttons provide input: Up, Down, Select (right side), and Back (left side). The SDK supports single clicks, long presses, repeating clicks (minimum 30ms interval), multi-clicks, and raw up/down events. Watchapps get full button access; watchfaces cannot intercept button presses (reserved for system timeline navigation). The **accelerometer** offers tap detection for gesture input or raw data sampling at 10–100 Hz for tilt-based controls.

All Pebble models include a **vibration motor** (LRA on new models for crisper haptics) but no speaker on legacy hardware. Custom vibration patterns with alternating on/off durations provide game feedback. **Persistent storage** saves game state, high scores, and settings — up to 256 bytes per key, surviving reboots.

The community has built impressive games within these constraints. **PebbleQuest** packed a full 3D fantasy RPG into just 25 KB on the original Pebble. Tetris clones, Flappy Bird ports, dungeon crawlers, virtual pets, and accelerometer-based catching games all demonstrate what's possible. The pebble-gbc-graphics library on GitHub provides a Game Boy Color-style graphics engine with sprite rendering, tilemaps, and palette swapping — a solid foundation for retro-style games.

For watch-to-phone communication (leaderboards, web data, configuration), the **AppMessage** API sends key-value dictionaries over Bluetooth. PebbleKit JS on the phone handles HTTP requests, geolocation, and settings pages. PebbleKit libraries for Android (Java/Kotlin) and iOS (Objective-C) enable deeper companion app integration.

## Where to find help and who to follow

The **Rebble Discord** server is the primary real-time hub, with dedicated channels for app development (#app-dev), SDK work (#sdk-dev), and firmware (#firmware-dev). The subreddit **r/pebble** surged in activity after the 2025 revival and remains the main public discussion forum — Eric Migicovsky participates directly in threads. A new **Rebble Forum** launched December 9, 2025, with app-specific threads for feedback and beta testing.

Developer documentation lives at two sites: **developer.rebble.io** (Rebble-maintained archive of original Pebble docs) and **developer.repebble.com** (Core Devices' actively updated docs including Alloy tutorials). The free textbook "Learning C with Pebble" at pebble.gitbooks.io covers C fundamentals through the lens of Pebble development. The **pebble-examples** GitHub organization contains dozens of official example projects covering watchfaces, games, weather apps, and communication patterns.

Key people to follow: **Eric Migicovsky** (ericmigi.com/blog) for Core Devices updates, **Katharine Berry** who architected Rebble Web Services, and the **Rebble Foundation Board** whose blog at rebble.io covers ecosystem governance and technical progress.

## Alternatives worth considering alongside Pebble

**Bangle.js 2** (~$100) is the closest spiritual successor to Pebble's developer experience. It runs JavaScript natively, has an always-on reflective LCD with ~4-week battery life, and offers **600+ open-source apps** installable through a browser-based app loader. Development happens entirely in a Web IDE — write JavaScript, upload wirelessly via Web Bluetooth, no compilation step. For a TypeScript/JavaScript developer, this is the most frictionless path to wrist-based app development available today.

**PineTime** (~$27) offers maximum openness at minimum cost. It runs InfiniTime (C++ on FreeRTOS) on a Nordic nRF52832 with a 240×240 color touchscreen. Development requires embedded C/C++ cross-compilation, and apps compile into firmware rather than loading dynamically — a significant difference from Pebble's model. The Pine64 community is large and active.

**Watchy** (~$70) is an ESP32-based e-ink watch programmed via Arduino IDE. It's more of a maker/tinkerer project than a polished smartwatch — the PCB-as-watch-body design requires a separate case for daily wear, and notification mirroring needs extra work. Best for Arduino enthusiasts who want maximum hardware control.

The **new Pebble watches from Core Devices** are, naturally, the strongest option for anyone committed to the Pebble ecosystem. The Pebble 2 Duo ships now with 30-day battery life, the Pebble Time 2 adds a 64-color display and touchscreen, and all 15,000+ existing PebbleOS apps are compatible.

## Practical gotchas every new developer should know

**Battery aging is real on legacy hardware.** Original Pebble watches from 2013–2016 often deliver 1–2 days instead of the original 5–7 due to degraded batteries. Replacements require careful disassembly. If buying used, the **Pebble 2** and **Pebble Time Steel** tend to be the most durable and available on eBay ($30–$150).

**iOS support is significantly limited.** Even with the new Core Devices companion app, Apple platform restrictions prevent sending texts, replying to notifications, or maintaining background connectivity. Android provides the full Pebble experience. A class-action lawsuit alleges Apple has tightened restrictions since iOS 13.

**Target your platform carefully.** The 24 KB memory limit on aplite (original Pebble) is brutally tight — most new developers should target basalt/diorite (64 KB) or emery (128 KB) instead. Use `PBL_IF_COLOR_ELSE()` and `PBL_IF_ROUND_ELSE()` macros for cross-platform code rather than hardcoding coordinates. Test on all target emulators before deploying.

**Publish through Rebble's developer portal.** Despite the Core/Rebble tensions, apps uploaded to dev-portal.rebble.io currently appear in both stores. The process is straightforward: create a developer account, upload your .pbw with screenshots and metadata, and set visibility to public or unlisted for beta testing. You can also simply share .pbw files directly via GitHub or personal sites.

**Start with CloudPebble for instant gratification.** Rather than setting up a local toolchain, go to cloudpebble.repebble.com, create a new C watchface project, and have something running on the emulator within minutes. Graduate to local development with VS Code and the Pebble extension once you've built confidence with the API. The "Learning C with Pebble" book is the best structured learning resource if C is unfamiliar territory.

## Conclusion

The Pebble ecosystem in 2026 stands at an unusual and exciting inflection point. What was once a dead platform has become one of the most open and hackable smartwatch ecosystems available — **100% open-source firmware**, modernized development tools supporting C and TypeScript, new hardware shipping, and an engaged community with decades of institutional knowledge preserved in 15,000+ apps. The Core Devices vs. Rebble Foundation dynamic adds complexity, but both organizations are shipping real work. For a creative technologist comfortable with code, the combination of tight hardware constraints (tiny screens, limited memory, four buttons) and a mature API creates a uniquely satisfying design challenge — constraints that force elegance. Start with CloudPebble and a simple watchface, explore the pebble-examples repos for patterns, and join the Rebble Discord to plug into a community that has been keeping these watches alive through sheer determination for nearly a decade.
