//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//	claim that you wrote the original software. If you use this software
//	in a product, an acknowledgment in the product documentation would be
//	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef FONT_H
#define FONT_H

#ifdef __cplusplus
extern "C" {
#endif

// To make the implementation private to the file that generates the implementation
#ifdef FONT_STATIC
#define FONT_DEF static
#else
#define FONT_DEF extern
#endif

#define FONT_INVALID -1

enum FONTflags {
	FONT_ZERO_TOPLEFT = 1,
	FONT_ZERO_BOTTOMLEFT = 2,
};

enum FONTalign {
	// Horizontal align
	FONT_ALIGN_LEFT 	= 1<<0,	// Default
	FONT_ALIGN_CENTER 	= 1<<1,
	FONT_ALIGN_RIGHT 	= 1<<2,
	// Vertical align
	FONT_ALIGN_TOP 		= 1<<3,
	FONT_ALIGN_MIDDLE	= 1<<4,
	FONT_ALIGN_BOTTOM	= 1<<5,
	FONT_ALIGN_BASELINE	= 1<<6, // Default
};

enum FONTerrorCode {
	// Font atlas is full.
	FONT_ATLAS_FULL = 1,
	// Scratch memory used to render glyphs is full, requested size reported in 'val', you may need to bump up FONT_SCRATCH_BUF_SIZE.
	FONT_SCRATCH_FULL = 2,
	// Calls to fontPushState has created too large stack, if you need deep state stack bump up FONT_MAX_STATES.
	FONT_STATES_OVERFLOW = 3,
	// Trying to pop too many states fontPopState().
	FONT_STATES_UNDERFLOW = 4,
};

struct FONTparams {
	int width, height;
	unsigned char flags;
	void* userPtr;
	int (*renderCreate)(void* uptr, int width, int height);
	int (*renderResize)(void* uptr, int width, int height);
	void (*renderUpdate)(void* uptr, int* rect, const unsigned char* data);
	void (*renderDraw)(void* uptr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
	void (*renderDelete)(void* uptr);
};
typedef struct FONTparams FONTparams;

struct FONTquad
{
	float x0,y0,s0,t0;
	float x1,y1,s1,t1;
};
typedef struct FONTquad FONTquad;

struct FONTtextIter {
	float x, y, nextx, nexty, scale, spacing;
	unsigned int codepoint;
	short isize, iblur;
	struct FONTfont* font;
	int prevGlyphIndex;
	const char* str;
	const char* next;
	const char* end;
	unsigned int utf8state;
};
typedef struct FONTtextIter FONTtextIter;

typedef struct FONTcontext FONTcontext;

// Contructor and destructor.
FONT_DEF FONTcontext* fontCreateInternal(FONTparams* params);
FONT_DEF void fontDeleteInternal(FONTcontext* s);

FONT_DEF void fontSetErrorCallback(FONTcontext* s, void (*callback)(void* uptr, int error, int val), void* uptr);
// Returns current atlas size.
FONT_DEF void fontGetAtlasSize(FONTcontext* s, int* width, int* height);
// Expands the atlas size. 
FONT_DEF int fontExpandAtlas(FONTcontext* s, int width, int height);
// Resets the whole stash.
FONT_DEF int fontResetAtlas(FONTcontext* stash, int width, int height);

// Add fonts
FONT_DEF int fontAddFont(FONTcontext* s, const char* name, const char* path);
FONT_DEF int fontAddFontMem(FONTcontext* s, const char* name, unsigned char* data, int ndata, int freeData);
FONT_DEF int fontGetFontByName(FONTcontext* s, const char* name);
FONT_DEF int fontAddFallbackFont(FONTcontext* stash, int base, int fallback);

// State handling
FONT_DEF void fontPushState(FONTcontext* s);
FONT_DEF void fontPopState(FONTcontext* s);
FONT_DEF void fontClearState(FONTcontext* s);

// State setting
FONT_DEF void fontSetSize(FONTcontext* s, float size);
FONT_DEF void fontSetColor(FONTcontext* s, unsigned int color);
FONT_DEF void fontSetSpacing(FONTcontext* s, float spacing);
FONT_DEF void fontSetBlur(FONTcontext* s, float blur);
FONT_DEF void fontSetAlign(FONTcontext* s, int align);
FONT_DEF void fontSetFont(FONTcontext* s, int font);

// Draw text
FONT_DEF float fontDrawText(FONTcontext* s, float x, float y, const char* string, const char* end);

// Measure text
FONT_DEF float fontTextBounds(FONTcontext* s, float x, float y, const char* string, const char* end, float* bounds);
FONT_DEF void fontLineBounds(FONTcontext* s, float y, float* miny, float* maxy);
FONT_DEF void fontVertMetrics(FONTcontext* s, float* ascender, float* descender, float* lineh);

// Text iterator
FONT_DEF int fontTextIterInit(FONTcontext* stash, FONTtextIter* iter, float x, float y, const char* str, const char* end);
FONT_DEF int fontTextIterNext(FONTcontext* stash, FONTtextIter* iter, struct FONTquad* quad);

// Pull texture changes
FONT_DEF const unsigned char* fontGetTextureData(FONTcontext* stash, int* width, int* height);
FONT_DEF int fontValidateTexture(FONTcontext* s, int* dirty);

// Draws the stash texture for debugging
FONT_DEF void fontDrawDebug(FONTcontext* s, float x, float y);

#ifdef __cplusplus
}
#endif

#endif // FONT_H


#ifdef FONTSTASH_IMPLEMENTATION

#define FONT_NOTUSED(v)  (void)sizeof(v)

#ifdef FONT_USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <math.h>

struct FONTttFontImpl {
	FT_Face font;
};
typedef struct FONTttFontImpl FONTttFontImpl;

static FT_Library ftLibrary;

static int font__tt_init()
{
	FT_Error ftError;
	FONT_NOTUSED(context);
	ftError = FT_Init_FreeType(&ftLibrary);
	return ftError == 0;
}

static int font__tt_loadFont(FONTcontext *context, FONTttFontImpl *font, unsigned char *data, int dataSize)
{
	FT_Error ftError;
	FONT_NOTUSED(context);

	//font->font.userdata = stash;
	ftError = FT_New_Memory_Face(ftLibrary, (const FT_Byte*)data, dataSize, 0, &font->font);
	return ftError == 0;
}

static void font__tt_getFontVMetrics(FONTttFontImpl *font, int *ascent, int *descent, int *lineGap)
{
	*ascent = font->font->ascender;
	*descent = font->font->descender;
	*lineGap = font->font->height - (*ascent - *descent);
}

static float font__tt_getPixelHeightScale(FONTttFontImpl *font, float size)
{
	return size / (font->font->ascender - font->font->descender);
}

static int font__tt_getGlyphIndex(FONTttFontImpl *font, int codepoint)
{
	return FT_Get_Char_Index(font->font, codepoint);
}

static int font__tt_buildGlyphBitmap(FONTttFontImpl *font, int glyph, float size, float scale,
							  int *advance, int *lsb, int *x0, int *y0, int *x1, int *y1)
{
	FT_Error ftError;
	FT_GlyphSlot ftGlyph;
	FT_Fixed advFixed;
	FONT_NOTUSED(scale);

	ftError = FT_Set_Pixel_Sizes(font->font, 0, (FT_UInt)(size * (float)font->font->units_per_EM / (float)(font->font->ascender - font->font->descender)));
	if (ftError) return 0;
	ftError = FT_Load_Glyph(font->font, glyph, FT_LOAD_RENDER);
	if (ftError) return 0;
	ftError = FT_Get_Advance(font->font, glyph, FT_LOAD_NO_SCALE, &advFixed);
	if (ftError) return 0;
	ftGlyph = font->font->glyph;
	*advance = (int)advFixed;
	*lsb = (int)ftGlyph->metrics.horiBearingX;
	*x0 = ftGlyph->bitmap_left;
	*x1 = *x0 + ftGlyph->bitmap.width;
	*y0 = -ftGlyph->bitmap_top;
	*y1 = *y0 + ftGlyph->bitmap.rows;
	return 1;
}

static void font__tt_renderGlyphBitmap(FONTttFontImpl *font, unsigned char *output, int outWidth, int outHeight, int outStride,
								float scaleX, float scaleY, int glyph)
{
	FT_GlyphSlot ftGlyph = font->font->glyph;
	int ftGlyphOffset = 0;
	int x, y;
	FONT_NOTUSED(outWidth);
	FONT_NOTUSED(outHeight);
	FONT_NOTUSED(scaleX);
	FONT_NOTUSED(scaleY);
	FONT_NOTUSED(glyph);	// glyph has already been loaded by font__tt_buildGlyphBitmap

	for ( y = 0; y < ftGlyph->bitmap.rows; y++ ) {
		for ( x = 0; x < ftGlyph->bitmap.width; x++ ) {
			output[(y * outStride) + x] = ftGlyph->bitmap.buffer[ftGlyphOffset++];
		}
	}
}

static int font__tt_getGlyphKernAdvance(FONTttFontImpl *font, int glyph1, int glyph2)
{
	FT_Vector ftKerning;
	FT_Get_Kerning(font->font, glyph1, glyph2, FT_KERNING_DEFAULT, &ftKerning);
	return (int)((ftKerning.x + 32) >> 6);  // Round up and convert to integer
}

#else

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
static void* font__tmpalloc(size_t size, void* up);
static void font__tmpfree(void* ptr, void* up);
#define STBTT_malloc(x,u)    font__tmpalloc(x,u)
#define STBTT_free(x,u)      font__tmpfree(x,u)
#include "stb_truetype.h"

struct FONTttFontImpl {
	stbtt_fontinfo font;
};
typedef struct FONTttFontImpl FONTttFontImpl;

static int font__tt_init(FONTcontext *context)
{
	FONT_NOTUSED(context);
	return 1;
}

static int font__tt_loadFont(FONTcontext *context, FONTttFontImpl *font, unsigned char *data, int dataSize)
{
	int stbError;
	FONT_NOTUSED(dataSize);

	font->font.userdata = context;
	stbError = stbtt_InitFont(&font->font, data, 0);
	return stbError;
}

static void font__tt_getFontVMetrics(FONTttFontImpl *font, int *ascent, int *descent, int *lineGap)
{
	stbtt_GetFontVMetrics(&font->font, ascent, descent, lineGap);
}

static float font__tt_getPixelHeightScale(FONTttFontImpl *font, float size)
{
	return stbtt_ScaleForPixelHeight(&font->font, size);
}

static int font__tt_getGlyphIndex(FONTttFontImpl *font, int codepoint)
{
	return stbtt_FindGlyphIndex(&font->font, codepoint);
}

static int font__tt_buildGlyphBitmap(FONTttFontImpl *font, int glyph, float size, float scale,
							  int *advance, int *lsb, int *x0, int *y0, int *x1, int *y1)
{
	FONT_NOTUSED(size);
	stbtt_GetGlyphHMetrics(&font->font, glyph, advance, lsb);
	stbtt_GetGlyphBitmapBox(&font->font, glyph, scale, scale, x0, y0, x1, y1);
	return 1;
}

static void font__tt_renderGlyphBitmap(FONTttFontImpl *font, unsigned char *output, int outWidth, int outHeight, int outStride,
								float scaleX, float scaleY, int glyph)
{
	stbtt_MakeGlyphBitmap(&font->font, output, outWidth, outHeight, outStride, scaleX, scaleY, glyph);
}

static int font__tt_getGlyphKernAdvance(FONTttFontImpl *font, int glyph1, int glyph2)
{
	return stbtt_GetGlyphKernAdvance(&font->font, glyph1, glyph2);
}

#endif

#ifndef FONT_SCRATCH_BUF_SIZE
#	define FONT_SCRATCH_BUF_SIZE 64000
#endif
#ifndef FONT_HASH_LUT_SIZE
#	define FONT_HASH_LUT_SIZE 256
#endif
#ifndef FONT_INIT_FONTS
#	define FONT_INIT_FONTS 4
#endif
#ifndef FONT_INIT_GLYPHS
#	define FONT_INIT_GLYPHS 256
#endif
#ifndef FONT_INIT_ATLAS_NODES
#	define FONT_INIT_ATLAS_NODES 256
#endif
#ifndef FONT_VERTEX_COUNT
#	define FONT_VERTEX_COUNT 1024
#endif
#ifndef FONT_MAX_STATES
#	define FONT_MAX_STATES 20
#endif
#ifndef FONT_MAX_FALLBACKS
#	define FONT_MAX_FALLBACKS 20
#endif

static unsigned int font__hashint(unsigned int a)
{
	a += ~(a<<15);
	a ^=  (a>>10);
	a +=  (a<<3);
	a ^=  (a>>6);
	a += ~(a<<11);
	a ^=  (a>>16);
	return a;
}

static int font__mini(int a, int b)
{
	return a < b ? a : b;
}

static int font__maxi(int a, int b)
{
	return a > b ? a : b;
}

struct FONTglyph
{
	unsigned int codepoint;
	int index;
	int next;
	short size, blur;
	short x0,y0,x1,y1;
	short xadv,xoff,yoff;
};
typedef struct FONTglyph FONTglyph;

struct FONTfont
{
	FONTttFontImpl font;
	char name[64];
	unsigned char* data;
	int dataSize;
	unsigned char freeData;
	float ascender;
	float descender;
	float lineh;
	FONTglyph* glyphs;
	int cglyphs;
	int nglyphs;
	int lut[FONT_HASH_LUT_SIZE];
	int fallbacks[FONT_MAX_FALLBACKS];
	int nfallbacks;
};
typedef struct FONTfont FONTfont;

struct FONTstate
{
	int font;
	int align;
	float size;
	unsigned int color;
	float blur;
	float spacing;
};
typedef struct FONTstate FONTstate;

struct FONTatlasNode {
	short x, y, width;
};
typedef struct FONTatlasNode FONTatlasNode;

struct FONTatlas
{
	int width, height;
	FONTatlasNode* nodes;
	int nnodes;
	int cnodes;
};
typedef struct FONTatlas FONTatlas;

struct FONTcontext
{
	FONTparams params;
	float itw,ith;
	unsigned char* texData;
	int dirtyRect[4];
	FONTfont** fonts;
	FONTatlas* atlas;
	int cfonts;
	int nfonts;
	float verts[FONT_VERTEX_COUNT*2];
	float tcoords[FONT_VERTEX_COUNT*2];
	unsigned int colors[FONT_VERTEX_COUNT];
	int nverts;
	unsigned char* scratch;
	int nscratch;
	FONTstate states[FONT_MAX_STATES];
	int nstates;
	void (*handleError)(void* uptr, int error, int val);
	void* errorUptr;
};

#ifdef STB_TRUETYPE_IMPLEMENTATION

static void* font__tmpalloc(size_t size, void* up)
{
	unsigned char* ptr;
	FONTcontext* stash = (FONTcontext*)up;

	// 16-byte align the returned pointer
	size = (size + 0xf) & ~0xf;

	if (stash->nscratch+(int)size > FONT_SCRATCH_BUF_SIZE) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONT_SCRATCH_FULL, stash->nscratch+(int)size);
		return NULL;
	}
	ptr = stash->scratch + stash->nscratch;
	stash->nscratch += (int)size;
	return ptr;
}

