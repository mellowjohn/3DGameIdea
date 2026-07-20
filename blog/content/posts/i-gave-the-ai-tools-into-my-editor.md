---
title: I gave the AI tools into my editor
date: 2026-07-20
summary: Chat was not enough. I exposed the same moves I make by hand (place props, sculpt terrain, author World Forge data) so an agent can work inside the live Wrathful Conquest editor.
cover: /images/cover-mcp-tools.webp
tags:
  - ai-workflow
  - mcp
  - engine
draft: false
---

The intro post explained why I am building **Wrathful Conquest** on a custom engine. This one is narrower: what changed when I stopped asking an AI to *talk about* the editor and started giving it tools that act inside it.

In industry shorthand that bridge is **MCP** (Model Context Protocol). In plain language: the agent gets a short list of project-specific commands. Place a tree. Carve a river bed. Open World Forge. Validate the project. Same paths a human uses, just callable from chat while the engine is running.

## The loop that felt like magic

The clearest win was terrain and world building.

I spent real time exposing the manual moves: place assets, place objects, write scripts, create components, sculpt the ground. Once those lived behind MCP, a visionary idea about the environment did not have to wait for me to click every brush stroke. I could keep the editor running, dock the chat to that session, and shape the world from conversation.

That is the feeling I care about. Not "AI made a game." More like: the distance between a thought about the forest and a change on the terrain got short enough that I stay in the idea.

![Live editor with MCP connection enabled and a forest scene under construction](/images/blog-mcp-scene.webp)

*Scene tab with live automation on. Status bar says MCP tools can connect. Hierarchy, inspector, and the Diagnostics MCP checkbox are all in one shell.*

## Before: compile, hope, repeat

Before a custom MCP, AI help was real but awkward. A lot of the pain was iteration cost. In my earlier Unity / C# stretch, every meaningful change wanted another compile cycle. You describe what you want, wait, refresh, notice it is wrong, wait again.

The goal with this stack is the opposite. Keep the heavy C++ rebuilds for real engine work. Push more day-to-day content into data, Lua gameplay scripts, and live editor commands an agent can call without restarting the universe. I am still sorting where Lua should own a behavior versus where the C++ runtime has to own it. That boundary is honest work. It is also the whole point: put the slow door only where it has to be.

## What I do now (and what I refuse to hand off)

My job tilted. I spend more time as a visionary, strategist, and creative thinker: does this system make sense as data? Can I picture myself in this world? Which characters belong here? What art direction am I defending?

Macro over micro. I still care about the micro (a tree that floats looks silly), but I do not want my default day to be tactical clicking. Taste, story calls, and "does this feel alive?" stay with me. Agents accelerate the loop when the contracts are clear.

## Three workflows that earn the bridge

**1. Live scene and sculpt.** Place props, snap to terrain, raise and lower ground, push water along a path. This is where "chat docks to a running engine" stops being a demo and starts being how the slice gets built.

**2. World Forge.** Factions, map anchors, regions, quests, dialogues. The narrative side is not a separate app I paste into later. It is a first-class tab next to Scene and Sculpt, and the agent can read and write the same JSON the UI shows.

![World Forge Map Canvas driven through MCP, with cartography and live automation on](/images/blog-mcp-world-forge-map.webp)

*World Forge Map Canvas after an MCP view command. Cartography on, world map on, connection still live in Diagnostics.*

**3. Design docs inside the platform.** The connected part matters. Design notes, feature status, and the editor are not three disconnected piles. When the agent and I share one shell, we argue about the same world instead of drifting into two versions of the truth.

![Design Docs tab listing art, design, and feature notes inside the editor](/images/blog-mcp-design-docs.webp)

*Design Docs as a viewport tab, not a side wiki. Same session, same MCP bridge.*

## What still hurts

Compilation still hurts. When a change truly needs a C++ rebuild, the rhythm breaks: build, relaunch, reconnect Cursor, confirm the bridge, keep going. I am also still learning which gameplay ideas belong in hot-reloadable Lua versus systems that must stay native. The struggle is not mysterious. It is the cost of owning the stack.

So the strategy is boring and good: expose more of what I can do manually, keep authoring iterative inside the running platform, and reserve the long compile loop for foundation work.

## The agent-side picture

On the Cursor side this is not a generic "chat with your repo" plugin. It is a project MCP with named tools for scene, terrain, water, World Forge, UI, validation, and screenshots. The screenshots in this post were taken through that bridge, including the Map Canvas view.

![Custom Wrathful Conquest MCP tool list](/images/blog-mcp-tools.webp)

*What the agent sees: intentional tools, not a grab bag of UI clicks.*

## Why this is an "AI at work" story

If you build products with agents, the pattern is bigger than games. Chat about a system is useful. Chat that can *operate* the system (with clear commands, checkable results, and a human still owning taste) changes the pace of the work.

For Wrathful Conquest, that is the bet: keep me in the visionary seat, keep the world feeling personal, and let the tools erase the busywork between an idea and a living scene.

Next up on this blog will likely be more visual (maps, art direction). This post was the process piece. If you are wiring agents into your own tools, start by listing the moves you already trust by hand, then make those moves callable.
