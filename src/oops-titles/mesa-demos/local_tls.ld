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
     * .eh_frame is deliberately NOT discarded, and this block must stay in step with
     * SELFish's `link/native_eboot.ld`, which this file is a copy of plus a PT_TLS.
     *
     * `oops-deps/libcxx/link/eh-frame.ld` brackets .eh_frame with __eh_frame_start and
     * __eh_frame_end, which is where a freestanding libunwind (_LIBUNWIND_IS_BAREMETAL)
     * reads its frame table from. A /DISCARD/ entry beats that KEEP, so the two symbols
     * came out equal and the table was zero bytes long. With no frame table the unwinder
     * cannot leave the throwing frame at all, so a `throw` reached std::terminate rather
     * than any `catch` - including `catch (...)`, which is what made it look like the
     * handler tables were wrong when the walk had never started. The loader's
     * "corrupted eh_frame_hdr or eh_frame" line in every mesa-demos log is the same fact.
     */

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.gnu.version*)
    }
}