static void font__tmpfree(void* ptr, void* up)
{
	(void)ptr;
	(void)up;
	// empty
}

#endif // STB_TRUETYPE_IMPLEMENTATION

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define FONT_UTF8_ACCEPT 0
#define FONT_UTF8_REJECT 12

static unsigned int font__decutf8(unsigned int* state, unsigned int* codep, unsigned int byte)
{
	static const unsigned char utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	unsigned int type = utf8d[byte];

	*codep = (*state != FONT_UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state + type];
	return *state;
}

// Atlas based on Skyline Bin Packer by Jukka Jylänki

static void font__deleteAtlas(FONTatlas* atlas)
{
	if (atlas == NULL) return;
	if (atlas->nodes != NULL) free(atlas->nodes);
	free(atlas);
}

static FONTatlas* font__allocAtlas(int w, int h, int nnodes)
{
	FONTatlas* atlas = NULL;

	// Allocate memory for the font stash.
	atlas = (FONTatlas*)malloc(sizeof(FONTatlas));
	if (atlas == NULL) goto error;
	memset(atlas, 0, sizeof(FONTatlas));

	atlas->width = w;
	atlas->height = h;

	// Allocate space for skyline nodes
	atlas->nodes = (FONTatlasNode*)malloc(sizeof(FONTatlasNode) * nnodes);
	if (atlas->nodes == NULL) goto error;
	memset(atlas->nodes, 0, sizeof(FONTatlasNode) * nnodes);
	atlas->nnodes = 0;
	atlas->cnodes = nnodes;

	// Init root node.
	atlas->nodes[0].x = 0;
	atlas->nodes[0].y = 0;
	atlas->nodes[0].width = (short)w;
	atlas->nnodes++;

	return atlas;

error:
	if (atlas) font__deleteAtlas(atlas);
	return NULL;
}

