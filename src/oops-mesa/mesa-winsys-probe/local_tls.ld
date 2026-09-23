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
