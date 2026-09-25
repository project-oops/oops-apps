# The controls reference card

A console port has a problem a desktop build does not: **nothing on screen says what the buttons
do.** A desktop game can get away with it because the keys are labelled under the player's
fingers. A pad's are not, and the one thing a player genuinely cannot work out for themselves is
which of four identical face buttons changes the camera.

So every port here should show its bindings. This directory holds the half of that which is the
same in every port; the other half — *which* button does *what* — belongs to the title.

## What is here

| | |
|---|---|
| `controller.svg` | the source: a generic gamepad as line art, 1000×640 |
| `controller.png` | what a title packages, 1024×640, white on transparent |
| `render.sh` | regenerates the PNG; run it after editing the SVG and commit both |
| `preview.png` | the same drawing on a dark background, so a reviewer can see it in a diff |

**It is deliberately not any particular pad.** The shape is the two-grip layout every modern
controller shares, and nothing on it is a trademarked glyph — the face buttons are plain circles,
numbered clockwise from the top. A card that draws somebody's symbols is a card that cannot ship.

**White on transparent, no fills**, so a title tints it to its own menu colour instead of
fighting a baked background.

## How a title uses it

1. Copy `controller.png` into the *packaged* data from the title's `package` rule — **not** into
   the upstream tree, which stays exactly as upstream ships it. Neverball's Makefile is the
   worked example; it lands as `data/gui/oops-controller.png`.
2. Draw it on whatever screen the game already has for help or options, with the bindings beside
   it.
3. Read the bindings from the game's own configuration rather than writing them out as prose, so
   a card that disagrees with the game is not possible.

**Look for an existing screen before building one.** Neverball already had a `Controls` page in
its help state, reachable from the title screen — so its card is a patch to one function rather
than a new state, a new menu entry and a new set of translations. Neverputt has no help screen at
all, which is why it does not have a card yet.

## What a card should say

Actions on one side, buttons on the other, and the actions phrased as what the player wants to do
rather than what the code calls it. Include anything that is **off by default** — a feature
nobody can find is a feature that is not there, which is why Neverball's card mentions motion
tilt and where to switch it on.

Keep it read-only. Rebinding is a much larger screen, and no port here has needed it.
