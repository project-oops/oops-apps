/*
 * The FreeType modules this build actually compiles, and no others.
 *
 * `ftinit.c` registers every module named here, so the default `ftmodule.h` - which
 * lists nineteen, including Type 1, CID, PFR, Type 42, Windows FNT, PCF, BDF, the SDF
 * renderers and the SVG one - makes the link ask for drivers whose sources
 * `oops-freetype.mk` deliberately does not build. That is not FreeType being awkward: a
 * normal build compiles all nineteen.
 *
 * Selected with `-DFT_CONFIG_MODULES_H=<ftmodule-oops.h>`, which is FreeType's own
 * documented hook for exactly this (`include/freetype/config/ftheader.h`).
 *
 * The seven below are the minimal TrueType set and match the source list one for one.
 * Adding a module here without adding its sources there gives an undefined symbol at
 * link; adding the sources without naming it here gives a driver FreeType never
 * registers and a font it cannot open. They move together.
 */
FT_USE_MODULE(FT_Module_Class, autofit_module_class)
FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Module_Class, psnames_module_class)
FT_USE_MODULE(FT_Module_Class, pshinter_module_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)
FT_USE_MODULE(FT_Renderer_Class, ft_raster1_renderer_class)
