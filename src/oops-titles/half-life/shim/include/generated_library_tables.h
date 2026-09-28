/*
 * The libraries linked into this payload, by the name the engine asks for (its base name:
 * `shim/lib_static_oops.c` drops the directory and extension). Each `lib_<name>_exports`
 * is the one global symbol its relocatable object keeps - see the Makefile's
 * `hl_reloc_lib`. This is the table upstream's waf build writes
 * (`scripts/waifulib/xshlib.py:112`), written once by hand because the set is fixed.
 *
 * `hl` is the server: `liblist.gam` names it `dlls/hl.so`.
 */
extern table_t lib_filesystem_stdio_exports[];
extern table_t lib_ref_gl_exports[];
extern table_t lib_menu_exports[];
extern table_t lib_client_exports[];
extern table_t lib_hl_exports[];

static table_t libs[] = {
    {"filesystem_stdio", lib_filesystem_stdio_exports},
    {"ref_gl", lib_ref_gl_exports},
    {"menu", lib_menu_exports},
    {"client", lib_client_exports},
    {"hl", lib_hl_exports},
    {0, 0},
};
