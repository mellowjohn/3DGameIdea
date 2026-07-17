# TICKET-0185: Relationship node parentId + person affiliation helpers

- Epic: EPIC-0002
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39fd3efc569581b0b870e2698dd5f06e

## Goal

Add optional `parentId` on relationship nodes for Persons hierarchy trees, plus Hierarchy Persons detail helpers to upsert faction membership edges.

## Context links

- `context/formats/world-forge-relationships.md`
- TICKET-0012, TICKET-0184
- DEC-0035

## Acceptance criteria

- [x] Node `parentId` optional; empty=root; must reference another node; reject self/unknown/cycles
- [x] Format doc updated; sample nodes may omit parentId
- [x] Suite: reject bad parentId / cycles
- [x] Hierarchy Persons detail: parentId combo + faction affiliation upsert (`member_of`/`leads`)

## Out of scope

Migrating deity edge targets to pantheon ids; inventing kin trees.

## Dependencies

Soft: TICKET-0012. Used by TICKET-0184 Persons page.

## Verification

`--suite world_forge` 139/139; rebuild `engine`.

## What changed

### Summary

Relationship nodes gained optional `parentId` for org/family trees. Hierarchy → Persons can reparent via dropdown and set faction affiliation by upserting `member_of`/`leads` edges (lookup combos only).

### Files / surfaces

**Modified:** `world_forge_relationships_asset.h/.cpp`, format doc, Hierarchy Persons UI, suite tests

### Schema / API

`parentId` on nodes; errors WORLD-FORGE-REL-PARENT / WORLD-FORGE-REL-PARENT-CYCLE.

### Leftover risk

Sample data does not author person trees yet; affiliation helper replaces prior member_of/leads from that person to any faction.

## Agent notes

Shipped with TICKET-0183/0184.