static int font__atlasInsertNode(FONTatlas* atlas, int idx, int x, int y, int w)
{
	int i;
	// Insert node
	if (atlas->nnodes+1 > atlas->cnodes) {
		atlas->cnodes = atlas->cnodes == 0 ? 8 : atlas->cnodes * 2;
		atlas->nodes = (FONTatlasNode*)realloc(atlas->nodes, sizeof(FONTatlasNode) * atlas->cnodes);
		if (atlas->nodes == NULL)
			return 0;
	}
	for (i = atlas->nnodes; i > idx; i--)
		atlas->nodes[i] = atlas->nodes[i-1];
	atlas->nodes[idx].x = (short)x;
	atlas->nodes[idx].y = (short)y;
	atlas->nodes[idx].width = (short)w;
	atlas->nnodes++;

	return 1;
}

static void font__atlasRemoveNode(FONTatlas* atlas, int idx)
{
	int i;
	if (atlas->nnodes == 0) return;
	for (i = idx; i < atlas->nnodes-1; i++)
		atlas->nodes[i] = atlas->nodes[i+1];
	atlas->nnodes--;
}

static void font__atlasExpand(FONTatlas* atlas, int w, int h)
{
	// Insert node for empty space
	if (w > atlas->width)
		font__atlasInsertNode(atlas, atlas->nnodes, atlas->width, 0, w - atlas->width);
	atlas->width = w;
	atlas->height = h;
}

static void font__atlasReset(FONTatlas* atlas, int w, int h)
{
	atlas->width = w;
	atlas->height = h;
	atlas->nnodes = 0;

	// Init root node.
	atlas->nodes[0].x = 0;
	atlas->nodes[0].y = 0;
	atlas->nodes[0].width = (short)w;
	atlas->nnodes++;
}

static int font__atlasAddSkylineLevel(FONTatlas* atlas, int idx, int x, int y, int w, int h)
{
	int i;

	// Insert new node
	if (font__atlasInsertNode(atlas, idx, x, y+h, w) == 0)
		return 0;

	// Delete skyline segments that fall under the shadow of the new segment.
	for (i = idx+1; i < atlas->nnodes; i++) {
		if (atlas->nodes[i].x < atlas->nodes[i-1].x + atlas->nodes[i-1].width) {
			int shrink = atlas->nodes[i-1].x + atlas->nodes[i-1].width - atlas->nodes[i].x;
			atlas->nodes[i].x += (short)shrink;
			atlas->nodes[i].width -= (short)shrink;
			if (atlas->nodes[i].width <= 0) {
				font__atlasRemoveNode(atlas, i);
				i--;
			} else {
				break;
			}
		} else {
			break;
		}
	}

	// Merge same height skyline segments that are next to each other.
	for (i = 0; i < atlas->nnodes-1; i++) {
		if (atlas->nodes[i].y == atlas->nodes[i+1].y) {
			atlas->nodes[i].width += atlas->nodes[i+1].width;
			font__atlasRemoveNode(atlas, i+1);
			i--;
		}
	}

	return 1;
}

