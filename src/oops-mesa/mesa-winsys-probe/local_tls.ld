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
     * `.eh_frame` is discarded: this title links no unwinder, so nothing reads it. SELFish's
     * `link/native_eboot.ld` keeps it (selfish#D105). A title that links
     * `oops-libunwind.mk` keeps it too: a discard beats the `KEEP` in
     * `oops-deps/libcxx/link/eh-frame.ld` and leaves a zero-byte frame table.
     */

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
        *(.gnu.version*)
    }
}
