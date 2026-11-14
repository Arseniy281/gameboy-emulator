# Phase 0 Research: gb-cpu-core

**Date**: 2026-10-22
**Spec**: specs/001-gb-cpu-core/spec.md

## Decisions

### HALT Bug Implementation

- **Decision**: Implement HALT bug behavior now.
- **Rationale**: Improves timing fidelity and prevents edge-case failures in tests
  that rely on the behavior when IME=0 and IF has a pending but disabled interrupt.
- **Alternatives considered**: Defer implementation (risk failing edge tests);
  feature flag (adds complexity and risks determinism if toggled).

### LCD STAT Interrupt Inclusion

- **Decision**: Include full LCD STAT interrupt handling (priority level, vector,
  and timing interactions) in this feature.
- **Rationale**: Aligns with documented interrupt priority and timing-sensitive
  behaviors; avoids gaps in timing tests.
- **Alternatives considered**: Defer (risk incomplete timing behavior);
  minimal LY==LYC only (insufficient coverage).

## Notes

- Early-stage debug logging of PC and registers to stderr is acceptable and must
  be disableable; logging must not alter timing when disabled.
