# White Elephant — Gameplay Design (Draft v0.1)

Working title: **"Steal Night"** (placeholder)

## Pitch

You're at a white elephant party. Leave with the best gift. But it takes liquid
courage to steal, and too much liquid courage makes a scene. It's a short
arcade loop: one party is about 3–5 minutes, and you chase a high score.

## Screen layout (240x240)

```
+------------------------------+
|  .-\-.   SWEET CHARM  .-|-.  |  <- BUZZ and VIBE needle gauges (keep both
| ( BUZZ )   $42 HIDE  ( VIBE ) |     pointing up), score, hide suspicion
|   (G1)   (G2)   (G3)         |  <- guests around the room, each with the
|                              |     gift they're holding shown above them
|  (G6)    [TREE]    (G4)      |  <- unopened gift pile under the tree
|                              |
|      (G5)     (YOU)          |
| > Aunt Barb steals the mug!  |  <- one-line message ticker
+------------------------------+
```

The room is a fixed scene, and each guest sits in a fixed position. That keeps it
Tiger-style: things light up in place rather than moving around freely.

## Controls

| Input | Action |
|---|---|
| Joystick | Move the selection cursor (guests / tree) |
| A | Confirm: open a gift or attempt a steal |
| B | Call the drink tray / grab the drink under your hand |
| X (hold) | Hide your gift |
| Y | Pause |

## Core loop

The game follows real white elephant rules, plus a real-time layer on top.

1. **Turn order is drawn** at the start. You are one of N guests (4–7).
2. **On a guest's turn**, they either **open** a wrapped gift from the tree
   or **steal** an opened gift from someone.
3. **If your gift is stolen**, you immediately take a turn: open or steal
   (no stealing straight back what was just taken from you).
4. **Freeze rule:** after its 3rd steal, a gift is locked to whoever holds it.
5. **The party ends** when the last wrapped gift is opened. The first player
   then gets one final optional steal (the classic rule).

**Real-time layer:** turns play out on a timer, not instantly. While the AI
guests take their turns, the action keeps moving:
- Call the **drink tray** whenever you like (see "Drink tray" below).
- Your **buzz decays** over time.
- Guests make **remarks** in the ticker that hint at their next move
  (e.g. "Dave keeps eyeing your gift...").

## Your turn

You choose between two options.

### Open a gift
- Select the tree and press A to pick up a wrapped gift.
- **Shake mini-game:** before you commit, you can shake up to 3 gifts. Each
  shake shows a hint (size / rattle / weight icon). Hints are honest when
  you're in the sweet spot, vague when you're sober, and sometimes *lies*
  when you're drunk.

### Steal
- Select a guest holding an unfrozen gift and press A.
- **Grab mini-game:** a marker sweeps across a bar, and you press A inside
  the target zone.
  - Sweet spot: wide zone, steady marker
  - Sober: narrow zone ("you hesitate awkwardly")
  - Drunk: the marker wobbles and stutters, and the zone jitters
- **A miss makes a scene:** you take the next wrapped gift instead, and the
  social penalty makes guests more likely to target you.

## Buzz meter

This is the heart of the game.

```
0 ------[ sober ]------|=== SWEET SPOT ===|------[ drunk ]------ 100
```

- **Delayed absorption:** a drink doesn't hit right away. It adds *pending
  buzz* (a faded ghost bar) that drains into real buzz over several seconds.
  You have to plan ahead: a shot now is felt in 10 seconds.
- **Decay:** real buzz slowly drops over time.
- **Drinks** each roll a random strength, so you never know exactly how hard
  one will hit. Current list and numbers are in `game/tuning.h`: light beer,
  good wine, bad wine, eggnog, shots, mystery punch, and water (which
  sobers you up).

### Drink tray

Press B (D on the keyboard) and a tray of 5 drinks appears. Your hand sweeps
back and forth over it; press B again to grab whatever it's over. Hesitate
for 3 seconds and you grab whatever's under your hand anyway.

- **No names on the tray**, only what each drink looks like. Drinks that look
  the same can't be told apart until you've grabbed one: good wine and bad
  wine are both just "wine", so a glass of wine is a gamble.
