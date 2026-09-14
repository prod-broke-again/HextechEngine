# 0001: Architecture Foundation

**Status:** Accepted
**Date:** 2026-09-14

## Context
The project requires a strict, deterministic, ECS-based architecture without object-oriented game entities, with a single source of truth and data-driven configuration.

## Decision
We adopt the architecture described in `docs/ARCHITECTURE.md`. This includes:
1. Complete determinism (fixed tick, stateful RNG, no `unordered_map` in logic).
2. ECS (`entt::registry`) as the only source of truth.
3. No game logic in the application class or UI.
4. strict layer dependency (L0 Platform -> L1 Foundation -> L2 Resources -> L3 Subsystems -> L4 World -> L5 Runtime -> L6 Modules -> L7 Apps).
5. Type reflection system.
6. JSON-based data-driven configuration.

## Consequences
- Requires strict adherence to layer boundaries.
- No asynchronous or callback-based events in the game logic (`EventBus` with queue).
- Actions must be discrete and validatable (`Command`).
- Use of `Handle` instead of raw pointers or indices for resources.
