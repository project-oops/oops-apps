# The controls reference card

Nothing on a console port's screen says what the buttons do, and a pad has no labels under the
player's fingers. Every port here shows its bindings. This directory holds the half of that
which is the same in every port; which button does what belongs to the title.

## Contents

| | |
|---|---|
| `controller.svg` | the source: a generic gamepad as line art, 1000×640 |
| `controller.png` | what a title packages, 1024×640, white on transparent |
| `render.sh` | regenerates the PNG; run it after editing the SVG and commit both |
| `preview.png` | the same drawing on a dark background, so a reviewer can see it in a diff |

The drawing is not any particular pad. The shape is the two-grip layout modern controllers share,
and nothing on it is a trademarked glyph - the face buttons are plain circles, numbered clockwise
from the top.

It is white on transparent with no fills, so a title tints it to its own menu colour.

## Two cards per title

Every title shows its controls twice:

- **At start-up**, before the game, from a `controls.txt` beside the title's Makefile. This one is
  shared code (`common/controls_card.h`): `common/controls.mk` builds it in, embeds the table and
  this drawing in the payload, and puts it in front of the title's entry point, so a title opts in
  with the file alone. It stays up for ten seconds or until Cross or Options. The table is written
  by hand, so it states the defaults.
- **In the game**, on a help or options screen the game already has, by a patch to that title.
  This one reads the live bindings, so it follows a rebind. A game with no such screen gets an
  overlay instead (Craft), or only the start-up card.

`controls.txt`:

```
# a comment
R2 = Accelerate
Left stick = Steer
> A note under the table, such as where the in-game card is.
```

Button names are this pad's: Cross, Circle, Square, Triangle, L1, L2, R1, R2, L3, R3, Options,
Create, Touchpad, Left stick, Right stick, D-pad.

## The in-game card

1. Copy `controller.png` into the packaged data from the title's `package` rule, not into the
   upstream tree, which stays as upstream ships it. Neverball's Makefile is the worked example;
   it lands as `data/gui/oops-controller.png`.
2. Draw it on the screen the game already has for help or options, with the bindings beside it.
3. Read the bindings from the game's own configuration rather than writing them out as prose, so
   the card cannot disagree with the game.

A card goes on an existing help screen where the game has one. Neverball's `Controls` page in its
help state is reachable from the title screen, so its card is a patch to one function. Neverputt
has no help screen.

## Card content

Actions on one side, buttons on the other, with the actions phrased as what the player wants to
do rather than what the code calls it. The card includes anything that is off by default:
Neverball's card mentions motion tilt and where to switch it on.

The card is read-only; it does not rebind.
