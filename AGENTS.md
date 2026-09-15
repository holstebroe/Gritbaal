# AGENTS.md — Development Guidelines for AI Agents

This repository contains **Gritbaal**, an über-gritty vintage analog synthesizer emulation implemented in C++17 as a `.clap` plugin.

## Core Directives & Constraints

1. **Binary Size Targets:**
   - Keep compiled plugin binary (`gritbaal.clap`) as small as possible: target **< 100 kB**, absolute maximum **1 MB**.
   - Do NOT introduce third-party GUI or DSP libraries. Use C++ standard library, native OS calls (Win32/X11/Cocoa), and CLAP C headers.

2. **Procedural Layered UI Rendering:**
   - UI elements (knobs, panels, borders, switches) must be procedurally generated (no heavy bitmap PNG/JPG embeds).
   - Pre-render static knob layers (background, dial, shadows) to offscreen buffers for fast real-time blitting and rotation.
   - UI primitive rendering must be abstracted cleanly across platform implementations.

3. **DSP Implementation Guidelines:**
   - Consult `docs/vintage_synth_modelleing_compendium.md` for circuit non-linearity modeling (PolyBLEP anti-aliased VCOs, ZDF ladder/MS-20 filters, thermal drift, dynamic power sag).
   - Consult `docs/design_document.md` for signal flow and panel architecture.
   - Consult `docs/implementation_roadmap.md` for roadmap phases and constraints.

4. **Testing Requirements:**
   - Always run `gritbaal_dsp_test` and `gritbaal_gui_test` binaries after changing DSP or GUI code to verify compilation and prevent regressions.