- **The tray reflects your state.** Sober, it leans toward water and lighter
  drinks; drunk, toward shots, bad wine and punch. **There's always at least
  one water**, so sobering up is always possible if you can hit it.
- **Clear-headed, the tray is sorted** weakest to strongest (water on the
  left). **Drunk, it's shuffled**, so you have to read it first.
- **Drunk, your hand moves faster and lurches**, making the one water a hard
  target.
- There's a short cooldown after each drink. You can't call the tray while
  hiding your gift.

- **Zone effects:**
  - **Sober (0–30):** steals are hard, and guests ignore you (you're boring).
  - **Sweet spot (30–65):** steals are easy and hints are honest. You're
    charming, so guests are less likely to steal from you. Score ticks up
    slowly ("life of the party").
  - **Drunk (65–90):** the screen sways, the cursor drifts, and random
    hiccups sometimes press buttons for you. Guests target you more.
  - **Blackout (90+ for 3 seconds):** **cut off. GAME OVER.** You're sent to
    the couch and the whole run ends. The current party scores nothing ("you
    don't remember what you unwrapped"), but points from completed parties
    still count toward the high score.

## Vibe meter

The second meter. It measures how you're coming across, and like buzz, you
want to stay in the middle:

```
0 ---[ WALLFLOWER ]---|==== CHARMING ====|---[ OBNOXIOUS ]--- 100
```

It's built to fight the buzz meter: drinking gets you out of Wallflower, but
too much drinking drags you into Obnoxious.

**The rule:** good outcomes pull you toward the middle. Bad outcomes
("flops") never help. You keep however far off-center you already were, land
on the side that matches what you did (rude or shy), and get pushed further
out. A flopped roast while shy swings you all the way to the rude end.

| Cause | Effect |
|---|---|
| Buzz sweet spot | Slowly pulls toward the middle, from either side |
| Sober | Drifts toward Wallflower |
| Drunk | Drifts toward Obnoxious, faster the drunker you are |
| Banter that lands | Pull toward the middle (bigger for riskier answers) |
| Flopped roast | Rude flop |
| Flopped laugh or flatter, ignoring, zoning out | Shy flop |
| Stealing | Rude flop; bigger for robbing the New Hire |
| Making a scene | Big rude flop |

**What the zones do:**
- **Charming:** guests are less likely to steal from you, the steal zone is
  wider, and you earn Charm points.
- **Wallflower:** no Charm points, and your turn timer is shorter (people
  talk over you).
- **Obnoxious:** guests target you. (HR strikes are a planned follow-up.)

## Banter

Every few seconds during other guests' turns, someone says something to you.
You have about 2.5 seconds to answer with the joystick:

| Joystick | Response | Risk |
|---|---|---|
| Up | Laugh along | Safe, small gain; rare small shy flop |
| Left | Flatter | Medium gain; shy flop if awkward. Lands best on the Manager and New Hire |
| Right | Roast | Big gain if it lands, big rude flop if it doesn't. Sales Guy loves it; HR does not |
| Down | Ignore | Shy flop |
| (no answer) | Zone out | Small shy flop |

- **Buzz matters.** Banter lands more often in the sweet spot and less when
  sober. When drunk, your answer sometimes comes out as a roast no matter
  what you picked.
- Guests wait for you to finish talking before taking their turn.
- Banter never interrupts your own turn.

## Hiding your gift

Hold X during other guests' turns to tuck your gift out of sight.

- **Sneaky:** while you hold it, guests are much less likely to go after your
  gift.
- **Suspicion** builds while you hold X and drains slowly after you let go
  (the HIDE gauge in the corner). Hiding through every guest's turn adds up.
- **Caught:** once suspicion passes the line, everyone can tell. Your gift
  becomes *more* likely to be stolen, even after you let go, until suspicion
  cools back down. Getting caught is a rude vibe flop, and every extra second
  you keep hiding pushes you further toward Obnoxious.
- **Both hands are busy:** you can't grab a drink while hiding.
- Hiding does nothing on your own turn, because nobody can steal from you then.

## Character builder

After the title screen, you build your character before the season starts:
skin tone, hairstyle, hair color, sweater style, sweater color and hat, plus a
"Surprise me" button. A big preview cycles through your expressions. Your
look is saved, so next time the builder opens on it and you can just press A
to go.

- **Parts and tinting:** everyone is a body + sweater + hair + hat, each drawn
  in four grays and recolored with a color ramp, so 6 hairstyles x 4 hair
  colors needs only 6 drawings.
- **Guests use the same parts.** Each guest's look is set in
  `game/content.h`, which makes look-alikes of real party guests easy. A
  hand-drawn full sprite still overrides it.
- **Hair under hats:** hats that cover the head (Santa Hat, Beanie) switch
  the hair to its tucked-in version, if that hairstyle has one; otherwise
  the normal hair is used.
- Options: 4 skin tones; Classic, Long, Curly, Bun, Spiky, Bald, Mohawk; Ginger,
  Blonde, Brown, Black; Plain, Stripes, Snowflake, Reindeer, Tree; Green, Red,
  Navy, Cream, Purple; no hat, Santa Hat, Antlers, Bow, Beanie.

## Side strips

The screen is wider than the game (320x240 around a 240x240 game), so each
side has a strand of string lights that reflects how the night is going: a
gentle twinkle off the party; at the party a chase that speeds up with your
buzz and strands that swing when you're drunk; red flashing past the blackout
line; a gold chase up both sides after your steal; dead lights (one sputtering)
when you're cut off. They're pure feedback: nothing to read, nothing new to
learn.

## Idle screen

- **When:** 30 seconds with no buttons on the title screen or the character
  builder. Never mid-party.
- **What:** a Christmas tree outside at night. Each time, a new tree (height,
  width, tiers, lopsidedness, shade of green), lights (color set, twinkle or
  steady glow), ornaments, a topper (star, angel or bow), the moon's phase and
  position, the stars, and the odd shooting star.
- **Snow is simulated** on a grid of 2x2-pixel cells. Flakes fall and flutter;
  where they land they settle and behave like sticky sand, clinging to gentle
  slopes and sliding off real drops, so they build up on ledges, the topper and
  the ground. Each branch has a random capacity, and an overloaded one dumps
  its snow as a clump that falls through to the branches below. Gusts push the
  falling snow and skate it across the drifts (and shake branches loose), and
  the snowfall wanders between flurries and heavy snow.
- **Then sleep:** after 2 minutes the screen turns off. Any button wakes it to
  the title; during the idle screen, any button goes back where you were, and
  that press does nothing else.

## High scores

- **Top 10 board**, saved on the handheld. Any run worth more than $0 that
  beats the lowest entry (or finds an empty slot) gets on. A tie goes below
  the older score.
- **Cut-off runs count** with what you banked from finished parties, the same
  number the cut-off screen shows.
- **Initials:** 3 letters, arcade-style. Your last initials are remembered.
- **Attract mode:** the title screen and the board take turns.
- **Seeded and resettable:** the board starts from a list in `content.h`.
  Reset with `--reset-scores` on the desktop, or a hidden X + Y hold on the
  title screen that can be switched off for the party.

## Lottery scratchers

End a party holding the Lottery Scratchers and you play a scratch-off to
find out what they're worth.

- **The ticket** has 6 silver spots in a 3x2 grid. **Every ticket wins:**
  exactly one value appears twice, and that pair is your prize. The other
  four spots show other prizes as near-misses.
- **Hold A** over a spot to scratch it off (about half a second); the
  coating speckles away as you go. Joystick moves between spots.
- **Drunk, your hand slips** onto a different spot mid-scratch.
- The moment the second matching spot is uncovered, you win, the rest of the
  ticket reveals itself, and the prize is the gift's value on the results
  screen.
- **The prize table** (values and rarity weights) is `SCRATCH_PRIZES` in
  `game/tuning.h`. The prize is picked when the mini-game starts. Guests
  value the tickets at the table's average payout.
- Mark any gift as a scratcher with `scratch_ticket` in `game/content.h`.

## Gifts

- About 20 possible gifts, and each party draws N of them.
- Each gift has a hidden **value** (shown on open) and a **category**
  (practical / gag / coveted).
- Examples:
  - **Coveted:** $50 gift card, Bluetooth speaker, fancy bourbon, "PTO
    voucher" (forged, but still)
  - **Practical:** heated desk mat, a nice mug, good socks
  - **Gag (negative value):** "Live Laugh Leverage" wall art, a used fidget
    spinner, a single scented candle labeled "Q4 Panic", a regifted fruitcake
    with the original tag still on it, a singing fish plaque
  - **Wildcard:** lottery scratchers (value revealed only at the end)
- Each gift gets a one-line snarky reveal caption in the ticker.
- **Coveted** gifts attract steals from the AI.

## AI guests

Each guest has a personality that changes how they behave.

These are office archetypes, so they're recognizable without being about anyone.

| Guest | Archetype | Behavior |
|---|---|---|
| Middle Manager | The Stealer | Always steals if anything good is unfrozen ("circling back to that speaker") |
| The New Hire | The Opener | Almost never steals; too nervous |
| Sales Guy | The Lush | Grabs drinks constantly; becomes unpredictable |
| HR Rep | The Grudge | Steals from whoever last stole from them, "for documentation purposes" |
| The Intern | The Collector | Fixates on one category, usually anything free |

Guests are drawn randomly, so every party has a different mix. Each one has a
few snarky ticker lines for opening, stealing, getting robbed and drinking.

## Tone

Office-party chaos: snarky guests, cringey gifts, corporate-speak jokes. It
can be a bit edgy, but nothing mean. The jokes punch at office life, not at
people.

## Scoring and round end

At the end of a party:
- **Final gift value**
- **+ Sweet-spot bonus** (time spent in the zone)
- **+ Steal bonus** (per successful steal)
- **+ Holding bonus** (you held a frozen coveted gift)
- **- Penalties** (making a scene)

**Arcade structure:** parties chain into a "Holiday Season". Each new party adds
a guest, makes drinks stronger, and makes the AI cagier. The run ends after
the final party, or the first time you get cut off. There are no extra lives. High scores are saved
to flash, with 3-letter initials of course.

## Decisions

- **Single player vs AI guests.** Pass-and-play is a stretch goal if time
  allows. To keep that door open, turn logic shouldn't assume the player is
  always seat 0 or that there's only one human seat.
- **Hardcore cut-off.** Being cut off ends the entire run.
- **Season shape (built):** 3 parties. Party 1 has you + 3 guests, and each
  party adds one. Drinks get 15% stronger and guests get greedier each party.
- **Your turn has a 15-second timer.** If you dither, you grab a gift off the
  pile (or keep yours during the final swap).
- **Opening picks a random wrapped gift** until the shake-hint mini-game is
  built.
- **Tone:** office-party chaos (see Tone).
- **HUD: needle gauges.** BUZZ and VIBE are half-dials in the top corners,
  each with its good zone straight up. The buzz dial shows a dotted "heading" needle for drinks still
  soaking in, and its needle trembles when drunk. The score and HIDE gauge sit
  between them.
- **Party in-jokes: yes.** Real names, inside jokes and gift ideas from the
  actual party get mixed in with the generic content (details pending from
  the user).

## Party in-jokes (pending content)

All jokey text lives in one content file (`game/content.h`), separate from the
code, so names and lines are easy to edit up to the night before.

- **Named guests:** a real person can replace or "wear" an archetype (e.g.
  "Mike" as Sales Guy) and gets custom ticker lines.
- **Custom gifts:** real gift ideas mixed into the pool, with value, category
  and a reveal caption.
- **Inside-joke lines:** extra remarks dropped into the ticker at random.
- **Easter egg (optional):** a rare event that only makes sense to people at
  the party.

Guardrail: affectionate roasting only. Nothing anyone would be embarrassed to
see over someone's shoulder.
