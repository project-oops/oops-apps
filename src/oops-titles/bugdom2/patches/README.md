# Patches

**Empty, and empty is the goal.** `common/upstream-fetch.sh` applies every `*.patch` here in sorted
order after checking out `UPSTREAM_REV`, and the set of them is part of the fetch stamp, so adding or
removing one re-fetches.

A patch is the right tool when upstream's own source is wrong *about this platform* — an `#if` whose
platform list does not know about us, an include that assumes a header the target lacks. It is the
wrong tool for anything that can be answered from outside upstream's tree, which is what `shim/` and
the build's defines are for.

Craft carries one, and only because a payload has no working directory for a relative asset path to
be relative to. That is the bar.
