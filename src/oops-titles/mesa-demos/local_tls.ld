/*
 * Linker script for a native current-generation (PS5) console executable.
 *
 * Based at offset 0 (ASLR-relocatable, ET_DYN / 0xFE10).
 * No PT_INTERP: the native kernel invokes /libexec/ld-elf.so.1 directly.
 */

PHDRS
{
    text      PT_LOAD FLAGS(5);   /* read + execute */
    data      PT_LOAD FLAGS(6);   /* read + write */
    dynlibdata 0x61000000 FLAGS(4);
    dynamic   PT_DYNAMIC FLAGS(6);
    procparam 0x61000001 FLAGS(4);
    tls       PT_TLS FLAGS(4);
}

SECTIONS
{
    . = 0;

    .text           : { *(.text .text.*) }    :text
    .plt            : { *(.plt .plt.*) }      :text

    . = ALIGN(0x4000);
    .hash           : { *(.hash) }            :data
    .gnu.hash       : { *(.gnu.hash) }        :data
    .dynsym         : { *(.dynsym) }          :data
    .dynstr         : { *(.dynstr) }          :data
    .rela.dyn       : { *(.rela.dyn) }        :data
    .rela.plt       : { *(.rela.plt) }        :data
    .rodata         : { *(.rodata .rodata.*) } :data

    /*
     * C++ dynamic-initialiser tables. There are no crt start-up objects here to walk these, so a
     * title runs them itself (oops-mesa `abi.c`, `oops_mesa_run_init_array`) using the bounds
     * symbols placed here. Without this, dynamically-initialised Mesa globals - ACO's opcode
     * table among them - stay zeroed (oops-mesa worklog 062). Writable because each entry carries
     * a RELATIVE relocation the loader applies.
     */
    . = ALIGN(8);
    .preinit_array  : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } :data
    .init_array     : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data
    .fini_array     : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data

    .sce_process_param : { KEEP(*(.sce_process_param)) } :data :procparam

    .tdata          : { *(.tdata .tdata.*) }   :data :tls
    .tbss           : { *(.tbss .tbss.* .tcommon) } :data :tls

    .data           : { *(.data .data.*) }    :data
    .got            : { *(.got) }             :data
    .got.plt        : { *(.got.plt) }         :data
    .bss            : { *(.bss .bss.* COMMON) } :data

    .sce_dynlibdata : { KEEP(*(.sce_dynlibdata)) } :dynlibdata
    .dynamic        : { *(.dynamic); . += 0x20000; } :data :dynamic

    /*
     * # `.eh_frame` is discarded HERE, and must not be in SELFish's copy of this script
     *
     * This file is SELFish's `link/native_eboot.ld` plus a PT_TLS. SELFish's copy dropped
     * `*(.eh_frame .eh_frame_hdr)` from this block on 2026-09-23 (selfish#D105) and must
     * stay that way: a `/DISCARD/` entry beats the `KEEP` in
     * `oops-deps/libcxx/link/eh-frame.ld`, so it left a freestanding libunwind
     * (`_LIBUNWIND_IS_BAREMETAL`) with `__eh_frame_start == __eh_frame_end` and a
     * zero-byte frame table. With no frame table the unwinder cannot step out of the
     * frame that threw, so `cxx-throw` reached std::terminate without entering any
     * `catch` - `catch (...)` included, which is what made it read as broken handler
     * tables when the walk had never started.
     *
     * **This title is not that title, and the entry stays here because it was measured.**
     * mesa-demos links no unwinder: `oops-libunwind.mk` is included by `cxx-throw` and by
     * nothing else, and `grep -c libunwind build/mesa-demos.map` is 0. Keeping `.eh_frame`
     * here cost 1,386,512 bytes on the eboot - 26.7 MB to 28.1 MB, measured on `textures`
     * - for a table with no reader in the image. That is what `/DISCARD/` is for.
     *
     * So the rule is: **discard it exactly where nothing in the linked image reads it.**
     * If this title ever includes `oops-libunwind.mk`, delete the `.eh_frame` line below
     * in the same change, and check the width rather than the symbols:
     *
     *     grep __eh_frame_ build/mesa-demos.map    # end must exceed start
     *
     * Four non-zero symbols is not evidence; `cxx-throw`'s `make check` asserted exactly
     * that and passed on every broken build.
     */

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
        *(.gnu.version*)
    }
}