static int font__atlasRectFits(FONTatlas* atlas, int i, int w, int h)
{
	// Checks if there is enough space at the location of skyline span 'i',
	// and return the max height of all skyline spans under that at that location,
	// (think tetris block being dropped at that position). Or -1 if no space found.
	int x = atlas->nodes[i].x;
	int y = atlas->nodes[i].y;
	int spaceLeft;
	if (x + w > atlas->width)
		return -1;
	spaceLeft = w;
	while (spaceLeft > 0) {
		if (i == atlas->nnodes) return -1;
		y = font__maxi(y, atlas->nodes[i].y);
		if (y + h > atlas->height) return -1;
		spaceLeft -= atlas->nodes[i].width;
		++i;
	}
	return y;
}

static int font__atlasAddRect(FONTatlas* atlas, int rw, int rh, int* rx, int* ry)
{
	int besth = atlas->height, bestw = atlas->width, besti = -1;
	int bestx = -1, besty = -1, i;

	// Bottom left fit heuristic.
	for (i = 0; i < atlas->nnodes; i++) {
		int y = font__atlasRectFits(atlas, i, rw, rh);
		if (y != -1) {
			if (y + rh < besth || (y + rh == besth && atlas->nodes[i].width < bestw)) {
				besti = i;
				bestw = atlas->nodes[i].width;
				besth = y + rh;
				bestx = atlas->nodes[i].x;
				besty = y;
			}
		}
	}

	if (besti == -1)
		return 0;

	// Perform the actual packing.
	if (font__atlasAddSkylineLevel(atlas, besti, bestx, besty, rw, rh) == 0)
		return 0;

	*rx = bestx;
	*ry = besty;

	return 1;
}

static void font__addWhiteRect(FONTcontext* stash, int w, int h)
{
	int x, y, gx, gy;
	unsigned char* dst;
	if (font__atlasAddRect(stash->atlas, w, h, &gx, &gy) == 0)
		return;

	// Rasterize
	dst = &stash->texData[gx + gy * stash->params.width];
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			dst[x] = 0xff;
		dst += stash->params.width;
	}

	stash->dirtyRect[0] = font__mini(stash->dirtyRect[0], gx);
	stash->dirtyRect[1] = font__mini(stash->dirtyRect[1], gy);
	stash->dirtyRect[2] = font__maxi(stash->dirtyRect[2], gx+w);
	stash->dirtyRect[3] = font__maxi(stash->dirtyRect[3], gy+h);
}

FONTcontext* fontCreateInternal(FONTparams* params)
{
	FONTcontext* stash = NULL;

	// Allocate memory for the font stash.
	stash = (FONTcontext*)malloc(sizeof(FONTcontext));
	if (stash == NULL) goto error;
	memset(stash, 0, sizeof(FONTcontext));

	stash->params = *params;

	// Allocate scratch buffer.
	stash->scratch = (unsigned char*)malloc(FONT_SCRATCH_BUF_SIZE);
	if (stash->scratch == NULL) goto error;

	// Initialize implementation library
	if (!font__tt_init(stash)) goto error;

	if (stash->params.renderCreate != NULL) {
		if (stash->params.renderCreate(stash->params.userPtr, stash->params.width, stash->params.height) == 0)
			goto error;
	}

	stash->atlas = font__allocAtlas(stash->params.width, stash->params.height, FONT_INIT_ATLAS_NODES);
	if (stash->atlas == NULL) goto error;

	// Allocate space for fonts.
	stash->fonts = (FONTfont**)malloc(sizeof(FONTfont*) * FONT_INIT_FONTS);
	if (stash->fonts == NULL) goto error;
	memset(stash->fonts, 0, sizeof(FONTfont*) * FONT_INIT_FONTS);
	stash->cfonts = FONT_INIT_FONTS;
	stash->nfonts = 0;

	// Create texture for the cache.
	stash->itw = 1.0f/stash->params.width;
	stash->ith = 1.0f/stash->params.height;
	stash->texData = (unsigned char*)malloc(stash->params.width * stash->params.height);
	if (stash->texData == NULL) goto error;
	memset(stash->texData, 0, stash->params.width * stash->params.height);

	stash->dirtyRect[0] = stash->params.width;
	stash->dirtyRect[1] = stash->params.height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Add white rect at 0,0 for debug drawing.
	font__addWhiteRect(stash, 2,2);

	fontPushState(stash);
	fontClearState(stash);

	return stash;

error:
	fontDeleteInternal(stash);
	return NULL;
}

static FONTstate* font__getState(FONTcontext* stash)
{
	return &stash->states[stash->nstates-1];
}

int fontAddFallbackFont(FONTcontext* stash, int base, int fallback)
{
	FONTfont* baseFont = stash->fonts[base];
	if (baseFont->nfallbacks < FONT_MAX_FALLBACKS) {
		baseFont->fallbacks[baseFont->nfallbacks++] = fallback;
		return 1;
	}
	return 0;
}

void fontSetSize(FONTcontext* stash, float size)
{
	font__getState(stash)->size = size;
}

void fontSetColor(FONTcontext* stash, unsigned int color)
{
	font__getState(stash)->color = color;
}

void fontSetSpacing(FONTcontext* stash, float spacing)
{
	font__getState(stash)->spacing = spacing;
}

void fontSetBlur(FONTcontext* stash, float blur)
{
	font__getState(stash)->blur = blur;
}

void fontSetAlign(FONTcontext* stash, int align)
{
	font__getState(stash)->align = align;
}

void fontSetFont(FONTcontext* stash, int font)
{
	font__getState(stash)->font = font;
}

void fontPushState(FONTcontext* stash)
{
	if (stash->nstates >= FONT_MAX_STATES) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONT_STATES_OVERFLOW, 0);
		return;
	}
	if (stash->nstates > 0)
		memcpy(&stash->states[stash->nstates], &stash->states[stash->nstates-1], sizeof(FONTstate));
	stash->nstates++;
}

void fontPopState(FONTcontext* stash)
{
	if (stash->nstates <= 1) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONT_STATES_UNDERFLOW, 0);
		return;
	}
	stash->nstates--;
}

void fontClearState(FONTcontext* stash)
{
	FONTstate* state = font__getState(stash);
	state->size = 12.0f;
	state->color = 0xffffffff;
	state->font = 0;
	state->blur = 0;
	state->spacing = 0;
	state->align = FONT_ALIGN_LEFT | FONT_ALIGN_BASELINE;
}

static void font__freeFont(FONTfont* font)
{
	if (font == NULL) return;
	if (font->glyphs) free(font->glyphs);
	if (font->freeData && font->data) free(font->data);
	free(font);
}

