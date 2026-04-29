# Codex Local Mode UI State Design

## Summary

Clarify and separate two independent states in the proxy page:

- local proxy service state: whether the HTTP service is listening on the configured port
- Codex local proxy mode: whether `~/.codex/config.toml` and `auth.json` currently point Codex at the local proxy

The UI switch for Codex local mode must reflect the actual Codex profile state, not only the app's saved config value.

## Requirements

- Rename the existing stealth-mode wording to `Codex uses local proxy mode`.
- Keep `Start Service` / `Stop Service` limited to the local HTTP service.
- Make the Codex local mode switch checked when the CAS-managed block is present in `~/.codex/config.toml`.
- Turning the switch on writes the local proxy config.
- Turning the switch off restores the original official Codex config/auth from the CAS backup.
- Default displayed proxy port should be `11480`, matching the current Codex local mode workflow.

## Acceptance

- Stopping the service no longer implies Codex config is restored.
- Turning off Codex local mode restores official config/auth even if the saved app config was out of sync.
- UI state updates after async write/restore completes.
