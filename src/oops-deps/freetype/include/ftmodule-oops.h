/*
 * The FreeType modules this build compiles, and no others.
 *
 * `ftinit.c` registers every module named here; the default `ftmodule.h` names drivers
 * whose sources `oops-freetype.mk` does not build. Selected with
 * `-DFT_CONFIG_MODULES_H=<ftmodule-oops.h>` (`include/freetype/config/ftheader.h`).
 *
 * TrueType and OpenType-CFF, and it matches the source list in `oops-freetype.mk` one
 * for one: a module without sources is an undefined symbol at link, sources without a
 * module are a driver FreeType never registers. CFF (with `psaux`, which parses its
 * charstrings) is here because SuperTuxKart's fonts are `.otf`; without it they load
 * as `FT_Err_Unknown_File_Format`.
 */
FT_USE_MODULE(FT_Module_Class, autofit_module_class)
FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Driver_ClassRec, cff_driver_class)
FT_USE_MODULE(FT_Module_Class, psaux_module_class)
FT_USE_MODULE(FT_Module_Class, psnames_module_class)
FT_USE_MODULE(FT_Module_Class, pshinter_module_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)
FT_USE_MODULE(FT_Renderer_Class, ft_raster1_renderer_class)
