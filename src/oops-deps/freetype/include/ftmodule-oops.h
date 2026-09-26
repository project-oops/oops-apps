/*
 * The FreeType modules this build compiles, and no others.
 *
 * `ftinit.c` registers every module named here; the default `ftmodule.h` names drivers
 * whose sources `oops-freetype.mk` does not build. Selected with
 * `-DFT_CONFIG_MODULES_H=<ftmodule-oops.h>` (`include/freetype/config/ftheader.h`).
 *
 * This is the minimal TrueType set and matches the source list in `oops-freetype.mk`
 * one for one: a module without sources is an undefined symbol at link, sources
 * without a module are a driver FreeType never registers.
 */
FT_USE_MODULE(FT_Module_Class, autofit_module_class)
FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Module_Class, psnames_module_class)
FT_USE_MODULE(FT_Module_Class, pshinter_module_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)
FT_USE_MODULE(FT_Renderer_Class, ft_raster1_renderer_class)
