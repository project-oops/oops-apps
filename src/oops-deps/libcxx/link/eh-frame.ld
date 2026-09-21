/* The four symbols a baremetal libunwind reads its frame table from.
 *
 * `oops-libunwind.mk` builds libunwind with `_LIBUNWIND_IS_BAREMETAL=1`, which replaces the
 * `dl_iterate_phdr` lookup with a fixed range named by these symbols. Nothing defines them by
 * default, so this script does, by bracketing the sections the compiler already emits.
 *
 * # This is `INSERT AFTER`, not a replacement
 *
 * A bare `-T script` *replaces* the default linker script, which for a title would throw away
 * everything `app.mk` and the platform rely on. `INSERT AFTER .text` instead splices these two
 * output sections into the default layout and leaves the rest of it alone. ld.lld supports this
 * and it is the only form that is safe next to `app.mk`'s existing link line.
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
} INSERT AFTER .text;
