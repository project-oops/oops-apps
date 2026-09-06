# oops-apps documentation

The homebrew this collection builds on [oops-sdk](https://github.com/project-oops/oops-sdk):
programs that run on the hardware and use the SDK's subsystems. Not one of the four - a
repository rather than a project, beside oops-libs and oops-sdk.

New here? The [root README](../README.md) has the admission rule, the apps-versus-probes line
that keeps obSCEne separate, and how an app builds.

## The apps

- **[porthole](../porthole/README.md)** - the target half of the capture-and-input path; its
  host half is [Prosperous](https://github.com/project-oops/Prosperous). The first member.
- **[system-panel](../system-panel/README.md)** - what the machine is, drawn to the screen.
  The smallest app that proves the SDK reaches hardware end to end.

## Project memory

- [DECISIONS.md](DECISIONS.md) - a generated index over `decisions/`, one file per entry.
  Why the repository exists, why obSCEne is exempt, and how the base layer was
  promoted from obSCEne into oops-sdk (closing D002).

## The words

Vocabulary is the collection's, not this repository's:

- [the collection's glossary](https://github.com/project-oops/OOPS/blob/main/docs/GLOSSARY.md) - standard ELF, `DT_`/`PT_`, and the cross-repository word collisions
- [oops-sdk](https://github.com/project-oops/oops-sdk) - the subsystems an app calls
- [SELFish](https://github.com/project-oops/SELFish/blob/main/docs/GLOSSARY.md) - payload, title, package, the generation split

**payload** and **target** are defined for all repositories in
[CONVENTIONS.md section 2](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md#the-words-for-our-own-layers).
An app here is a **payload** built for the **target**.

Shared rules - provenance, naming, decision logs, honest failure, gates - are in
[the OOPS conventions](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md) and
not restated here.
