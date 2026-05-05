# Xiaomi Account Management Design

## Goal

Move Xiaomi MiMo API key entry out of the API proxy page and into account management. Xiaomi should appear as an external model account record after a key is saved.

## Behavior

- The Add Account dialog gets a Xiaomi tab.
- The Xiaomi tab collects:
  - Base URL, default `https://api.mimo-v2.com/v1`.
  - API Key.
- Saving the Xiaomi tab writes the key to existing `xiaomiApiKey` config and base URL to `xiaomiBaseUrl`.
- The original API key must never be displayed or prefilled.
- Updating a Xiaomi key is overwrite-only: the edit flow shows an empty API Key input and saves only the newly entered key.
- Account Management shows a Xiaomi MiMo row when a Xiaomi key exists.
- The Xiaomi row does not participate in Codex quota refresh or batch refresh.
- The Xiaomi row actions are:
  - Switch: set route mode to Xiaomi.
  - Overwrite Key: open the Add Account Xiaomi tab with an empty key field.
  - Delete: clear the Xiaomi key, removing the row.

## UI Placement

API Proxy page keeps service-level proxy controls and route mode. Xiaomi Base URL/API Key inputs move to the Add Account Xiaomi pane.

## Implementation Notes

Use the existing config fields added for Xiaomi routing. The first version supports one Xiaomi record, displayed as `小米 MiMo` / `Xiaomi MiMo`, because the current native route state has one active Xiaomi credential.

## Verification

- `node --check webui/js/app.js`
- Debug build
- Release build only if explicitly requested
