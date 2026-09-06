# D001 - A repository for apps built on the SDK, and obSCEne is not one


**decided** · 2026-09-04

oops-sdk existed with one consumer. A layer with a single user is under-exercised - the same
reason oops-libs builds every feature - and a homebrew app had nowhere to live but its own
repository or somebody else's. This is that place: the apps this collection builds on its own
SDK, in one repository.

The admission rule is deliberately narrow, the way oops-libs' is: **an app built on the SDK** -
something a person runs on the hardware for its own sake. A repository that takes anything
becomes a place things are dropped.

**obSCEne is exempt, and the reason is the line the whole idea turns on.** obSCEne is a
*probe*: its purpose is to **measure** what the platform does and report it. An app here
**does** something. A thing whose purpose is to find out whether a function works is a probe
and belongs in obSCEne; a thing whose purpose is to put a picture on the screen is an app and
belongs here. Keeping both under one roof is how a measurement gets shipped as a feature, and
it is exactly the class of duplication this collection has been closing rather than opening.

Not a fifth project. Like oops-libs and oops-sdk, this is infrastructure underneath the four:
a repository, not a product. It is built with a real first member - Porthole - rather than
opened empty, so its shape is driven by something that exists rather than guessed.
