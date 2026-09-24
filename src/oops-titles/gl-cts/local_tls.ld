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
     * `.eh_frame` is **not** discarded here, and the copy of this comment that said it was
     * is what cost a hardware run on 2026-09-24.
     *
     * The other copies of this file - `mesa-cube`, `mesa-demos`, the two Mesa probes - do
     * discard it, deliberately and correctly: they link no unwinder, `oops-libunwind.mk` is
     * included by `cxx-throw` and by nothing else there, so the section has no reader in the
     * image and keeping it is pure size. On `mesa-demos` it measured 1.33 MB. SELFish's
     * `link/native_eboot.ld`, which every copy of this file is derived from, keeps it; the
     * two are meant to differ, and selfish#D105 says why.
     *
     * **This title is the exception that comment anticipated.** It sets
     * `OOPS_CXX_EXCEPTIONS = 1` and includes `oops-libunwind.mk`, because dEQP is built on
     * exceptions from end to end: `tcu::NotSupportedError` is how a test says it did not
     * run, `tcu::ResourceError` is how it says a file was missing, and `tcuMain.cpp:93` is
     * a `catch (const std::exception &)` around the entire run.
     *
     * Discarding the section beat the `KEEP` in `oops-deps/libcxx/link/eh-frame.ld`, so
     * `__eh_frame_start` and `__eh_frame_end` came out equal, the freestanding libunwind
     * (`_LIBUNWIND_IS_BAREMETAL` scans between those two rather than using
     * `dl_iterate_phdr`) took a zero-byte range as its whole frame table, and the unwinder
     * could not leave the frame that threw. On hardware that was:
     *
     *     [OOPS-GL] the DRI frontend would not create a screen
     *     [cxx] std::terminate reached - an exception found no handler, or a handler threw
     *     # reason:  abort is called(system)
     *
     * - a `throw` from `glu::createRenderContext` passing straight through `tcuMain`'s
     * `catch` into `abort`. **Every dEQP exception was fatal**, so the suite could never
     * have reported a NotSupported, which is most of what a first run produces.
     *
     * The inherited comment named this exact case and said what to do about it. It was
     * read as describing the other titles.
     *
     * **Check the width of the range, not the presence of its symbols.** Four non-zero
     * symbols is what passed on every broken build; `__eh_frame_end - __eh_frame_start`
     * is the only arm that fails when this is wrong.
     */

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.gnu.version*)
    }
}
