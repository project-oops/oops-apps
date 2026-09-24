# Shim

**Empty: nothing has been measured yet, so nothing is known to be needed.** This is where the
port answers what upstream expects and the target does not have, without editing upstream:
`shim/include/` goes on the include path ahead of upstream's own headers, and a shim translation
unit supplies a function.

`../../craft/shim/` is the worked example - twenty-three GLFW entry points and four headers, over
oops-sdk's display, input and clock, with GLFW itself never compiled. `../../supertux/shim/` is
the other useful shape: a note recording the one line that is already known to be required,
written before any code.

What lands here is decided by `../docs/PORTING.md`'s first job, not guessed at in advance.
