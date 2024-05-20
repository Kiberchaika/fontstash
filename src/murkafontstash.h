#ifndef MURKAFONTSTASH_H
#define MURKAFONTSTASH_H

#ifdef __cplusplus
extern "C" {
#endif
    
    FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags);
    FONS_DEF void glfonsDelete(FONScontext* ctx);
    
    FONS_DEF unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    
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

struct MURKAFONScontext {
	murka::MurkaRendererBase* renderer = nullptr;
	murka::MurImage* img = nullptr;
	murka::MurVbo* vbo = nullptr;
	int width, height;
};
typedef struct MURKAFONScontext MURKAFONScontext;

static int glfons__renderCreate(void* userPtr, int width, int height)
{
	MURKAFONScontext* context = (MURKAFONScontext*)userPtr;
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

static int glfons__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return glfons__renderCreate(userPtr, width, height);
}

static void glfons__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	MURKAFONScontext* context = (MURKAFONScontext*)userPtr;
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


static void glfons__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	MURKAFONScontext* context = (MURKAFONScontext*)userPtr;
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
		v.push_back(murka::MurkaPoint3D(p.x, p.y, 0));
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

static void glfons__renderDelete(void* userPtr)
{
	MURKAFONScontext* context = (MURKAFONScontext*)userPtr;

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

FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags)
{
	FONSparams params;
	MURKAFONScontext* context;

	context = (MURKAFONScontext*)malloc(sizeof(MURKAFONScontext));
	if (context == NULL) goto error;
	memset(context, 0, sizeof(MURKAFONScontext));

	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = glfons__renderCreate;
	params.renderResize = glfons__renderResize;
	params.renderUpdate = glfons__renderUpdate;
	params.renderDraw = glfons__renderDraw;
	params.renderDelete = glfons__renderDelete;
	params.userPtr = context;

	return fonsCreateInternal(&params);

error:
	if (context != NULL) free(context);
	return NULL;
}

FONS_DEF void glfonsDelete(FONScontext* ctx)
{
	fonsDeleteInternal(ctx);
}

FONS_DEF unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#ifdef __cplusplus
}
#endif


#endif
