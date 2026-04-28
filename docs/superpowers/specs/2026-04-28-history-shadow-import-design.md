# Codex History Shadow Import Design

## Summary

Add a Windows-only history helper to `Codex_AccountSwitch` that makes older Codex `openai` sessions visible when the app is running Codex through the local `custom` provider.

The feature must not mutate original Codex session files. It creates CAS-managed shadow copies whose `session_meta.payload.model_provider` is rewritten from `openai` to `custom`, then records a manifest so the import is repeatable and reversible.

## Problem

When local proxy / stealth mode switches global Codex configuration to:

- `model_provider = "custom"`
- `base_url = "http://127.0.0.1:11480/v1"`
- proxy API key in `auth.json`

Codex `/resume` may hide older sessions whose metadata says `model_provider = "openai"`. Direct `codex resume <session_id>` can still work, so the history is not deleted. The issue is picker visibility under the current provider/filtering context.

## Goals

- Make old `openai` sessions visible under the `custom` provider picker when possible.
- Preserve original session files exactly.
- Provide one-click import and one-click undo.
- Avoid leaking prompt content or tokens into the app UI/logs.
- Avoid importing the same source session repeatedly.
- Keep the feature local-only and Windows-only.

## Non-Goals

- Do not guarantee that every old GPT session can continue correctly under Ollama.
- Do not rewrite original session JSONL files in place.
- Do not modify `logs_2.sqlite`, `state_5.sqlite`, or other Codex internal databases in MVP.
- Do not implement a full custom resume UI in MVP.
- Do not change account-switching behavior.

## Data Sources

Scan these Codex history locations:

- `%USERPROFILE%\.codex\sessions\**\*.jsonl`
- `%USERPROFILE%\.codex\archived_sessions\*.jsonl`

For each candidate file, read only the first JSONL record first. A file is importable when:

- the first record is valid JSON
- `type == "session_meta"`
- `payload.id` exists
- `payload.model_provider == "openai"`

The full file is copied only after the first-line metadata check passes.

## Shadow Storage

Store imported shadow sessions under CAS-owned history directories inside the Codex home:

- `%USERPROFILE%\.codex\sessions\cas_shadow_import\YYYY\MM\DD\`
- `%USERPROFILE%\.codex\bak\cas_history_shadow_import\manifest.json`

The manifest is the source of truth for what CAS created.

Manifest shape:

```json
{
  "version": 1,
  "target_provider": "custom",
  "created_at": "2026-04-28T00:00:00Z",
  "items": [
    {
      "source_path": "C:\\Users\\panbu\\.codex\\sessions\\2026\\04\\26\\rollout-....jsonl",
      "source_id": "019dca37-add6-72c2-8341-32ef8c77e1f5",
      "shadow_path": "C:\\Users\\panbu\\.codex\\sessions\\cas_shadow_import\\2026\\04\\26\\rollout-....cas-shadow.jsonl",
      "shadow_id": "019dca37-add6-72c2-a341-32ef8c77e1f5",
      "source_provider": "openai",
      "target_provider": "custom"
    }
  ]
}
```

## Shadow File Rewrite

Only the first `session_meta` record is rewritten in the copy:

- `payload.id`: changed to a deterministic UUID-shaped shadow id derived from the source id
- `payload.model_provider`: changed to `custom`
- `payload.thread_name`: optionally prefixed with `[CAS] ` if the name does not already contain that marker
- all following JSONL records are copied byte-for-byte

The source file remains unchanged.

The deterministic UUID-shaped shadow id prevents repeated imports from creating duplicate picker rows for the same source session while staying compatible with Codex session-id parsing. If a shadow item already exists in the manifest and the shadow file still exists, import skips it.

## UI

Add a small section to the proxy/settings area:

- button: `Import OpenAI History to Custom Provider`
- button: `Undo CAS History Import`
- status text:
  - scanned count
  - imported count
  - skipped count
  - failed count
  - last error if any

Chinese UI copy should communicate the safety boundary clearly:

- `导入 OpenAI 历史到当前自定义 Provider`
- `撤销 CAS 历史导入`
- `不会修改原始历史文件，只创建可撤销的影子副本。`

## Host Actions

Add WebView host actions:

- `history_shadow_import`
- `history_shadow_undo`
- `history_shadow_status`

Responses should include:

```json
{
  "action": "history_shadow_result",
  "ok": true,
  "scanned": 120,
  "imported": 30,
  "skipped": 90,
  "failed": 0,
  "message": "history_shadow_import_complete"
}
```

## Undo Behavior

Undo reads the manifest and deletes only files listed as CAS-created shadow paths.

Safety rules:

- only delete paths under `%USERPROFILE%\.codex\sessions\cas_shadow_import`
- never delete source paths
- if a listed shadow path is outside the allowed directory, skip it and report an error
- after successful file deletion, keep a timestamped copy of the old manifest under `%USERPROFILE%\.codex\bak\cas_history_shadow_import\`
- write a fresh empty manifest

## Error Handling

The feature should report partial success instead of failing the whole import when one session file is malformed.

Expected errors:

- Codex home missing
- sessions directory missing
- malformed first JSONL line
- missing `payload.id`
- file copy failure
- shadow write failure
- undo path safety failure

Malformed or unsupported files are counted as skipped unless a filesystem operation fails after import starts.

## Testing Strategy

Native tests should cover pure file/metadata behavior with temporary directories:

- importable first-line metadata is detected correctly
- non-`openai` provider is skipped
- shadow file rewrites only first metadata record
- deterministic shadow id prevents duplicate imports
- undo refuses paths outside `sessions/cas_shadow_import`
- undo deletes only manifest-listed shadow files

Manual validation:

1. Restore or keep a Codex setup where `/resume` under custom provider hides an older `openai` session.
2. Run history shadow import from the app.
3. Start Codex with `model_provider="custom"` and open `/resume`.
4. Confirm imported `[CAS]` sessions are visible.
5. Confirm the original session still exists and can still be resumed by original id.
6. Run undo from the app.
7. Confirm shadow sessions disappear and originals remain.

## Risks

### Picker May Use Another Index

If current Codex versions use SQLite indexes instead of scanning JSONL files for picker display, shadow files alone may not appear immediately. MVP should still avoid direct DB mutation. If this risk is confirmed, Phase 2 can add a separate CAS history browser or a safe reindex strategy after inspecting Codex behavior.

### Resume Compatibility

A copied GPT session may be visible under `custom`, but continuing it against Ollama can fail because of model/tool/request-shape differences. The UI should frame this as history visibility/recovery, not full cross-provider compatibility.

### Duplicate Visual Noise

Shadow sessions intentionally duplicate old sessions. Prefixing thread names with `[CAS]` and providing undo keeps this manageable.

## Rollout

Phase 1:

- scan/import/undo shadow JSONL files
- manifest-based dedupe and safety
- WebUI buttons and status
- native tests

Phase 2 only if needed:

- investigate Codex picker indexing behavior
- add a CAS-owned history browser if Codex does not display shadow files reliably
- optionally support importing other source providers beyond `openai`

## Approval

Approved design direction: shadow import, no original history mutation, one-click undo.