static int font__allocFont(FONTcontext* stash)
{
	FONTfont* font = NULL;
	if (stash->nfonts+1 > stash->cfonts) {
		stash->cfonts = stash->cfonts == 0 ? 8 : stash->cfonts * 2;
		stash->fonts = (FONTfont**)realloc(stash->fonts, sizeof(FONTfont*) * stash->cfonts);
		if (stash->fonts == NULL)
			return -1;
	}
	font = (FONTfont*)malloc(sizeof(FONTfont));
	if (font == NULL) goto error;
	memset(font, 0, sizeof(FONTfont));

	font->glyphs = (FONTglyph*)malloc(sizeof(FONTglyph) * FONT_INIT_GLYPHS);
	if (font->glyphs == NULL) goto error;
	font->cglyphs = FONT_INIT_GLYPHS;
	font->nglyphs = 0;

	stash->fonts[stash->nfonts++] = font;
	return stash->nfonts-1;

error:
	font__freeFont(font);

	return FONT_INVALID;
}

static FILE* font__fopen(const char* filename, const char* mode)
{
#ifdef _WIN32
	int len = 0;
	int fileLen = strlen(filename);
	int modeLen = strlen(mode);
	wchar_t wpath[MAX_PATH];
	wchar_t wmode[MAX_PATH];
	FILE* f;

	if (fileLen == 0)
		return NULL;
	if (modeLen == 0)
		return NULL;
	len = MultiByteToWideChar(CP_UTF8, 0, filename, fileLen, wpath, fileLen);
	if (len >= MAX_PATH)
		return NULL;
	wpath[len] = L'\0';
	len = MultiByteToWideChar(CP_UTF8, 0, mode, modeLen, wmode, modeLen);
	if (len >= MAX_PATH)
		return NULL;
	wmode[len] = L'\0';
	f = _wfopen(wpath, wmode);
	return f;
#else
	return fopen(filename, mode);
#endif
}

int fontAddFont(FONTcontext* stash, const char* name, const char* path)
{
	FILE* fp = 0;
	int dataSize = 0, readed;
	unsigned char* data = NULL;

	// Read in the font data.
	fp = font__fopen(path, "rb");
	if (fp == NULL) goto error;
	fseek(fp,0,SEEK_END);
	dataSize = (int)ftell(fp);
	fseek(fp,0,SEEK_SET);
	data = (unsigned char*)malloc(dataSize);
	if (data == NULL) goto error;
	readed = fread(data, 1, dataSize, fp);
	fclose(fp);
	fp = 0;
	if (readed != dataSize) goto error;

	return fontAddFontMem(stash, name, data, dataSize, 1);

error:
	if (data) free(data);
	if (fp) fclose(fp);
	return FONT_INVALID;
}

int fontAddFontMem(FONTcontext* stash, const char* name, unsigned char* data, int dataSize, int freeData)
{
	int i, ascent, descent, fh, lineGap;
	FONTfont* font;

	int idx = font__allocFont(stash);
	if (idx == FONT_INVALID)
		return FONT_INVALID;

	font = stash->fonts[idx];

	strncpy(font->name, name, sizeof(font->name));
	font->name[sizeof(font->name)-1] = '\0';

	// Init hash lookup.
	for (i = 0; i < FONT_HASH_LUT_SIZE; ++i)
		font->lut[i] = -1;

	// Read in the font data.
	font->dataSize = dataSize;
	font->data = data;
	font->freeData = (unsigned char)freeData;

	// Init font
	stash->nscratch = 0;
	if (!font__tt_loadFont(stash, &font->font, data, dataSize)) goto error;

	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	font__tt_getFontVMetrics( &font->font, &ascent, &descent, &lineGap);
	fh = ascent - descent;
	font->ascender = (float)ascent / (float)fh;
	font->descender = (float)descent / (float)fh;
	font->lineh = (float)(fh + lineGap) / (float)fh;

	return idx;

error:
	font__freeFont(font);
	stash->nfonts--;
	return FONT_INVALID;
}

int fontGetFontByName(FONTcontext* s, const char* name)
{
	int i;
	for (i = 0; i < s->nfonts; i++) {
		if (strcmp(s->fonts[i]->name, name) == 0)
			return i;
	}
	return FONT_INVALID;
}


static FONTglyph* font__allocGlyph(FONTfont* font)
{
	if (font->nglyphs+1 > font->cglyphs) {
		font->cglyphs = font->cglyphs == 0 ? 8 : font->cglyphs * 2;
		font->glyphs = (FONTglyph*)realloc(font->glyphs, sizeof(FONTglyph) * font->cglyphs);
		if (font->glyphs == NULL) return NULL;
	}
	font->nglyphs++;
	return &font->glyphs[font->nglyphs-1];
}


// Based on Exponential blur, Jani Huhtanen, 2006

#define APREC 16
#define ZPREC 7

static void font__blurCols(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	for (y = 0; y < h; y++) {
		int z = 0; // force zero border
		for (x = 1; x < w; x++) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[w-1] = 0; // force zero border
		z = 0;
		for (x = w-2; x >= 0; x--) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst += dstStride;
	}
}

