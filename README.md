# Vrui Voice Assistant (v2, in progress)

An offline, in-VR voice assistant for [Vrui](https://vrui-vr.github.io/), implemented
in C++ and installed as three Vrui plug-ins: a vislet (the assistant + its on-screen
status orb) and two push-to-talk tools (command mode and number-dictation mode).

**Status: under construction.** This is a from-scratch retyping of the working
assistant at [`../vrui-assistant`](../vrui-assistant); every file under
`VoiceAssistant/` starts as a blank stub and is being filled in by hand, one piece at a
time. Until that's further along, `../vrui-assistant` is the one that actually runs.

## Loading a tool plug-in (no Vrui.cfg edit needed)

Vislets load via `-vislet <ClassName> ';'`. A Tool plug-in (e.g. `VoiceAssistantTool`,
the push-to-talk button) instead needs `-addToolClass <ClassName>` to show up in
Vrui's tool-selection menu at runtime -- press any unbound button/key and pick it from
that popup to bind it yourself, no editing `/usr/local/etc/Vrui-15.0/Vrui.cfg` required:

```bash
VruiDemoSmall -vislet VoiceAssistant ';' -addToolClass VoiceAssistantTool
```

## Build

```bash
make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make check-deps   # system libraries present?
make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make -j$(nproc)
sudo make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make install
```

Fetch the vendored engine binaries and offline models first (into the source tree and
a per-user directory respectively; no `sudo` needed):

```bash
bash scripts/download_models.sh
```

Then run any Vrui application with the vislet:

```bash
ollama serve &
VruiDemoSmall -vislet VoiceAssistant ';'
```

## Layout

- **[makefile](makefile)** — standalone add-on build against an installed Vrui
  (`VRUI_MAKEDIR`), same pattern as the
  [Kinect 3D Video package](https://github.com/vrui-vr/kinect).
- **[VoiceAssistant/](VoiceAssistant/)** — the C++ vislet + tools, engine bindings,
  the command/entity catalog, and the vendored `third_party/{vosk,piper}` binaries.
- **[BuildRoot/](BuildRoot/)** — `Packages.VoiceAssistant` (package-definition stub).
- **[scripts/](scripts/)** — `download_models.sh`, fetching the offline STT/TTS
  engines and models.
- **`models/`** — gitignored; the runtime default is the per-user
  `~/.local/share/vrui-assistant/models`.

## Notes

There is a local, untracked `guide/` directory alongside this README with the house
style guide, a dependency audit, a build-order plan, and a beginner's glossary of the
project's build tooling — personal reference material for doing this recode, not part
of the project. It won't appear if you clone this repository elsewhere.
