---
title: Why I'm building Wrathful Conquest (and the engine under it)
date: 2026-07-16
summary: From GameMaker to Unity to a custom engine. Why I want this RPG to feel alive, and how AI collaborators help me build the world without losing the person behind it.
cover: /images/cover-ai-engine.webp
tags:
  - introduction
  - engine
  - ai-workflow
draft: false
---

I'm Johnathan Rossi. By day I work at LinkedIn. Before that I spent about five years in consulting. Outside of work I build things constantly, and I am a lifelong RPG fan. **Wrathful Conquest** is where that passion turns into a real game, and into the custom engine underneath it.

This first post is the scope check. What I'm building, why I ended up here, what already works, and how AI fits without turning the whole thing into a tech demo with no soul.

## It started in 2D, then it didn't

Wrathful Conquest did not begin as a "build your own engine" manifesto.

I started in **GameMaker**. It was going to be a 2D project. Then I realized I wanted the open-world feel more than I wanted the comfort of that first stack. I wanted something that could feel vibrant and alive in three dimensions, so I moved to **Unity**.

Unity got me further. Then the project broke in a way that made me stop and ask a blunt question: if I already want custom systems, and I already want development to stay fluent with AI, why keep forcing that through a platform I do not fully control?

So I started building my own engine. Not because that is the easy path. Because I needed the systems to match the product I actually want, and I needed a clear gauge of what that product requires.

Today that engine is Windows-first, written in **C++20** (a modern systems language), and drawn with **Direct3D 12** (the low-level graphics API that talks to the GPU on Windows). Those details matter later. The point now is simpler: the game and the tools grow together on purpose.

## One line to remember

If a friend only remembers one sentence about this game, I want it to be this:

**This game should feel alive.**

Alive means the play feels present. Interactions matter. The world invites fellowship and participation, not tourism. I want players to feel like they are in the journey, not watching a checklist of systems.

That is also the tension I care about with AI. Yes, I am building with AI collaborators on purpose. No, I do not want the finished experience to feel like "someone generated a project." I want to close that gap: modern AI-assisted development on one side, and something personal and human on the other.

## What already works (and why I'm proud of it)

My proudest progress so far is not a manifesto. It is the world.

![Editor viewport with terrain, trees, and a character capsule](/images/blog-editor.webp)

*The live editor: sculpted terrain, placed foliage, hierarchy, inspector, and the MCP connection checkbox in Diagnostics.*

The engine foundation is real enough that I can sculpt terrain, place objects, put a character in, and walk the space. That sounds basic until you remember how long "basic" can take when you own the stack. I thought this layer would take a lot longer. It is here.

![Runtime debug-world capture with character capsule on terrain](/images/blog-world.webp)

*A hidden runtime capture of the same idea: character in the world, terrain underfoot. Early and dark, but real.*

I am also building **World Forge**, the narrative and world-authoring side of the project, so story structure and world data stay connected to what you see in the editor. This is the mock v1 shell: factions, relationships, map, quests, and dialogues in one place.

![World Forge mock v1 with Factions tab and sample Tessera data](/images/blog-world-forge.webp)

*World Forge mock v1: Factions / Relationships / Map / Quests / Dialogues, loaded from the sample project.*

And then there is the AI bridge.

I built structured commands so an AI agent can work against the live project: editor changes, data, schema-level work, the boring glue that usually kills momentum. In industry shorthand that bridge is often called **MCP** (Model Context Protocol). In plain language: the agent does not only chat about the game. It can take intentional actions inside the tooling I already use.

![Editor Diagnostics panel with Enable MCP connection](/images/blog-mcp-editor.webp)

*In the editor: flip on live automation so Cursor can talk to this session.*

![Custom Wrathful Conquest MCP tool list](/images/blog-mcp-tools.webp)

*What the custom MCP looks like on the agent side: project-specific tools for scene, terrain, World Forge, Lua, and UI.*

That is the part that still feels a little nuts in a good way. It changes my role. I get to spend more time as a purposeful product owner and visionary, and less time as the only pair of hands between an idea and the editor.

I still make the calls. Taste, scope, and "does this feel alive?" stay with me. Agents accelerate the iteration.

## Why a custom MCP was worth the engine bet

Other platforms have AI integrations. That was never the whole argument.

I wanted a product I could know deeply, tailor to how I actually work, and keep iterative at runtime: art direction, storylines, editor changes, project data. Less "wait for a heavy compile cycle every time curiosity shows up." More "try it, see it, keep moving."

That fluency is the real reason the custom engine and the AI workflow are the same bet.

## What this blog is

This site is the public build log.

I want to post weekly when I can. If a week slips, that is okay. Life and shipping both happen. When I get technical, I will teach the terms as I go. My readers are a mix of friends, consultants, and engineers, and not everyone has built a game engine. Clarity beats gatekeeping.

If you want the short version of me and the project, the [About](/about) page has it. Subscribe if you want an email when new posts land. Share a post when it earns a conversation.

Thanks for being here at the start. The goal is simple to say and hard to earn: build a world that feels alive, with tools that let me move fast without losing the person behind the work.
