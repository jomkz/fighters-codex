# Architecture Decision Records

Engineering decisions about the toolkit (`fx_lib`, `fx`, `fx-gui`) that are not
self-evident from the code get a short, numbered decision record here. RE facts about
the game belong in [the FA knowledge base](../fa/README.md), not here; codec-level
one-way decisions stay in spec front-matter (`codec.rationale`, see
[spec-authoring.md](../spec-authoring.md)).

## When to write one

Write an ADR when a decision (a) affects more than one component or an external
contract (dependencies, platforms, artifact shape), (b) overturns or amends a
previously documented policy, or (c) chooses between alternatives that a future
reader would reasonably re-litigate without the context.

## Conventions

- Files are numbered `NNNN-kebab-title.md` in decision order and listed in the index
  below and in the site nav (`mkdocs.yml`).
- **Status** is one of `Proposed`, `Accepted`, or `Superseded by ADR-NNNN`. Accepted
  ADRs are immutable except for status changes and link fixes — a changed decision
  gets a new ADR that supersedes the old one.
- Each record uses the template: **Status / Context / Options considered / Decision /
  Consequences**, and names the issues it serves.

## Index

| ADR | Title | Status |
|-----|-------|--------|
| [0001](0001-fx-gui-sdl3-opengl3-miniaudio.md) | fx-gui cross-platform backend — SDL3 + OpenGL 3.3 + miniaudio | Accepted |
| [0002](0002-fxe-clean-room-source-port.md) | fxe — committed, generated, clean-room source port of the game executable | Accepted |