static void font__blurRows(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	for (x = 0; x < w; x++) {
		int z = 0; // force zero border
		for (y = dstStride; y < h*dstStride; y += dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[(h-1)*dstStride] = 0; // force zero border
		z = 0;
		for (y = (h-2)*dstStride; y >= 0; y -= dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst++;
	}
}


static void font__blur(FONTcontext* stash, unsigned char* dst, int w, int h, int dstStride, int blur)
{
	int alpha;
	float sigma;
	(void)stash;

	if (blur < 1)
		return;
	// Calculate the alpha such that 90% of the kernel is within the radius. (Kernel extends to infinity)
	sigma = (float)blur * 0.57735f; // 1 / sqrt(3)
	alpha = (int)((1<<APREC) * (1.0f - expf(-2.3f / (sigma+1.0f))));
	font__blurRows(dst, w, h, dstStride, alpha);
	font__blurCols(dst, w, h, dstStride, alpha);
	font__blurRows(dst, w, h, dstStride, alpha);
	font__blurCols(dst, w, h, dstStride, alpha);
//	font__blurrows(dst, w, h, dstStride, alpha);
//	font__blurcols(dst, w, h, dstStride, alpha);
}

static FONTglyph* font__getGlyph(FONTcontext* stash, FONTfont* font, unsigned int codepoint,
								 short isize, short iblur)
{
	int i, g, advance, lsb, x0, y0, x1, y1, gw, gh, gx, gy, x, y;
	float scale;
	FONTglyph* glyph = NULL;
	unsigned int h;
	float size = isize/10.0f;
	int pad, added;
	unsigned char* bdst;
	unsigned char* dst;
	FONTfont* renderFont = font;

	if (isize < 2) return NULL;
	if (iblur > 20) iblur = 20;
	pad = iblur+2;

	// Reset allocator.
	stash->nscratch = 0;

	// Find code point and size.
	h = font__hashint(codepoint) & (FONT_HASH_LUT_SIZE-1);
	i = font->lut[h];
	while (i != -1) {
		if (font->glyphs[i].codepoint == codepoint && font->glyphs[i].size == isize && font->glyphs[i].blur == iblur)
			return &font->glyphs[i];
		i = font->glyphs[i].next;
	}

	// Could not find glyph, create it.
	g = font__tt_getGlyphIndex(&font->font, codepoint);
	// Try to find the glyph in fallback fonts.
	if (g == 0) {
		for (i = 0; i < font->nfallbacks; ++i) {
			FONTfont* fallbackFont = stash->fonts[font->fallbacks[i]];
			int fallbackIndex = font__tt_getGlyphIndex(&fallbackFont->font, codepoint);
			if (fallbackIndex != 0) {
				g = fallbackIndex;
				renderFont = fallbackFont;
				break;
			}
		}
		// It is possible that we did not find a fallback glyph.
		// In that case the glyph index 'g' is 0, and we'll proceed below and cache empty glyph.
	}
	scale = font__tt_getPixelHeightScale(&renderFont->font, size);
	font__tt_buildGlyphBitmap(&renderFont->font, g, size, scale, &advance, &lsb, &x0, &y0, &x1, &y1);
	gw = x1-x0 + pad*2;
	gh = y1-y0 + pad*2;

	// Find free spot for the rect in the atlas
	added = font__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	if (added == 0 && stash->handleError != NULL) {
		// Atlas is full, let the user to resize the atlas (or not), and try again.
		stash->handleError(stash->errorUptr, FONT_ATLAS_FULL, 0);
		added = font__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	}
	if (added == 0) return NULL;

	// Init glyph.
	glyph = font__allocGlyph(font);
	glyph->codepoint = codepoint;
	glyph->size = isize;
	glyph->blur = iblur;
	glyph->index = g;
	glyph->x0 = (short)gx;
	glyph->y0 = (short)gy;
	glyph->x1 = (short)(glyph->x0+gw);
	glyph->y1 = (short)(glyph->y0+gh);
	glyph->xadv = (short)(scale * advance * 10.0f);
	glyph->xoff = (short)(x0 - pad);
	glyph->yoff = (short)(y0 - pad);
	glyph->next = 0;

	// Insert char to hash lookup.
	glyph->next = font->lut[h];
	font->lut[h] = font->nglyphs-1;

	// Rasterize
	dst = &stash->texData[(glyph->x0+pad) + (glyph->y0+pad) * stash->params.width];
	font__tt_renderGlyphBitmap(&renderFont->font, dst, gw-pad*2,gh-pad*2, stash->params.width, scale,scale, g);

	// Make sure there is one pixel empty border.
	dst = &stash->texData[glyph->x0 + glyph->y0 * stash->params.width];
	for (y = 0; y < gh; y++) {
		dst[y*stash->params.width] = 0;
		dst[gw-1 + y*stash->params.width] = 0;
	}
	for (x = 0; x < gw; x++) {
		dst[x] = 0;
		dst[x + (gh-1)*stash->params.width] = 0;
	}

	// Debug code to color the glyph background
/*	unsigned char* fdst = &stash->texData[glyph->x0 + glyph->y0 * stash->params.width];
	for (y = 0; y < gh; y++) {
		for (x = 0; x < gw; x++) {
			int a = (int)fdst[x+y*stash->params.width] + 20;
			if (a > 255) a = 255;
			fdst[x+y*stash->params.width] = a;
		}
	}*/

	// Blur
	if (iblur > 0) {
		stash->nscratch = 0;
		bdst = &stash->texData[glyph->x0 + glyph->y0 * stash->params.width];
		font__blur(stash, bdst, gw,gh, stash->params.width, iblur);
	}

	stash->dirtyRect[0] = font__mini(stash->dirtyRect[0], glyph->x0);
	stash->dirtyRect[1] = font__mini(stash->dirtyRect[1], glyph->y0);
	stash->dirtyRect[2] = font__maxi(stash->dirtyRect[2], glyph->x1);
	stash->dirtyRect[3] = font__maxi(stash->dirtyRect[3], glyph->y1);

	return glyph;
}

static void font__getQuad(FONTcontext* stash, FONTfont* font,
						   int prevGlyphIndex, FONTglyph* glyph,
						   float scale, float spacing, float* x, float* y, FONTquad* q)
{
	float rx,ry,xoff,yoff,x0,y0,x1,y1;

	if (prevGlyphIndex != -1) {
		float adv = font__tt_getGlyphKernAdvance(&font->font, prevGlyphIndex, glyph->index) * scale;
		*x += (int)(adv + spacing + 0.5f);
	}

	// Each glyph has 2px border to allow good interpolation,
	// one pixel to prevent leaking, and one to allow good interpolation for rendering.
	// Inset the texture region by one pixel for correct interpolation.
	xoff = (short)(glyph->xoff+1);
	yoff = (short)(glyph->yoff+1);
	x0 = (float)(glyph->x0+1);
	y0 = (float)(glyph->y0+1);
	x1 = (float)(glyph->x1-1);
	y1 = (float)(glyph->y1-1);

	if (stash->params.flags & FONT_ZERO_TOPLEFT) {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y + yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry + y1 - y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	} else {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y - yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry - y1 + y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	}

	*x += (int)(glyph->xadv / 10.0f + 0.5f);
}

static void font__flush(FONTcontext* stash)
{
	// Flush texture
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		if (stash->params.renderUpdate != NULL)
			stash->params.renderUpdate(stash->params.userPtr, stash->dirtyRect, stash->texData);
		// Reset dirty rect
		stash->dirtyRect[0] = stash->params.width;
		stash->dirtyRect[1] = stash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
	}

	// Flush triangles
	if (stash->nverts > 0) {
		if (stash->params.renderDraw != NULL)
			stash->params.renderDraw(stash->params.userPtr, stash->verts, stash->tcoords, stash->colors, stash->nverts);
		stash->nverts = 0;
	}
}

static __inline void font__vertex(FONTcontext* stash, float x, float y, float s, float t, unsigned int c)
{
	stash->verts[stash->nverts*2+0] = x;
	stash->verts[stash->nverts*2+1] = y;
	stash->tcoords[stash->nverts*2+0] = s;
	stash->tcoords[stash->nverts*2+1] = t;
	stash->colors[stash->nverts] = c;
	stash->nverts++;
}

static float font__getVertAlign(FONTcontext* stash, FONTfont* font, int align, short isize)
{
	if (stash->params.flags & FONT_ZERO_TOPLEFT) {
		if (align & FONT_ALIGN_TOP) {
			return font->ascender * (float)isize/10.0f;
		} else if (align & FONT_ALIGN_MIDDLE) {
			return (font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONT_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONT_ALIGN_BOTTOM) {
			return font->descender * (float)isize/10.0f;
		}
	} else {
		if (align & FONT_ALIGN_TOP) {
			return -font->ascender * (float)isize/10.0f;
		} else if (align & FONT_ALIGN_MIDDLE) {
			return -(font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONT_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONT_ALIGN_BOTTOM) {
			return -font->descender * (float)isize/10.0f;
		}
	}
	return 0.0;
}

FONT_DEF float fontDrawText(FONTcontext* stash,
				   float x, float y,
				   const char* str, const char* end)
{
	FONTstate* state = font__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONTglyph* glyph = NULL;
	FONTquad q;
	int prevGlyphIndex = -1;
	short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONTfont* font;
	float width;

	if (stash == NULL) return x;
	if (state->font < 0 || state->font >= stash->nfonts) return x;
	font = stash->fonts[state->font];
	if (font->data == NULL) return x;

	scale = font__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);

	if (end == NULL)
		end = str + strlen(str);

	// Align horizontally
	if (state->align & FONT_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONT_ALIGN_RIGHT) {
		width = fontTextBounds(stash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONT_ALIGN_CENTER) {
		width = fontTextBounds(stash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += font__getVertAlign(stash, font, state->align, isize);

	for (; str != end; ++str) {
		if (font__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
		glyph = font__getGlyph(stash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			font__getQuad(stash, font, prevGlyphIndex, glyph, scale, state->spacing, &x, &y, &q);

			if (stash->nverts+6 > FONT_VERTEX_COUNT)
				font__flush(stash);

			font__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			font__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);
			font__vertex(stash, q.x1, q.y0, q.s1, q.t0, state->color);

			font__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			font__vertex(stash, q.x0, q.y1, q.s0, q.t1, state->color);
			font__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	}
	font__flush(stash);

	return x;
}

FONT_DEF int fontTextIterInit(FONTcontext* stash, FONTtextIter* iter,
					 float x, float y, const char* str, const char* end)
{
	FONTstate* state = font__getState(stash);
	float width;

	memset(iter, 0, sizeof(*iter));

	if (stash == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	iter->font = stash->fonts[state->font];
	if (iter->font->data == NULL) return 0;

	iter->isize = (short)(state->size*10.0f);
	iter->iblur = (short)state->blur;
	iter->scale = font__tt_getPixelHeightScale(&iter->font->font, (float)iter->isize/10.0f);

	// Align horizontally
	if (state->align & FONT_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONT_ALIGN_RIGHT) {
		width = fontTextBounds(stash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONT_ALIGN_CENTER) {
		width = fontTextBounds(stash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += font__getVertAlign(stash, iter->font, state->align, iter->isize);

	if (end == NULL)
		end = str + strlen(str);

	iter->x = iter->nextx = x;
	iter->y = iter->nexty = y;
	iter->spacing = state->spacing;
	iter->str = str;
	iter->next = str;
	iter->end = end;
	iter->codepoint = 0;
	iter->prevGlyphIndex = -1;

	return 1;
}

FONT_DEF int fontTextIterNext(FONTcontext* stash, FONTtextIter* iter, FONTquad* quad)
{
	FONTglyph* glyph = NULL;
	const char* str = iter->next;
	iter->str = iter->next;

	if (str == iter->end)
		return 0;

	for (; str != iter->end; str++) {
		if (font__decutf8(&iter->utf8state, &iter->codepoint, *(const unsigned char*)str))
			continue;
		str++;
		// Get glyph and quad
		iter->x = iter->nextx;
		iter->y = iter->nexty;
		glyph = font__getGlyph(stash, iter->font, iter->codepoint, iter->isize, iter->iblur);
		if (glyph != NULL)
			font__getQuad(stash, iter->font, iter->prevGlyphIndex, glyph, iter->scale, iter->spacing, &iter->nextx, &iter->nexty, quad);
		iter->prevGlyphIndex = glyph != NULL ? glyph->index : -1;
		break;
	}
	iter->next = str;

	return 1;
}

FONT_DEF void fontDrawDebug(FONTcontext* stash, float x, float y)
{
	int i;
	int w = stash->params.width;
	int h = stash->params.height;
	float u = w == 0 ? 0 : (1.0f / w);
	float v = h == 0 ? 0 : (1.0f / h);

	if (stash->nverts+6+6 > FONT_VERTEX_COUNT)
		font__flush(stash);

	// Draw background
	font__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	font__vertex(stash, x+w, y+h, u, v, 0x0fffffff);
	font__vertex(stash, x+w, y+0, u, v, 0x0fffffff);

	font__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	font__vertex(stash, x+0, y+h, u, v, 0x0fffffff);
	font__vertex(stash, x+w, y+h, u, v, 0x0fffffff);

	// Draw texture
	font__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	font__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);
	font__vertex(stash, x+w, y+0, 1, 0, 0xffffffff);

	font__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	font__vertex(stash, x+0, y+h, 0, 1, 0xffffffff);
	font__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);

	// Drawbug draw atlas
	for (i = 0; i < stash->atlas->nnodes; i++) {
		FONTatlasNode* n = &stash->atlas->nodes[i];

		if (stash->nverts+6 > FONT_VERTEX_COUNT)
			font__flush(stash);

		font__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		font__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
		font__vertex(stash, x+n->x+n->width, y+n->y+0, u, v, 0xc00000ff);

		font__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		font__vertex(stash, x+n->x+0, y+n->y+1, u, v, 0xc00000ff);
		font__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
	}

	font__flush(stash);
}

FONT_DEF float fontTextBounds(FONTcontext* stash,
					 float x, float y, 
					 const char* str, const char* end,
					 float* bounds)
{
	FONTstate* state = font__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONTquad q;
	FONTglyph* glyph = NULL;
	int prevGlyphIndex = -1;
	short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONTfont* font;
	float startx, advance;
	float minx, miny, maxx, maxy;

	if (stash == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	font = stash->fonts[state->font];
	if (font->data == NULL) return 0;

	scale = font__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);

	// Align vertically.
	y += font__getVertAlign(stash, font, state->align, isize);

	minx = maxx = x;
	miny = maxy = y;
	startx = x;

	if (end == NULL)
		end = str + strlen(str);

	for (; str != end; ++str) {
		if (font__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
		glyph = font__getGlyph(stash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			font__getQuad(stash, font, prevGlyphIndex, glyph, scale, state->spacing, &x, &y, &q);
			if (q.x0 < minx) minx = q.x0;
			if (q.x1 > maxx) maxx = q.x1;
			if (stash->params.flags & FONT_ZERO_TOPLEFT) {
				if (q.y0 < miny) miny = q.y0;
				if (q.y1 > maxy) maxy = q.y1;
			} else {
				if (q.y1 < miny) miny = q.y1;
				if (q.y0 > maxy) maxy = q.y0;
			}
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	}

	advance = x - startx;

	// Align horizontally
	if (state->align & FONT_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONT_ALIGN_RIGHT) {
		minx -= advance;
		maxx -= advance;
	} else if (state->align & FONT_ALIGN_CENTER) {
		minx -= advance * 0.5f;
		maxx -= advance * 0.5f;
	}

	if (bounds) {
		bounds[0] = minx;
		bounds[1] = miny;
		bounds[2] = maxx;
		bounds[3] = maxy;
	}

	return advance;
}

FONT_DEF void fontVertMetrics(FONTcontext* stash,
					 float* ascender, float* descender, float* lineh)
{
	FONTfont* font;
	FONTstate* state = font__getState(stash);
	short isize;

	if (stash == NULL) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (short)(state->size*10.0f);
	if (font->data == NULL) return;

	if (ascender)
		*ascender = font->ascender*isize/10.0f;
	if (descender)
		*descender = font->descender*isize/10.0f;
	if (lineh)
		*lineh = font->lineh*isize/10.0f;
}

FONT_DEF void fontLineBounds(FONTcontext* stash, float y, float* miny, float* maxy)
{
	FONTfont* font;
	FONTstate* state = font__getState(stash);
	short isize;

	if (stash == NULL) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (short)(state->size*10.0f);
	if (font->data == NULL) return;

	y += font__getVertAlign(stash, font, state->align, isize);

	if (stash->params.flags & FONT_ZERO_TOPLEFT) {
		*miny = y - font->ascender * (float)isize/10.0f;
		*maxy = *miny + font->lineh*isize/10.0f;
	} else {
		*maxy = y + font->descender * (float)isize/10.0f;
		*miny = *maxy - font->lineh*isize/10.0f;
	}
}

FONT_DEF const unsigned char* fontGetTextureData(FONTcontext* stash, int* width, int* height)
{
	if (width != NULL)
		*width = stash->params.width;
	if (height != NULL)
		*height = stash->params.height;
	return stash->texData;
}

FONT_DEF int fontValidateTexture(FONTcontext* stash, int* dirty)
{
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		dirty[0] = stash->dirtyRect[0];
		dirty[1] = stash->dirtyRect[1];
		dirty[2] = stash->dirtyRect[2];
		dirty[3] = stash->dirtyRect[3];
		// Reset dirty rect
		stash->dirtyRect[0] = stash->params.width;
		stash->dirtyRect[1] = stash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
		return 1;
	}
	return 0;
}

FONT_DEF void fontDeleteInternal(FONTcontext* stash)
{
	int i;
	if (stash == NULL) return;

	if (stash->params.renderDelete)
		stash->params.renderDelete(stash->params.userPtr);

	for (i = 0; i < stash->nfonts; ++i)
		font__freeFont(stash->fonts[i]);

	if (stash->atlas) font__deleteAtlas(stash->atlas);
	if (stash->fonts) free(stash->fonts);
	if (stash->texData) free(stash->texData);
	if (stash->scratch) free(stash->scratch);
	free(stash);
}

FONT_DEF void fontSetErrorCallback(FONTcontext* stash, void (*callback)(void* uptr, int error, int val), void* uptr)
{
	if (stash == NULL) return;
	stash->handleError = callback;
	stash->errorUptr = uptr;
}

FONT_DEF void fontGetAtlasSize(FONTcontext* stash, int* width, int* height)
{
	if (stash == NULL) return;
	*width = stash->params.width;
	*height = stash->params.height;
}

FONT_DEF int fontExpandAtlas(FONTcontext* stash, int width, int height)
{
	int i, maxy = 0;
	unsigned char* data = NULL;
	if (stash == NULL) return 0;

	width = font__maxi(width, stash->params.width);
	height = font__maxi(height, stash->params.height);

	if (width == stash->params.width && height == stash->params.height)
		return 1;

	// Flush pending glyphs.
	font__flush(stash);

	// Create new texture
	if (stash->params.renderResize != NULL) {
		if (stash->params.renderResize(stash->params.userPtr, width, height) == 0)
			return 0;
	}
	// Copy old texture data over.
	data = (unsigned char*)malloc(width * height);
	if (data == NULL)
		return 0;
	for (i = 0; i < stash->params.height; i++) {
		unsigned char* dst = &data[i*width];
		unsigned char* src = &stash->texData[i*stash->params.width];
		memcpy(dst, src, stash->params.width);
		if (width > stash->params.width)
			memset(dst+stash->params.width, 0, width - stash->params.width);
	}
	if (height > stash->params.height)
		memset(&data[stash->params.height * width], 0, (height - stash->params.height) * width);

	free(stash->texData);
	stash->texData = data;

	// Increase atlas size
	font__atlasExpand(stash->atlas, width, height);

	// Add existing data as dirty.
	for (i = 0; i < stash->atlas->nnodes; i++)
		maxy = font__maxi(maxy, stash->atlas->nodes[i].y);
	stash->dirtyRect[0] = 0;
	stash->dirtyRect[1] = 0;
	stash->dirtyRect[2] = stash->params.width;
	stash->dirtyRect[3] = maxy;

	stash->params.width = width;
	stash->params.height = height;
	stash->itw = 1.0f/stash->params.width;
	stash->ith = 1.0f/stash->params.height;

	return 1;
}

FONT_DEF int fontResetAtlas(FONTcontext* stash, int width, int height)
{
	int i, j;
	if (stash == NULL) return 0;

	// Flush pending glyphs.
	font__flush(stash);

	// Create new texture
	if (stash->params.renderResize != NULL) {
		if (stash->params.renderResize(stash->params.userPtr, width, height) == 0)
			return 0;
	}

	// Reset atlas
	font__atlasReset(stash->atlas, width, height);

	// Clear texture data.
	stash->texData = (unsigned char*)realloc(stash->texData, width * height);
	if (stash->texData == NULL) return 0;
	memset(stash->texData, 0, width * height);

	// Reset dirty rect
	stash->dirtyRect[0] = width;
	stash->dirtyRect[1] = height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Reset cached glyphs
	for (i = 0; i < stash->nfonts; i++) {
		FONTfont* font = stash->fonts[i];
		font->nglyphs = 0;
		for (j = 0; j < FONT_HASH_LUT_SIZE; j++)
			font->lut[j] = -1;
	}

	stash->params.width = width;
	stash->params.height = height;
	stash->itw = 1.0f/stash->params.width;
	stash->ith = 1.0f/stash->params.height;

	// Add white rect at 0,0 for debug drawing.
	font__addWhiteRect(stash, 2,2);

	return 1;
}

#endif // FONTSTASH_IMPLEMENTATION
