# Recording — Quest UI + progression (2026-07-29)

Provenance for Dom + John design session on quest player UX, journal categories, and character power progression. Source export: `recording_dg85ct0l_2026-07-29.md`.

## Recording Information

- **Date**: July 29, 2026 at 02:33:38 PM EDT
- **Channel**: General
- **Duration**: 12m 13s
- **Speakers**: John, Dom
- **Total Messages**: 183
- **Voice Events**: 3

## Session summary

Reviewed World Forge quest authoring (main / side, dialogue hooks, objectives, forks) and the new [`quest-ui.pen`](quest-ui.pen) player flow. Locked journal filters as **Main · Side · Faction · Archetype** (separate toggles). Faction quests = Cristallo / Arrotrebae / Kingdom-of-Tessera standing threads; archetype quests = lane home-org lines that unlock gear / abilities. Accept UX intent: world **?** over quest NPC → interact → bottom-center dialogue (existing dialogue chrome). Tracked objective stays a HUD chip (top-left draft). Events/cinematics may bind to quest stages. Progression lean: **no traditional XP / player level** — power from gear, act-tier loot, and archetype quest unlocks (Terraria-shaped); Dom still conflicted — not a hard DEC. Session cut mid-question on what “unlocking archetypes” grants.

## Transcript → canon maps

