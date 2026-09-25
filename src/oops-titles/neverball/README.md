# Neverball

<p align="center">
  <img src="assets/logo.png" alt="Neverball" width="200">
</p>

Tilt the floor, not the ball — an open-source arcade balance game, ported to the console.

## About

Neverball rolls a ball through an obstacle course by tilting the floor, racing the clock and
collecting coins. It is one of the collection's ported upstream titles, tracked against a pinned
release.

- **Pinned to a finished release** — `neverball-1.6.0`, by commit hash rather than tag, so the
  tree a build starts from can't move underneath it.
- **Fetched, not vendored** — the upstream sources are pulled on demand via `upstream.lock`; only
  the port's own `patches/` live here.

## Screenshot

<p align="center">
  <img src="assets/demo.gif" alt="Neverball running on Prospero, captured over JetKVM" width="600">
</p>

## On a console

Four things this port adds that upstream has no reason to. Each is here rather than in a patch
wherever that was possible, because a patch is rebased onto every upstream revision forever.

- **Motion tilt** — tilt the pad to tilt the floor. **Off until you turn it on**, in Options; the
  `Controls` page says so, because a feature that is off by default and mentioned nowhere is a
  feature that is not there. It costs *no patch*: Neverball has had a tilt-sensor interface since
  it supported the Wiimote, upstream's Makefile picks one of three backends, and this port adds a
  fourth in [`shim/nb_tilt_dualsense.c`](shim/nb_tilt_dualsense.c). Axis order, inversion,
  sensitivity and deadzone are knobs in `/app0/oops-input` — which physical axis is pitch cannot
  be settled by reading, and changing a knob costs a file copy rather than a rebuild.
- **Rumble** on a bounce, at the strength the bounce sound is mixed at. The decay is shared, in
  [`common/haptics.c`](../../../common/haptics.c); this title contributes one call.
- **A controls reference card**, on the `Controls` page of the help screen — reachable from the
  title screen at any time. The wireframe is the shared one from
  [`common/assets/controls/`](../../../common/assets/controls/), and the bindings are read from
  the same configuration the play state tests, so the card cannot disagree with the game.
- **Settings persist.** Upstream saves once, after the main loop returns; a console title is
  closed from the dashboard and never gets there.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

---

<sub>Logo: the Neverball ball, from the upstream project's own icon, recomposited on black. Neverball is GPL v2.</sub>
