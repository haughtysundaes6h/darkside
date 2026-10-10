/* ft2build.h - Minimal wrapper for FreeType 2 */
#ifndef FT2BUILD_H_
#define FT2BUILD_H_

#define FT_CONFIG_CONFIG_H  <freetype/config/ftconfig.h>
#define FT_CONFIG_STANDARD_LIBRARY_H  <freetype/config/ftstdlib.h>
#define FT_CONFIG_OPTIONS_H  <freetype/config/ftoption.h>
#define FT_CONFIG_MODULES_H  <freetype/config/ftmodule.h>

/* Define all FreeType macros */
#define FT_FREETYPE_H   <freetype/freetype.h>
#define FT_MODULE_H     <freetype/ftmodapi.h>
#define FT_GLYPH_H      <freetype/ftglyph.h>
#define FT_SYNTHESIS_H  <freetype/ftsynth.h>
#define FT_OTSVG_H      <freetype/otsvg.h>
#define FT_BBOX_H       <freetype/ftbbox.h>

/* Define C++ compatibility macros */
#ifdef __cplusplus
#define FT_BEGIN_HEADER  extern "C" {
#define FT_END_HEADER    }
#else
#define FT_BEGIN_HEADER  /* nothing */
#define FT_END_HEADER    /* nothing */
#endif

#endif /* FT2BUILD_H_ */