| Transcript | Canon |
| --- | --- |
| “Ash ball blade” | **Ashfell Blade** |
| “Eritrobi” / “Echoby” | **Arrotrebae** |
| “Cristo” | **Cristallo** |
| “Laurel Safe versus Fleet” | **Larrell** save vs flee (`larrell_save_vs_flee`) |
| “Ferraria” / “Terraria” | **Terraria**-shaped gearing ([DEC-0048](../decisions/index.md#dec-0048-terraria-shaped-gearing-with-soft-archetype-affinity)) |
| “axones” / “axe” (story) | **acts** |
| “Questy” (right-tab tracker) | Questie-like addon reference — not a project product name |
| “hook star / stars” | dialogue **hooks** (`startId` / `completeId` / per-objective) |

## Locks / drafts from this session

| Topic | Status | Note |
| --- | --- | --- |
| Journal kinds: Main / Side / Faction / **Archetype** | **draft lock** | Separate tabs; not “faction = archetype” |
| Faction vs archetype meaning | **draft lock** | Faction = major polity standing quests; Archetype = lane org unlocks |
| Quest accept: `?` NPC → dialogue bottom-center | **draft lock** (UX) | Matches dialogue-ui placement |
| HUD tracker top-left | **draft** | Dom asked right vs top; pen keeps top-left pending owner polish |
| Kind coloration | **draft lock** | Distinct colors for main / side / faction / archetype |
| Quest ↔ event timeline / cinematic | **intent** | Stages may trigger cinematics + dialogue; schema still EventTimeline |
| No XP / level; scale via gear + act + archetype quests | **lean draft** | Dom conflicted; open until DEC |
| Archetype unlock reward shape (ability vs gear vs both) | **open** | Cut short by Dom coaching call |

## Related updates

- [`quest-ui.pen`](quest-ui.pen) — Archetype tab, NPC accept screen, progression notes
- [`../formats/world-forge-quests.md`](../formats/world-forge-quests.md) — proposed `archetype` kind
- [`../interviews/open-questions.md`](../interviews/open-questions.md) — leveling + archetype quest rewards
- [`../features/gearing-system.md`](../features/gearing-system.md) — progression lean

## Transcript

*John is present - 02:33:38 PM*

*𝕯𝖔𝖒 is present - 02:33:38 PM*

**𝕯𝖔𝖒** - 02:33:40 PM  
Oh sure.

**John** - 02:33:40 PM  
Wait, can you repeat what you just said? Sorry, I wanted to capture that.

**𝕯𝖔𝖒** - 02:33:42 PM  
Uh yeah, so uh just for you no cat, um we're talking about developing our quest lines. That's what we're starting on. And my question is is it like going to be just like a question mark on top of an NPC? Then you go to 'em and then once when you interact, then you have like maybe on like

**John** - 02:33:52 PM  
Mm-hmm.

**𝕯𝖔𝖒** - 02:34:06 PM  
Yeah.

**𝕯𝖔𝖒** - 02:34:07 PM  
Bottom part of the screen, but like you know, towards the center, you have like the quest text, and then he talks until you get an option and presents itself.

**John** - 02:34:12 PM  
Yeah.

**𝕯𝖔𝖒** - 02:34:16 PM  
Yeah.

**𝕯𝖔𝖒** - 02:34:17 PM  
I'm just wondering how it's gonna like how it's gonna look, how it's gonna feel. Maybe if it feels like Final Fantasy and

**John** - 02:34:18 PM  
Yeah.

**𝕯𝖔𝖒** - 02:34:24 PM  
How that works. I don't

**John** - 02:34:25 PM  
Yeah. I think so I can show you like what we have so far in terms of quest implementation and how we can build on that further. But we have entities called quests and as you can see like MQ is main quest.

**John** - 02:34:42 PM  
And SQs are side quests, right?

**𝕯𝖔𝖒** - 02:34:43 PM  
Oh excuse it. Okay.

**John** - 02:34:45 PM  
And here you can like

**𝕯𝖔𝖒** - 02:34:45 PM  
Well.

**John** - 02:34:48 PM  
edit and create quests and they pretty much have like specific things, so like what makes their summary

**𝕯𝖔𝖒** - 02:34:53 PM  
Should they have different coloration?

**John** - 02:34:56 PM  
Yeah, I think like we could do different c colorations for like main quests, side quests, and we could do filtration for that, which would be good. Right? Um

**𝕯𝖔𝖒** - 02:35:01 PM  
Mm-hmm.

**𝕯𝖔𝖒** - 02:35:03 PM  
Mm. Yeah.

**John** - 02:35:05 PM  
There's dialogue hooks.

**John** - 02:35:08 PM  
The dialogue that starts the quest is here, the dialogue that c completes it is here, so it's like the last dialogue.

**𝕯𝖔𝖒** - 02:35:16 PM  
Yeah.

**John** - 02:35:16 PM  
Um there's also dialogue abandonment.

**John** - 02:35:21 PM  
If that if that's the case. And then there's this concept of objectives. So there's like quest objectives here.

**𝕯𝖔𝖒** - 02:35:27 PM  
Mm-hmm.

**John** - 02:35:27 PM  
Um

**John** - 02:35:29 PM  
Other than that, there's this concept of forks, like the Laurel Safe versus Fleet concept.

**𝕯𝖔𝖒** - 02:35:34 PM  
Mm-hmm.

**John** - 02:35:35 PM  
And then you can add requirements and rewards and open questions. Um so there's this so far. Looking at this though, but besides like designing the UI, what seems like transparent to you? What seems confusing?

**John** - 02:35:58 PM  
Oh.

**𝕯𝖔𝖒** - 02:35:59 PM  
Um

**𝕯𝖔𝖒** - 02:36:04 PM  
Okay.

**𝕯𝖔𝖒** - 02:36:05 PM  
I don't know how hook star like stars.

**John** - 02:36:09 PM  
Then I can do preview as well.

**𝕯𝖔𝖒** - 02:36:16 PM  
I'm I I guess I just more so kinda wanna see

**𝕯𝖔𝖒** - 02:36:21 PM  
How it's going to look like from a UIUX perspective.

**John** - 02:36:25 PM  
Okay.

**John** - 02:36:29 PM  
Let's start from a UIUX perspective. Let me go here.

**𝕯𝖔𝖒** - 02:36:33 PM  
Like what's the user flow gonna look like after you select the uh quest icon?

**John** - 02:36:37 PM  
Yep, let me do this.

**John** - 02:36:40 PM  
So um I'm gonna do

**John** - 02:36:43 PM  
A new new pen file.

**𝕯𝖔𝖒** - 02:36:46 PM  
And e and even if like I guess you'll like you know how like Questy has like the quests on the right tab? Like are they gonna be on like the right side? Are they gonna be it's gonna be like like if you have a quest that's tracking, is that gonna be the top?

**𝕯𝖔𝖒** - 02:36:59 PM  
your screen maybe or

**John** - 02:37:02 PM  
Mm-hmm.

**𝕯𝖔𝖒** - 02:37:02 PM  
Like in a specific location.

**John** - 02:37:05 PM  
No, that makes sense.

**𝕯𝖔𝖒** - 02:37:06 PM  
Yeah.

**John** - 02:37:09 PM  
Yeah.

**John** - 02:37:12 PM  
Um how you can organize and create

**John** - 02:37:18 PM  
Yeah.

**John** - 02:37:19 PM  
And make the file.

**John** - 02:37:21 PM  
Alright, cool. So I'm gonna let it do that for now.

**John** - 02:37:25 PM  
that but I think other than that well that's working.

**John** - 02:37:30 PM  
Um we have that. We have this concept of events.

**John** - 02:37:35 PM  
happen too which could tie into quests

**John** - 02:37:39 PM  
So

**John** - 02:37:43 PM  
you could have like a cinematic event that happens, right? That could be tied to each of these quests and it could trigger like, oh, okay, like this cinematic event happens and

**𝕯𝖔𝖒** - 02:37:50 PM  
Yeah.

**John** - 02:37:54 PM  
Um, it could be like in conjunction with dialogue that happens. And I think it's important to talk about these things 'cause this understands like oh, okay, like how do you want

**John** - 02:38:04 PM  
Um how do you want quests to like work? Is it like okay you have a quest and maybe you have some kind of cinematic that happens and then you have dialogue, right? That happens within the quests. And then you have like an event, like it tells the player, Hey look, like go here and walk over here and then something happens and it triggers something, right? So

**𝕯𝖔𝖒** - 02:38:12 PM  
Mm-hmm.

**𝕯𝖔𝖒** - 02:38:14 PM  
Yeah.

**John** - 02:38:25 PM  
I think it's important to understand that aspect too.

**𝕯𝖔𝖒** - 02:38:29 PM  
Yeah.

**John** - 02:38:30 PM  
Um, I think also like going to quest itself, we would have main quests, side quests. I think we're gonna have like archetype quests. I think that's gonna be an important category.

**𝕯𝖔𝖒** - 02:38:41 PM  
Oh yeah, maybe that could be

**𝕯𝖔𝖒** - 02:38:47 PM  
Yeah.

**𝕯𝖔𝖒** - 02:38:48 PM  
Like maybe they could do archetype quests to unlock specific abilities or to get specific items.

**John** - 02:38:54 PM  
Right. Like I think that would be cool. Like maybe there's like an archetype quest line that um

**𝕯𝖔𝖒** - 02:39:00 PM  
Yeah.

**John** - 02:39:01 PM  
Like for the ash ball blade that'll let unlocks like a specific gear piece.

**John** - 02:39:07 PM  
Or a player, right? And it's a it's a huge power advantage. Or there's

**John** - 02:39:13 PM  
Um sp like you said, like a specific ability that

**John** - 02:39:16 PM  
players can do and it and it could be an optional, it could be required, but like they could go and do it and then it unlocks like a specific ability that, you know

**John** - 02:39:27 PM  
increases their damage output or their makes them have more sustain or gives them a really cool utility, right?

**𝕯𝖔𝖒** - 02:39:34 PM  
Yeah.

**John** - 02:39:35 PM  
Um, I think those are things that we can definitely think about. Then also as well as from quests, how do they drive relationships?

**John** - 02:39:48 PM  
Like how do they

**John** - 02:39:50 PM  
How do they like do they like increase reputation? Do they like give you rewards? Obviously I think they do. They're gonna give you like gear and experience, ex etc. Um actually you know that's a good question.

**𝕯𝖔𝖒** - 02:39:54 PM  
Mm-hmm.

**𝕯𝖔𝖒** - 02:39:58 PM  
Yeah.

**𝕯𝖔𝖒** - 02:40:02 PM  
Okay.

**𝕯𝖔𝖒** - 02:40:04 PM  
Mm-hmm.

**John** - 02:40:05 PM  
Do we want experience in this game or no?

**𝕯𝖔𝖒** - 02:40:09 PM  
Hello, leveling.

**John** - 02:40:10 PM  
Yeah, do we want that or no? Like d or do we want to keep it more like terraria where it's like you just get gear and stuff and

**John** - 02:40:18 PM  
What not?

**𝕯𝖔𝖒** - 02:40:18 PM  
Like do you scale based upon player level or you scale based upon items?

**John** - 02:40:23 PM  
Yeah.

**𝕯𝖔𝖒** - 02:40:24 PM  
Um

**John** - 02:40:32 PM  
Are you just stronger because you have a better weapon and you have better talents and whatnot?

**𝕯𝖔𝖒** - 02:40:38 PM  
Yeah.

**John** - 02:40:39 PM  
you just unlock those things because you progress through the storyline.

**𝕯𝖔𝖒** - 02:40:40 PM  
Well

**𝕯𝖔𝖒** - 02:40:43 PM  
Yeah

**𝕯𝖔𝖒** - 02:40:46 PM  
That's quite interesting.

**𝕯𝖔𝖒** - 02:40:48 PM  
Because I guess I um

**𝕯𝖔𝖒** - 02:40:52 PM  
Mm.

**John** - 02:41:00 PM  
Honestly, like this game doesn't need a level system, technically. Like

**𝕯𝖔𝖒** - 02:41:03 PM  
Yeah, I'm I'm conflicted actually. 'Cause

**John** - 02:41:06 PM  
Right.

**𝕯𝖔𝖒** - 02:41:07 PM  
Maybe it doesn't.

**𝕯𝖔𝖒** - 02:41:09 PM  
And then

**𝕯𝖔𝖒** - 02:41:10 PM  
Well then and then how how would you scale it? You s you scale it based upon the weapons, you scale it based upon maybe the gear you would acquire. But then how does how does how does your archetype then create like attributes?

**John** - 02:41:16 PM  
Yeah.

**John** - 02:41:18 PM  
Like the axones that you're in.

**John** - 02:41:25 PM  
So the archetype

**John** - 02:41:28 PM  
will scale based on where you are in the storyline, right? So like for example, you know, when you've completed a major objective within an act, you unlock things. The same with like maybe going doing progression with the archetype questline. Maybe that's how you scale your character up more. Like instead of

**𝕯𝖔𝖒** - 02:41:32 PM  
Yeah.

**𝕯𝖔𝖒** - 02:41:37 PM  
Yeah.

**𝕯𝖔𝖒** - 02:41:47 PM  
Yeah, so like like Ferraria, like whenever you kill a boss, then you have the potential to get more gear and scale.

**John** - 02:41:53 PM  
That's right, exactly. So like if you kill a major boss you have the potential of getting more gear and it'll and it'll un locks further into the axe, right? I think and I think that's pretty cool in the sense that it's like oh okay, it forces players to

**John** - 02:42:08 PM  
do things in that sense without it just being like, Oh, I level up my character and that's it, you know, and I w pick a talent or a spell.

**𝕯𝖔𝖒** - 02:42:09 PM  
Yeah. But what what about

**𝕯𝖔𝖒** - 02:42:13 PM  
Yeah. But what about predetermines that allocation by selecting archetype? Does that scale or does that relatively stay the same? Because you're specifically

**John** - 02:42:20 PM  
So predetermined stats are so I think it's gonna be like this, like where you your archetype determines like

**𝕯𝖔𝖒** - 02:42:22 PM  
Yeah.

**John** - 02:42:29 PM  
like how much of a bonus you get from the stats scaling and then the stats that you get from the items will obviously be an influenced by your like archetype.

**𝕯𝖔𝖒** - 02:42:32 PM  
Okay, from the item.

**𝕯𝖔𝖒** - 02:42:39 PM  
Okay.

**John** - 02:42:40 PM  
And the further you are in the axe, obviously the the better quality items and gear and the the more exposed you are into increased loot tables.

**John** - 02:42:52 PM  
I think that's gonna be the most accurate thing.

**𝕯𝖔𝖒** - 02:42:56 PM  
Mm-hmm.

**John** - 02:42:56 PM  
And I think that's pretty cool because I think the biggest problem that I've had with like level content scaling

**John** - 02:43:04 PM  
is that I could just like do something some cheese and like level up my character.

**𝕯𝖔𝖒** - 02:43:08 PM  
Yeah.

**John** - 02:43:09 PM  
And I could ignore a lot of the other content in the game because I don't have to do it.

**𝕯𝖔𝖒** - 02:43:15 PM  
Yeah, yeah, yeah, yeah. But like in in this way you're actually forced to max out

**John** - 02:43:16 PM  
Right.

**𝕯𝖔𝖒** - 02:43:23 PM  
Like

**𝕯𝖔𝖒** - 02:43:24 PM  
middle cont like the content you need and maybe go back to earlier content, acquire items.

**John** - 02:43:30 PM  
Right.

**𝕯𝖔𝖒** - 02:43:30 PM  
Then go back and

**John** - 02:43:35 PM  
No, I totally agree. Oh, and also actually now let's go to

**John** - 02:43:39 PM  
made this quest UI pen. And obviously it doesn't have our conversation context.

**𝕯𝖔𝖒** - 02:43:44 PM  
Yeah.

**John** - 02:43:45 PM  
But we can take a look at what it's been deciding to drive for the user flow here.

**John** - 02:43:53 PM  
So

**John** - 02:43:54 PM  
This is what it's saying.

**John** - 02:43:56 PM  
Alclus work and play. Except

**𝕯𝖔𝖒** - 02:43:59 PM  
Yeah.

**John** - 02:43:59 PM  
Rack.

**John** - 02:44:01 PM  
Advance.

**John** - 02:44:02 PM  
Fork.

**John** - 02:44:04 PM  
the outcomes like what happens.

**𝕯𝖔𝖒** - 02:44:06 PM  
Uh

**𝕯𝖔𝖒** - 02:44:07 PM  
Yeah, yeah, yeah.

**John** - 02:44:09 PM  
And resolve.

**𝕯𝖔𝖒** - 02:44:11 PM  
Okay.

**John** - 02:44:11 PM  
Like and then there's like main side faction. Do you like this or?

**𝕯𝖔𝖒** - 02:44:16 PM  
Main side of faction is archetype.

**John** - 02:44:20 PM  
Yeah, I'm assuming factions gotta be like oh, faction is a couple of things.

**𝕯𝖔𝖒** - 02:44:25 PM  
Yeah.

**John** - 02:44:25 PM  
One is archetype.

**𝕯𝖔𝖒** - 02:44:27 PM  
And the other could be reputation.

**John** - 02:44:27 PM  
I think faction also

**John** - 02:44:29 PM  
Yeah, and I think faction is like Cristallo, Eritrobi.

**John** - 02:44:34 PM  
Right that's a good idea.

**John** - 02:44:35 PM  
And like Tessera and stuff. Like those you could have quests besides.

**John** - 02:44:41 PM  
Um the main quests, which obviously there are main quests that involve you with the Cristo and the Echoby and whatnot, but

**John** - 02:44:48 PM  
Like there's also side quests that are faction quests.

**John** - 02:44:52 PM  
Right.

**John** - 02:44:54 PM  
Um

**John** - 02:44:55 PM  
Like does this all is this does this look good in terms of a workflow here?

**𝕯𝖔𝖒** - 02:44:59 PM  
Yeah. I think

**𝕯𝖔𝖒** - 02:45:02 PM  
Action.

**𝕯𝖔𝖒** - 02:45:03 PM  
I guess so unless you wanna keep faction as it is or you wanna separate faction by archetype.

**𝕯𝖔𝖒** - 02:45:12 PM  
Like have a a like an archetype toggle and then a faction toggle.

**John** - 02:45:15 PM  
Yeah.

**𝕯𝖔𝖒** - 02:45:15 PM  
But

**𝕯𝖔𝖒** - 02:45:16 PM  
Otherwise you're fine. Or I or archetype could just be categorized with

**John** - 02:45:22 PM  
I think that's like you know, and I I like archetype can be its own thing and faction can be its own thing and and I think that's like more transparent to users, probably.

**𝕯𝖔𝖒** - 02:45:30 PM  
Yeah, yeah.

**John** - 02:45:32 PM  
Um what about

**𝕯𝖔𝖒** - 02:45:33 PM  
Because I because I think they're gonna wanna do quests to unlock certain things for

**𝕯𝖔𝖒** - 02:45:37 PM  
Like now but now that we're not doing

**John** - 02:45:38 PM  
Right.

**𝕯𝖔𝖒** - 02:45:41 PM  
What do you get in terms when you unlock archetypes? Fuck, I got a coaching call, shit.

**John** - 02:45:47 PM  
Oh no you're good. I'm gonna end the recording for now.

**𝕯𝖔𝖒** - 02:45:49 PM  
Yeah.

*John ended the recording - 02:45:51 PM*

