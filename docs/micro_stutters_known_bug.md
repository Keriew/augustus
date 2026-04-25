# Micro Stutters Known Bug

Snapshot: 2026-04-06

## Current bug
- Brief micro stutters were observed during play.
- The stutters were not yet reproduced under controlled conditions, so this is currently a symptom report rather than a confirmed root cause.

## Reported symptoms
- The game appeared to hitch very briefly at runtime.
- The user suspected the hitches might be intermittent rather than constant.
- The user noted they may have been running other local workload at the same time, so external contention is still a possibility.

## Important clue
- The observed hitches seemed to line up with audio files playing for the first time.
- That raises the possibility of a first-use audio decode/load/cache path causing a one-time stall on the main thread.
- This is only a correlation so far, not a confirmed cause.

## Current uncertainty
- The user was also running a separate model workload while testing, so some or all of the stutter may have been unrelated engine contention.
- Because of that, the audio correlation should be treated as an investigation clue, not as settled diagnosis.

## Most relevant code areas
- Audio playback / loading:
  - `src/sound/`
  - `src/platform/`
- Any first-use asset decode/cache path tied to sound effects or music playback
- Frame pacing / main loop timing:
  - `src/platform/augustus.cpp`
  - `src/platform/renderer.cpp`

## Best current theory
- A first-use audio path may be doing synchronous load, decode, or cache work on the main thread and causing a one-time hitch.
- External machine load may also have contributed, so the issue may be a real engine hitch, an amplified hitch, or a false positive.

## Next debugging direction
1. Re-test without the extra local model workload running.
2. Check whether the hitch only happens on first playback of a given sound effect or music cue.
3. Add lightweight timing around first-use sound load/decode/playback paths.
4. Compare frame pacing before and after the first occurrence of the same audio cue.
