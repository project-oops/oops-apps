/* The four symbols a baremetal libunwind reads its frame table from.
 *
 * `oops-libunwind.mk` builds libunwind with `_LIBUNWIND_IS_BAREMETAL=1`, which replaces the
 * `dl_iterate_phdr` lookup with a fixed range named by these symbols. Nothing defines them by
 * default, so this script does, by bracketing the sections the compiler already emits.
 *
 * # This is `INSERT AFTER`, not a replacement
 *
 * A bare `-T script` *replaces* the default linker script, which for a title would throw away
 * everything `app.mk` and the platform rely on. `INSERT AFTER` instead splices these two output
 * sections into the default layout and leaves the rest of it alone. ld.lld supports this and it
 * is the only form that is safe next to `app.mk`'s existing link line.
 *
 * # `AFTER .rodata`, and this is the whole point of the file
 *
 * **The frame table is read as data, so it must not land in the text segment.** This platform
 * maps a title's text execute-only: the CPU may fetch instructions from it and may not load from
 * it. `SELFish`'s `native_eboot.ld` puts `.text` and `.plt` in a `FLAGS(5)` PT_LOAD and
 * everything readable - `.rodata`, `.gcc_except_table` and the rest - in the `FLAGS(6)` one.
 *
 * This said `INSERT AFTER .text` until 2026-09-23, which put `.eh_frame` and `.eh_frame_hdr` in
 * the execute-only segment. libunwind found them, tried to read them, and the kernel killed the
 * title with
 *
 *     exception: 0xa0020328 (SYSTEM_XO_VIOLATION)
 *     fault address: 00000000004403fc (xotext:"eboot.bin"+0x403fc)
 *
 * where `0x403fc` is `__eh_frame_end` / `__eh_frame_hdr_start` exactly - the first byte of the
 * index, faulting on the first load. `.gcc_except_table` was never affected, because nothing
 * names it here and the default layout had already put it beside `.rodata` in the readable
 * segment. That asymmetry is what made this look like a handler-table problem for two probes:
 * the LSDA was always fine and the frame table was never readable.
 *
 * `AFTER .rodata` puts both sections next to `.gcc_except_table`, which is where the other
 * unwind table already lives. An output section with no `:phdr` of its own inherits the previous
 * section's, so following `.rodata` is what assigns them to the readable segment - the ordering
 * is load-bearing, not cosmetic.
 *
 * # Why `KEEP` matters here
 *
 * `.eh_frame` is reached from the unwinder rather than from any call, so `--gc-sections` has no
 * reason to believe anything references it. Dropping it would leave a title that links, loads,
 * throws, finds an empty table, and treats the first frame as the end of the stack - which
 * presents as a silent `std::terminate` a long way from the `throw`.
 *
 * # The trap this is written against
 *
 * `app.mk`'s `TARGET_LDFLAGS` carries `-Wl,--unresolved-symbols=ignore-all`. So if these symbols
 * were *not* defined, the link would still succeed and they would resolve to zero - libunwind
 * would compute a zero-length frame range and every `throw` would abort. The link is therefore
 * not evidence that this file was used. **Check the built ELF**:
 *
 *     nm <title>.elf | grep __eh_frame_
 *
 * Four symbols with non-zero addresses is the evidence; a clean link is not. This is the same
 * shape as the `nm -u` discipline oops-sdk records for cross-module calls, and it is why this
 * comment is longer than the script.
 *
 * # And four non-zero symbols are not enough either
 *
 * The base script this splices into is SELFish's `link/native_eboot.ld`, or one of the
 * `local_tls.ld` copies of it, and until 2026-09-23 every one of them listed
 * `*(.eh_frame .eh_frame_hdr)` in `/DISCARD/`. A `/DISCARD/` entry beats the `KEEP` above, so
 * the input sections went nowhere and both symbols landed on the *same* address - four
 * non-zero values, a zero-length table, and `nm` reporting what looks like success.
 *
 * So the check that means something is the width, not the presence:
 *
 *     nm <title>.elf | grep __eh_frame_       # __eh_frame_end must exceed __eh_frame_start
 *
 * `oops-utilities/cxx-throw`'s `make check` asserts exactly that. What the zero-length table
 * looked like from the outside was a `throw` reaching std::terminate without entering any
 * handler, `catch (...)` included - because with no frame table the unwinder cannot leave the
 * throwing frame, so it never gets as far as asking what the handler tables say.
 */
SECTIONS
{
  .eh_frame : {
    __eh_frame_start = .;
    KEEP(*(.eh_frame))
    __eh_frame_end = .;
  }

  .eh_frame_hdr : {
    __eh_frame_hdr_start = .;
    KEEP(*(.eh_frame_hdr))
    __eh_frame_hdr_end = .;
  }
} INSERT AFTER .rodata;
