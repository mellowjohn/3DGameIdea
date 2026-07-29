---
title: I needed the fire to breathe
date: 2026-07-29
summary: Terrain proved the custom engine could hold a world. Fire proved it could feel alive. Here is why that mattered for Wrathful Conquest, and how the campfire stack is built.
cover: /images/cover-fire-breathe.jpg
tags:
  - art
  - particles
  - engine
draft: false
---

I am building **Wrathful Conquest**, a passion-project open-world RPG on a custom engine.

In the [first post](/posts/why-wrathful-conquest) I said the line I want people to remember is that this game should feel alive. That is not a slogan I stuck on later. It is why I left GameMaker for Unity, and why I left Unity for an engine I own: C++20, Direct3D 12, editor and runtime growing together on purpose.

Terrain and materials were the first big proof that bet was working. I could sculpt ground, paint textures, and walk the space. Then I stood in that space and it still felt empty. Foundation can be real and the place can still read like a still image.

That is why this campfire post exists. Particles are easy to dismiss as polish. For this project they are a check on the thesis. If the engine can only draw static ground and props, I have a tech demo. If it can stack mesh, light, materials, and layered emitters into something that burns in the scene, I have a world I want to keep building in.

I knew this was not the most productive thing on the critical path. I chased it anyway.

![Vertical-slice world with open terrain and materials, sparse props](/images/blog-fire-vertical-slice-empty.jpg)

*Vertical slice after terrain and materials: open ground, water, a few markers. The systems work. The place still feels empty.*

## What this has to do with the engine

Owning the stack is only useful if the systems show up in the frame together.

A commercial engine already has mature VFX tools. That was never the argument. The argument was control: particle recipes as plain project data, materials I can change without fighting a black box, a prefab that bundles mesh + light + emitters, and an editor an agent can drive while I stay in art direction. The [MCP post](/posts/i-gave-the-ai-tools-into-my-editor) was about giving AI tools into that loop. This post is about what that loop is for.

Fire is a small system with a loud read. Same pieces I need everywhere else later: authored content, runtime simulation, lighting, and a prefab you can drop in the world. Get the stack right on a campfire and you have a pattern for torches, hit sparks, wind, corrupt auras, the whole VFX vocabulary of the game.

## What was wrong before

Early campfires were a prop pretending. An older AI-generated mesh had blocky stone, a little triangle wood stack, and one static point light. No smoke. No embers. No flicker. Just an orange glow stuck in place.

I rebuilt the mesh by hand in Blockbench, low-poly on purpose (think [Unturned](https://store.steampowered.com/app/304930/Unturned/) energy, pushed a bit further on art fidelity). The mesh alone was still dead. The fix was stacking several systems on the same prefab, closer to how [Roblox](https://create.roblox.com/docs/effects/particle-emitters) particle tools encourage layered emitters. Plain-text inspiration only. Wrathful Conquest is not affiliated with, endorsed by, or sponsored by either.

<video controls playsinline loop preload="metadata" src="/videos/fire-breathe-campfire.mp4" poster="/images/cover-fire-breathe.jpg"></video>

*Sandbox Scene capture: the layered campfire running in the editor.*

## How the campfire is put together

The production campfire prefab is not one effect. It is a mesh, a warm point light, an emissive material, and four particle emitters at slightly different heights.

**1. The mesh and the glowing wood**

The campfire model uses an emissive material that pulses. In plain terms, the wood itself gets brighter and dimmer over about a second and a half, so the prop does some of the breathing before any particle shows up. The emissive color is warm orange. Bloom softens the hot spots so the core does not read as a hard white blob.

**2. A short, dense core**

Right above the wood sits a small "core" emitter. Particles live less than half a second, stay tight to the disc they spawn from, and use crossed billboards (two planes crossed so the flame has volume from more than one camera angle). This layer is the hot white-to-orange center. Without it, the bigger flame layer looks hollow.

**3. The main molten flame**

Above that is the hero flame emitter. It uses a 4x4 fire flipbook texture (a sheet of flame frames played like a tiny animation). Each particle starts on a random frame so they do not blink in lockstep. Over its life it goes white-hot, then orange, then dark red, grows, then shrinks as it dies. It accelerates upward and gets some drag, so it rises fast at first and then softens. Rate is high enough to keep the column filled. Soft additive blending lets layers stack without looking like cutout stickers.

**4. Sparks / embers**

A spark emitter sits a bit higher and shoots small bits with a wide spread. Lifetimes are short (often under a third of a second). Speed is high, then gravity pulls them down. That scatter is what sells "this fire is throwing ash," not just a soft orange fog.

**5. Smoke**

Smoke starts higher than the flame, lives a couple of seconds, grows as it rises, and uses alpha blending instead of the bright additive look. Color drifts toward a dull gray-purple. Slow flipbook framerate keeps it reading as smoke instead of another flame.

**6. The point light**

A warm orange point light sits near the flame height with a wide radius. Particles sell the motion. The light sells the space around the prop.

![Sandbox campfire framed tight in Scene view](/images/blog-fire-sandbox-close.jpg)

*Scene view: fire centered with a clear sightline past the props.*

![Another Scene angle on the sandbox campfire](/images/blog-fire-layers.jpg)

*Wider angle: smoke and glow still readable.*

The short version of the craft: one emitter is almost never enough. Core for heat, flame for body, sparks for chaos, smoke for volume, light for the room, emissive mesh so the wood is not a dead prop under the VFX.

## What agents did, and what broke

AI agents did a lot under my art direction: concept art for the prop, textures the particles emit, and most of the first-pass emitter setup. I still built the mesh myself. I want that mesh loop in the agent workflow later.

That split matters for the wider project. Agents are fast when the engine already has clear data (particle JSON, materials, prefabs) and live tools. They are messy when the art still needs a human eye. The Matrix holes are the proof: agents will hand you textures with weird black-abyss patches that look ripped out of the movie. I have fixed that on the campfire and on a pile of other props. Generate something cool, then patch the void so it belongs in the world.

Tuning was the real work after the first stack existed. Too little rate and the flame strobes. Too much and it turns into a solid orange pillar. Sparks that live too long look like floating confetti. Smoke that starts too low eats the flame. Most of the "this finally looks like fire" moment was adjusting those overlaps, not inventing a new system.

## What still is not right

Sound. I have barely touched it. A fire should crackle. Wind should move with the burn. Visual life without audio is only half the job.

Same idea applies beyond fire. Wind streaks across the world and against the character are the other cheap motion layer I have been chasing. One point light and one mesh will never sell a place on their own. The asset only feels integrated when the subsystems stack.

That is the engine story in miniature. Terrain showed I could hold a world. Fire showed I could put life in it. Next is more of that vocabulary in more places, without losing the person deciding what "alive" actually means.

If you want the earlier process piece, start with [I put AI tools in the editor](/posts/i-gave-the-ai-tools-into-my-editor).
