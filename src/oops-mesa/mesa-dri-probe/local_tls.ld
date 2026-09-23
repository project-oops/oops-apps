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
     * `.eh_frame` is discarded here and deliberately **not** in SELFish's
     * `link/native_eboot.ld`, which every copy of this file is derived from. The two are
     * meant to differ; see selfish#D105.
     *
     * Discarding it there beat the `KEEP` in `oops-deps/libcxx/link/eh-frame.ld`, so
     * `__eh_frame_start` and `__eh_frame_end` came out equal, a freestanding libunwind
     * took a zero-byte range as its whole frame table, and a `throw` reached
     * std::terminate without entering any `catch` - `catch (...)` included, because with
     * no table the unwinder cannot leave the frame that threw.
     *
     * This title links no unwinder - `oops-libunwind.mk` is included by `cxx-throw` and by
     * nothing else - so the section has no reader in the image and keeping it is pure
     * size. On `mesa-demos`, which is the same case, it measured 1.33 MB.
     *
     * If this title ever includes `oops-libunwind.mk`, drop the `.eh_frame` line below in
     * the same change, and check the *width* of the range rather than the presence of its
     * symbols: four non-zero symbols is what passed on every broken build.
     */

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
        *(.gnu.version*)
    }
}
