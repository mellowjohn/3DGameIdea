---
title: About
summary: I'm Johnathan Rossi, and Wrathful Conquest is my from-scratch open-world RPG and custom engine. Honest build notes live here.
---

I'm **Johnathan Rossi**. By day I work at LinkedIn. Before that I spent about five years in consulting. Outside of work I build things constantly, and I am a lifelong RPG fan (World of Warcraft, Dragon Age, Skyrim, Divinity: Original Sin, Fallout, and a long list of others). **Wrathful Conquest** is where that passion turns into game development: a single-player open-world action RPG, and the custom engine that runs it.

I'm building both from scratch on Windows with C++20 and Direct3D 12. In plain terms, that means I own the rendering stack and the tools instead of renting someone else's black box. I want the game and the engine to grow together on purpose.

You can find me on [LinkedIn](https://www.linkedin.com/in/john-rossi-16687416b/).

This page is the short version. The posts are the week-to-week trail.

## The game

The goal is a third-person dark-fantasy RPG in a seamless open world: exploration, combat, factions, and choices that stick. Story and systems live in versioned project data so the world can grow without rewriting the foundation every time something changes.

I keep story spoilers off this site. If you want the lore, you'll get it in the game. If you want how the sausage gets made, you're in the right place.

## The engine

I'm not wrapping a commercial engine. The runtime is Windows-first, written in modern C++, and drawn with Direct3D 12 (Microsoft's low-level graphics API). The aim is a large streamed world, solid collision and character movement, an editor you can actually work in, and content formats that stay readable and automatable.

Highlights of the approach:

- A seamless open-world partition (kilometer-scale world broken into cells that load and unload as you move) instead of bolted-on load zones
- A hybrid entity model: authored hierarchy where it helps people think, ECS-style data where it needs to scale
- Lua for content-heavy gameplay logic, C++ for the systems that have to be fast and reliable
- An integrated editor plus command surfaces so the same operations work for a human and for automation

I'm honest about status. Foundation, world streaming, collision, character movement, and a growing editor are real. Animation polish, full RPG data, dialogue tools, VFX, and a vertical combat slice are still on the road. Planned stays planned until it ships.

## Building with AI collaborators

AI coding agents are not a side note here. They are a real accelerator for this project, and I want to be open about that. The tooling is designed so agents can do bounded, checkable work: clear project data, stable commands, and an editor they can drive without guessing.

That does not mean "AI wrote my game." Design decisions stay with me. Agents help execute and iterate when the contracts are clear. I will write about what works, what breaks, and how I'm using them, so other builders can steal the useful parts and skip the painful ones.

## What this site is

This is the public build log for Wrathful Conquest.

You'll see short notes when something meaningful ships, occasional deeper posts on architecture or process, and a clear line between engine progress and story spoilers. I post on LinkedIn for reach. This archive is the durable home.

Some posts will get more technical. When they do, I'll teach the terms as I go. My readers are a mix of friends, consultants, and engineers, and not everyone has built a game engine. That is fine. Clarity beats gatekeeping.

## Follow along

Subscribe on this site if you want an email when new posts land. I send those myself. Share a post when it earns a conversation. If you're building something similar (an engine, a game, or an AI-assisted workflow), I want this to be useful to you, not just a personal scrapbook.
