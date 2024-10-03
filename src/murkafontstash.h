#ifndef MURKAFONTSTASH_H
#define MURKAFONTSTASH_H

#ifdef __cplusplus
extern "C" {
#endif
    
    FONT_DEF FONTcontext* glfontCreate(int width, int height, int flags);
    FONT_DEF void glfontDelete(FONTcontext* ctx);
    
    FONT_DEF unsigned int glfontRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    
#ifdef __cplusplus
}
#endif

#endif

#ifdef MURKAFONTSTASH_IMPLEMENTATION
#include "MurkaRendererBase.h"
#include "MurkaRenderer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MURKAFONTcontext {
	murka::MurkaRendererBase* renderer = nullptr;
	murka::MurImage* img = nullptr;
	murka::MurVbo* vbo = nullptr;
	int width, height;
};
typedef struct MURKAFONTcontext MURKAFONTcontext;

static int glfont__renderCreate(void* userPtr, int width, int height)
{
	MURKAFONTcontext* context = (MURKAFONTcontext*)userPtr;
	// Create may be called multiple times, delete existing texture.
	if (context->img != 0) {
		delete context->img;
		context->img = 0;
	}

	if (context->vbo != 0) {
		delete context->vbo;
		context->vbo = 0;
	}

	context->width = width;
	context->height = height;

	context->img = new murka::MurImage();
	context->vbo = new murka::MurVbo();

	return 1;
}

static int glfont__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return glfont__renderCreate(userPtr, width, height);
}

static void glfont__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	MURKAFONTcontext* context = (MURKAFONTcontext*)userPtr;
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];

	murka::MurkaRenderer* renderer = (murka::MurkaRenderer*)context->renderer;
#if defined(MURKA_JUCE)
	context->img->setOpenGLContext(renderer->getOpenGLContext());
#endif

	if (!context->img->isAllocated()) {
		bool bUseArb = renderer->getUsingArbTex();
		renderer->disableArbTex();
		context->img->allocate(context->width, context->height);
		bUseArb ? renderer->enableArbTex() : renderer->disableArbTex();
	}

#ifdef MURKA_JUCE
	context->vbo->setOpenGLContext(renderer->getOpenGLContext());
#endif
	if (!context->vbo->isInited()) context->vbo->setup();


	if (context->img == 0) return;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			context->img->setColor(x + rect[0], y + rect[1], murka::MurkaColor(255, 255, 255, float(data[(rect[1] + y) * context->width + (rect[0] + x)] )));
		}
	}

	context->img->update();
}


static void glfont__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	MURKAFONTcontext* context = (MURKAFONTcontext*)userPtr;
	if (context->img == 0) return;

	/*
	// debug
	context->renderer->enableAlphaBlending();
	context->renderer->draw(*(context->img), 0, 0);
	return;
	*/ 

	context->renderer->pushStyle();
	context->renderer->enableFill();
	context->renderer->enableAlphaBlending();

	context->renderer->bind(*(context->img));
	
	std::vector<murka::MurkaPoint3D> v;
	for (size_t i = 0; i < nverts; i++) {
		murka::MurkaPoint p = ((murka::MurkaPoint*)verts)[i];
		v.push_back(murka::MurkaPoint3D(p.x / context->renderer->getScreenScale(), p.y / context->renderer->getScreenScale(), 0));
	}

	context->vbo->setVertexData((murka::MurkaPoint3D*)v.data(), nverts);
	context->vbo->setTexCoordData((murka::MurkaPoint*)tcoords, nverts);
#ifdef MURKA_JUCE
	context->vbo->update(GL_STREAM_DRAW, ((murka::MurkaRenderer*)context->renderer)->getMainShaderAttribLocation("position"), ((murka::MurkaRenderer*)context->renderer)->getMainShaderAttribLocation("uv"), ((murka::MurkaRenderer*)context->renderer)->getMainShaderAttribLocation("col"));
#else 
	context->vbo->update(GL_STREAM_DRAW);
#endif
	context->renderer->drawVbo(*(context->vbo), GL_TRIANGLES, 0, nverts);

	context->renderer->unbind(*(context->img));
	context->renderer->popStyle();
}

static void glfont__renderDelete(void* userPtr)
{
	MURKAFONTcontext* context = (MURKAFONTcontext*)userPtr;

	if (context->img != 0) {
		delete context->img;
		context->img = 0;
	}

	if (context->vbo != 0) {
		delete context->vbo;
		context->vbo = 0;
	}

    free(context);
}

FONT_DEF FONTcontext* glfontCreate(int width, int height, int flags)
{
	FONTparams params;
	MURKAFONTcontext* context;

	context = (MURKAFONTcontext*)malloc(sizeof(MURKAFONTcontext));
	if (context == NULL) goto error;
	memset(context, 0, sizeof(MURKAFONTcontext));

	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = glfont__renderCreate;
	params.renderResize = glfont__renderResize;
	params.renderUpdate = glfont__renderUpdate;
	params.renderDraw = glfont__renderDraw;
	params.renderDelete = glfont__renderDelete;
	params.userPtr = context;

	return fontCreateInternal(&params);

error:
	if (context != NULL) free(context);
	return NULL;
}

FONT_DEF void glfontDelete(FONTcontext* ctx)
{
	fontDeleteInternal(ctx);
}

FONT_DEF unsigned int glfontRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#ifdef __cplusplus
}
#endif


#endif
