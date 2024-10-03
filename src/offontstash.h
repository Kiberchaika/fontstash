#ifndef OFFONTSTASH_H
#define OFFONTSTASH_H

FONTcontext* glfontCreate(int width, int height, int flags);
void glfontDelete(FONTcontext* ctx);

unsigned int glfontRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif

#ifdef OFFONTSTASH_IMPLEMENTATION
#include "ofMain.h"

struct GLFONTcontext {
	ofImage* img;
	ofVbo* vbo;
	int width, height;
};
typedef struct GLFONTcontext GLFONTcontext;

static int glfont__renderCreate(void* userPtr, int width, int height)
{
	GLFONTcontext* context = (GLFONTcontext*)userPtr;
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

	context->img = new ofImage();
	context->img->allocate(width, height, OF_IMAGE_COLOR_ALPHA);
	
	context->vbo = new ofVbo();
	
	return 1;
}

static int glfont__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return glfont__renderCreate(userPtr, width, height);
}

static void glfont__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	GLFONTcontext* context = (GLFONTcontext*)userPtr;
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];

	if (context->img == 0) return;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			context->img->setColor(x + rect[0], y + rect[1], ofColor(255, data[(rect[1] + y) * context->width + (rect[0] + x)]));
		}
	}

	context->img->update();
}


ofFloatColor rgbaToOf(unsigned int& col)
{
	ofFloatColor c;
	c.a = (col >> 24 & 0xff) / 255.0;
	c.b = (col >> 16 & 0xff) / 255.0;
	c.g = (col >> 8 & 0xff) / 255.0;
	c.r = (col >> 0 & 0xff) / 255.0;
	return c;
}

static void glfont__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	GLFONTcontext* context = (GLFONTcontext*)userPtr;
	if (context->img == 0) return;

	/*
	// debug
	context->img->draw(0,0);
	return;
	*/

	ofFillFlag flag = ofGetFill();
	ofFill();
	context->img->bind();

	context->vbo->setVertexData((glm::vec2*)verts, nverts, GL_DYNAMIC_DRAW);
	context->vbo->setTexCoordData(tcoords, nverts, GL_DYNAMIC_DRAW);
	/*
	vector<ofFloatColor> col;
	col.resize(nverts);
	for (int i = 0; i < nverts; i++)
	{
		col[i] = rgbaToOf(colors[i]);
	}
	context->vbo->setColorData(col.data(), nverts, GL_DYNAMIC_DRAW);
	*/

	context->vbo->draw(GL_TRIANGLES, 0, nverts);

	context->img->unbind();
	flag ? ofFill() : ofNoFill();
}

static void glfont__renderDelete(void* userPtr)
{
	GLFONTcontext* context = (GLFONTcontext*)userPtr;

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

FONTcontext* glfontCreate(int width, int height, int flags)
{
	FONTparams params;
	GLFONTcontext* context;

	context = (GLFONTcontext*)malloc(sizeof(GLFONTcontext));
	if (context == NULL) goto error;
	memset(context, 0, sizeof(GLFONTcontext));

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

void glfontDelete(FONTcontext* ctx)
{
	fontDeleteInternal(ctx);
}

unsigned int glfontRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#endif
