#include "VrApi.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <stdlib.h>

#define error(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifndef NDEBUG
#define info(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#endif // NDEBUG

static const char *TAG = "HELLO WORLD";

static const char *egl_get_error_string(EGLint error) {
	switch (error) {
	case EGL_SUCCESS:
		return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:
		return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:
		return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:
		return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:
		return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:
		return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:
		return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:
		return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:
		return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:
		return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:
		return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:
		return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:
		return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:
		return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:
		return "EGL_CONTEXT_LOST";
	default:
		abort();
	}
}

static const char *gl_get_framebuffer_status_string(GLenum status) {
	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED:
		return "GL_FRAMEBUFFER_UNDEFINED";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_UNSUPPORTED:
		return "GL_FRAMEBUFFER_UNSUPPORTED";
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
	default:
		abort();
	}
}

struct egl {
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
};

static void egl_create(struct egl *egl) {
	info("get EGL display");
	egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egl->display == EGL_NO_DISPLAY) {
		error("can't get EGL display: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("initialize EGL display");
	if (eglInitialize(egl->display, NULL, NULL) == EGL_FALSE) {
		error("can't initialize EGL display: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("get number of EGL configs");
	EGLint num_configs = 0;
	if (eglGetConfigs(egl->display, NULL, 0, &num_configs) == EGL_FALSE) {
		error("can't get number of EGL configs: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("allocate EGL configs");
	EGLConfig *configs = malloc(num_configs * sizeof(EGLConfig));
	if (configs == NULL) {
		error("cant allocate EGL configs: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("get EGL configs");
	if (eglGetConfigs(egl->display, configs, num_configs, &num_configs) == EGL_FALSE) {
		error("can't get EGL configs: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("choose EGL config");
	static const EGLint CONFIG_ATTRIBS[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_SAMPLES, 0,
		EGL_NONE,
	};
	EGLConfig found_config = NULL;
	for (int i = 0; i < num_configs; ++i) {
		EGLConfig config = configs[i];

		info("get EGL config renderable type");
		EGLint renderable_type = 0;
		if (eglGetConfigAttrib(egl->display, config, EGL_RENDERABLE_TYPE, &renderable_type) == EGL_FALSE) {
			error("can't get EGL config renderable type: %s", egl_get_error_string(eglGetError()));
			exit(EXIT_FAILURE);
		}
		if ((renderable_type & EGL_OPENGL_ES3_BIT_KHR) == 0) {
			continue;
		}

		info("get EGL config surface type");
		EGLint surface_type = 0;
		if (eglGetConfigAttrib(egl->display, config, EGL_SURFACE_TYPE, &surface_type) == EGL_FALSE) {
			error("can't get EGL config surface type: %s", egl_get_error_string(eglGetError()));
			exit(EXIT_FAILURE);
		}
		if ((renderable_type & EGL_PBUFFER_BIT) == 0) {
			continue;
		}
		if ((renderable_type & EGL_WINDOW_BIT) == 0) {
			continue;
		}

		const EGLint *attrib = CONFIG_ATTRIBS;
		while (attrib[0] != EGL_NONE) {
			info("get EGL config attrib");
			EGLint value = 0;
			if (eglGetConfigAttrib(egl->display, config, attrib[0], &value) == EGL_FALSE) {
				error("can't get EGL config attrib: %s", egl_get_error_string(eglGetError()));
				exit(EXIT_FAILURE);
			}
			if (value != attrib[1]) {
				break;
			}
			attrib += 2;
		}
		if (attrib[0] != EGL_NONE) {
			continue;
		}

		found_config = config;
		break;
	}
	if (found_config == NULL) {
		error("can't choose EGL config");
		exit(EXIT_FAILURE);
	}

	info("free EGL configs");
	free(configs);

	info("create EGL context");
	static const EGLint CONTEXT_ATTRIBS[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	egl->context = eglCreateContext(egl->display, found_config, EGL_NO_CONTEXT, CONTEXT_ATTRIBS);
	if (egl->context == EGL_NO_CONTEXT) {
		error("can't create EGL context: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("create EGL surface");
	static const EGLint SURFACE_ATTRIBS[] = {
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE,
	};
	egl->surface = eglCreatePbufferSurface(egl->display, found_config, SURFACE_ATTRIBS);
	if (egl->surface == EGL_NO_SURFACE) {
		error("can't create EGL pixel buffer surface: %s", egl_get_error_string(eglGetError()));
		exit(EXIT_FAILURE);
	}

	info("make EGL context current");
	if (eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context) == EGL_FALSE) {
		error("can't make EGL context current: %s", egl_get_error_string(eglGetError()));
	}
}

static void egl_destroy(struct egl *egl) {
	info("make EGL context no longer current");
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	info("destroy EGL surface");
	eglDestroySurface(egl->display, egl->surface);

	info("destroy EGL context");
	eglDestroyContext(egl->display, egl->context);

	info("terminate EGL display");
	eglTerminate(egl->display);
}

struct framebuffer {
	int swap_chain_index;
	int swap_chain_length;
	GLsizei width;
	GLsizei height;
	ovrTextureSwapChain *color_texture_swap_chain;
	GLuint *depth_renderbuffers;
	GLuint *framebuffers;
};

static void framebuffer_create(struct framebuffer *framebuffer, GLsizei width, GLsizei height) {
	framebuffer->swap_chain_index = 0;
	framebuffer->width = width;
	framebuffer->height = height;

	info("create color texture swap chain");
	framebuffer->color_texture_swap_chain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, GL_RGBA8, width, height, 1, 3);
	if (framebuffer->color_texture_swap_chain == NULL) {
		error("can't create color texture swap chain");
		exit(EXIT_FAILURE);
	}

	framebuffer->swap_chain_length = vrapi_GetTextureSwapChainLength(framebuffer->color_texture_swap_chain);

	info("allocate depth renderbuffers");
	framebuffer->depth_renderbuffers = malloc(framebuffer->swap_chain_length * sizeof(GLuint));
	if (framebuffer->depth_renderbuffers == NULL) {
		error("can't allocate depth renderbuffers");
		exit(EXIT_FAILURE);
	}

	info("allocate framebuffers");
	framebuffer->framebuffers = malloc(framebuffer->swap_chain_length * sizeof(GLuint));
	if (framebuffer->framebuffers == NULL) {
		error("can't allocate framebuffers");
		exit(EXIT_FAILURE);
	}

	glGenRenderbuffers(framebuffer->swap_chain_length, framebuffer->depth_renderbuffers);
	glGenFramebuffers(framebuffer->swap_chain_length, framebuffer->framebuffers);
	for (int i = 0; i < framebuffer->swap_chain_length; ++i) {
		info("create color texture %d", i);
		GLuint color_texture = vrapi_GetTextureSwapChainHandle(framebuffer->color_texture_swap_chain, i);
		glBindTexture(GL_TEXTURE_2D, color_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0 );

		info("create depth renderbuffer %d", i);
		glBindRenderbuffer(GL_RENDERBUFFER, framebuffer->depth_renderbuffers[i]);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		info("create framebuffer %d", i);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->framebuffers[i]);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer->depth_renderbuffers[i]);
		GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			error("can't create framebuffer %d: %s", i, gl_get_framebuffer_status_string(status));
			exit(EXIT_FAILURE);
		}
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
}

static void framebuffer_destroy(struct framebuffer *framebuffer) {
	info("destroy framebuffers");
	glDeleteFramebuffers(framebuffer->swap_chain_length, framebuffer->framebuffers);

	info("destroy depth renderbuffers");
	glDeleteRenderbuffers(framebuffer->swap_chain_length, framebuffer->depth_renderbuffers);

	info("free framebuffers");
	free(framebuffer->framebuffers);

	info("free depth renderbuffers");
	free(framebuffer->depth_renderbuffers);

	info("destroy color texture swap chain");
	vrapi_DestroyTextureSwapChain(framebuffer->color_texture_swap_chain);
}

enum attrib {
	ATTRIB_BEGIN,
	ATTRIB_POSITION = ATTRIB_BEGIN,
	ATTRIB_COLOR,
	ATTRIB_END,
};

enum uniform {
	UNIFORM_BEGIN,
	UNIFORM_MODEL_MATRIX = UNIFORM_BEGIN,
	UNIFORM_VIEW_MATRIX,
	UNIFORM_PROJECTION_MATRIX,
	UNIFORM_END,
};

struct program {
	GLuint program;
	GLint uniform_locations[UNIFORM_END];
};

static const char *ATTRIB_NAMES[ATTRIB_END] = {
	"vertexPosition",
	"vertexColor",
};

static const char *UNIFORM_NAMES[UNIFORM_END] = {
	"ModelMatrix",
	"ViewMatrix",
	"ProjectionMatrix",
};

static const char VERTEX_SHADER[] =
	"#version 300 es\n"
	"\n"
	"in vec3 vertexPosition;\n"
	"in vec4 vertexColor;\n"
	"uniform mat4 ModelMatrix;\n"
	"uniform mat4 ViewMatrix;\n"
    "uniform mat4 ProjectionMatrix;\n"
	"\n"
	"out vec4 fragmentColor;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = ProjectionMatrix * ( ViewMatrix * ( ModelMatrix * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
	"	fragmentColor = vertexColor;\n"
	"}\n";

static const char FRAGMENT_SHADER[] =
	"#version 300 es\n"
	"\n"
	"in lowp vec4 fragmentColor;\n"
	"out lowp vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	outColor = fragmentColor;\n"
	"}\n";

static GLuint compile_shader(GLenum type, const char *string) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &string, NULL);
	glCompileShader(shader);
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		char *log = malloc(length);
		glGetShaderInfoLog(shader, length, NULL, log);
		error("can't compile shader: %s", log);
		exit(EXIT_FAILURE);
	}
	return shader;
}

static void program_create(struct program *program) {
	program->program = glCreateProgram();
	GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
	glAttachShader(program->program, vertex_shader);
	GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
	glAttachShader(program->program, fragment_shader);
	for (enum attrib attrib = ATTRIB_BEGIN; attrib != ATTRIB_END; ++attrib) {
		glBindAttribLocation(program->program, attrib, ATTRIB_NAMES[attrib]);
	}
	glLinkProgram(program->program);
	GLint status = 0;
	glGetProgramiv(program->program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint length = 0;
		glGetProgramiv(program->program, GL_INFO_LOG_LENGTH, &length);
		char *log = malloc(length);
		glGetProgramInfoLog(program->program, length, NULL, log);
		error("can't link program: %s", log);
		exit(EXIT_FAILURE);
	}
	for (enum uniform uniform = UNIFORM_BEGIN; uniform != UNIFORM_END; ++uniform) {
		program->uniform_locations[uniform] = glGetUniformLocation(program->program, UNIFORM_NAMES[uniform]);
	}
}

static void program_destroy(struct program* program) {
	glDeleteProgram(program->program);
}

struct attrib_pointer {
	GLint size;
	GLenum type;
	GLboolean normalized;
	GLsizei stride;
	const GLvoid *pointer;
};

struct vertex {
	char position[4];
	unsigned char color[4];
};

struct geometry {
	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint index_buffer;
};

static const struct attrib_pointer ATTRIB_POINTERS[ATTRIB_END] = {
	{ 4, GL_BYTE, GL_TRUE, sizeof(struct vertex), (const GLvoid *) offsetof(struct vertex, position) },
	{ 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct vertex), (const GLvoid *) offsetof(struct vertex, color) },
};

static const struct vertex VERTICES[] = {
	{ { -127, +127, -127, +127 }, { 255, 0, 255, 255 } },
	{ { +127, +127, -127, +127 }, { 0, 255, 0, 255 } },
	{ { +127, +127, +127, +127 }, { 0, 0, 255, 255 } },
	{ { -127, +127, +127, +127 }, { 255, 0, 0, 255 } },
	{ { -127, -127, -127, +127 }, { 0, 0, 255, 255 } },
	{ { -127, -127, +127, +127 }, { 0, 255, 0, 255 } },
	{ { +127, -127, +127, +127 }, { 255, 0, 255, 255 } },
	{ { +127, -127, -127, +127 }, { 255, 0, 0, 255 } },
};

static const unsigned short INDICES[] = {
	0, 2, 1, 2, 0, 3,	// top
	4, 6, 5, 6, 4, 7,	// bottom
	2, 6, 7, 7, 1, 2,	// right
	0, 4, 5, 5, 3, 0,	// left
	3, 5, 6, 6, 2, 3,	// front
	0, 1, 7, 7, 4, 0	// back
};
static const GLsizei NUM_INDICES = sizeof(INDICES) / sizeof(unsigned short);

static void geometry_create(struct geometry *geometry) {
	glGenVertexArrays(1, &geometry->vertex_array);
	glBindVertexArray(geometry->vertex_array);
	glGenBuffers(1, &geometry->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW);
	for (enum attrib attrib = ATTRIB_BEGIN; attrib != ATTRIB_END; ++attrib) {
		struct attrib_pointer attrib_pointer = ATTRIB_POINTERS[attrib];
		glEnableVertexAttribArray(attrib);
		glVertexAttribPointer(attrib, attrib_pointer.size, attrib_pointer.type, attrib_pointer.normalized, attrib_pointer.stride, attrib_pointer.pointer);
	}
	glGenBuffers(1, &geometry->index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES), INDICES, GL_STATIC_DRAW);
	glBindVertexArray(0);
}

static void geometry_destroy(struct geometry *geometry) {
	glDeleteBuffers(1, &geometry->index_buffer);
	glDeleteBuffers(1, &geometry->vertex_buffer);
	glDeleteVertexArrays(1, &geometry->vertex_array);
}

/************************************************************************************

Filename	:	VrCubeWorld_NativeActivity.c
Content		:	This sample uses the Android NativeActivity class. This sample does
				not use the application framework.
				This sample only uses the VrApi.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/window.h>				// for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>	// for native window JNI
#include "android_native_app_glue.h"

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

static const int CPU_LEVEL			= 2;
static const int GPU_LEVEL			= 3;
static const int NUM_MULTI_SAMPLES	= 4;

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return ( now.tv_sec * 1e9 + now.tv_nsec ) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

#ifdef CHECK_GL_ERRORS

static const char * GlErrorString( GLenum error )
{
	switch ( error )
	{
		case GL_NO_ERROR:						return "GL_NO_ERROR";
		case GL_INVALID_ENUM:					return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:					return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:				return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION:	return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:					return "GL_OUT_OF_MEMORY";
		default: return "unknown";
	}
}

static void GLCheckErrors( int line )
{
	for ( int i = 0; i < 10; i++ )
	{
		const GLenum error = glGetError();
		if ( error == GL_NO_ERROR )
		{
			break;
		}
		error( "GL error on line %d: %s", line, GlErrorString( error ) );
	}
}

#define GL( func )		func; GLCheckErrors( __LINE__ );

#else // CHECK_GL_ERRORS

#define GL( func )		func;

#endif // CHECK_GL_ERRORS

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
	struct framebuffer framebuffers[VRAPI_FRAME_LAYER_EYE_MAX];
	int				NumBuffers;
} ovrRenderer;

static void ovrRenderer_Create( ovrRenderer * renderer, const ovrJava * java )
{
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;

	// Create the frame buffers.
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		framebuffer_create(
			&renderer->framebuffers[eye],
			vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ),
			vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT ) );

	}
}

static void ovrRenderer_Destroy( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		framebuffer_destroy( &renderer->framebuffers[eye] );
	}
}

static ovrLayerProjection2 ovrRenderer_RenderFrame( ovrRenderer * renderer, const ovrJava * java,
											const struct program * program, const struct geometry * geometry,
											const ovrTracking2 * tracking, ovrMobile * ovr )
{
	ovrTracking2 updatedTracking = *tracking;

	ovrMatrix4f eyeViewMatrixTransposed[2];
	eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose( &updatedTracking.Eye[0].ViewMatrix );
	eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose( &updatedTracking.Eye[1].ViewMatrix );

	ovrMatrix4f projectionMatrixTransposed[2];
	projectionMatrixTransposed[0] = ovrMatrix4f_Transpose( &updatedTracking.Eye[0].ProjectionMatrix );
	projectionMatrixTransposed[1] = ovrMatrix4f_Transpose( &updatedTracking.Eye[1].ProjectionMatrix );

	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.HeadPose = updatedTracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		struct framebuffer *framebuffer = &renderer->framebuffers[renderer->NumBuffers == 1 ? 0 : eye];
		layer.Textures[eye].ColorSwapChain = framebuffer->color_texture_swap_chain;
		layer.Textures[eye].SwapChainIndex = framebuffer->swap_chain_index;
		layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection( &updatedTracking.Eye[eye].ProjectionMatrix );
	}
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	// Render the eye images.
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		// NOTE: In the non-mv case, latency can be further reduced by updating the sensor prediction
		// for each eye (updates orientation, not position)
		struct framebuffer *framebuffer = &renderer->framebuffers[eye];
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->framebuffers[framebuffer->swap_chain_index]);

		GL( glUseProgram( program->program ) );

		ovrMatrix4f modelMatrix = ovrMatrix4f_CreateTranslation(0.0, 0.0, -1.0);
		modelMatrix = ovrMatrix4f_Transpose( &modelMatrix );
		GL( glUniformMatrix4fv( program->uniform_locations[UNIFORM_MODEL_MATRIX], 1, GL_FALSE, (const GLfloat *) &modelMatrix ) );

		ovrMatrix4f viewMatrix = ovrMatrix4f_Transpose(&updatedTracking.Eye[eye].ViewMatrix);
		GL( glUniformMatrix4fv( program->uniform_locations[UNIFORM_VIEW_MATRIX], 1, GL_FALSE, (const GLfloat *) &viewMatrix ) );

		ovrMatrix4f projectionMatrix = ovrMatrix4f_Transpose(&updatedTracking.Eye[eye].ProjectionMatrix);
		GL( glUniformMatrix4fv( program->uniform_locations[UNIFORM_PROJECTION_MATRIX], 1, GL_FALSE, (const GLfloat *) &projectionMatrix ) );

		GL( glEnable( GL_SCISSOR_TEST ) );
		GL( glDepthMask( GL_TRUE ) );
		GL( glEnable( GL_DEPTH_TEST ) );
		GL( glDepthFunc( GL_LEQUAL ) );
		GL( glEnable( GL_CULL_FACE ) );
		GL( glCullFace( GL_BACK ) );
		GL( glViewport( 0, 0, framebuffer->width, framebuffer->height ) );
		GL( glScissor( 0, 0, framebuffer->width, framebuffer->height ) );
		GL( glClearColor( 0.125f, 0.0f, 0.125f, 1.0f ) );
		GL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );
		GL( glBindVertexArray( geometry->vertex_array ) );
		GL( glDrawElements( GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, NULL ) );
		GL( glBindVertexArray( 0 ) );
		GL( glUseProgram( 0 ) );

		// Clear to fully opaque black.
		GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );
		// bottom
		GL( glScissor( 0, 0, framebuffer->width, 1 ) );
		GL( glClear( GL_COLOR_BUFFER_BIT ) );
		// top
		GL( glScissor( 0, framebuffer->height - 1, framebuffer->width, 1 ) );
		GL( glClear( GL_COLOR_BUFFER_BIT ) );
		// left
		GL( glScissor( 0, 0, 1, framebuffer->height ) );
		GL( glClear( GL_COLOR_BUFFER_BIT ) );
		// right
		GL( glScissor( framebuffer->width - 1, 0, 1, framebuffer->height ) );
		GL( glClear( GL_COLOR_BUFFER_BIT ) );

		// Discard the depth buffer, so the tiler won't need to write it back out to memory.
		const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
		glInvalidateFramebuffer( GL_DRAW_FRAMEBUFFER, 1, depthAttachment );

		// Flush this frame worth of commands.
		glFlush();

		framebuffer->swap_chain_index = ( framebuffer->swap_chain_index + 1 ) % framebuffer->swap_chain_length;

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	return layer;
}

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct
{
	ovrJava				Java;
	struct egl			Egl;
	ANativeWindow *		NativeWindow;
	bool				Resumed;
	ovrMobile *			Ovr;
	bool				SceneCreated;
	struct program			Program;
	struct geometry			Geometry;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	bool				BackButtonDownLastFrame;
	ovrRenderer			Renderer;
} ovrApp;

static void ovrApp_Clear( ovrApp * app )
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->NativeWindow = NULL;
	app->Resumed = false;
	app->Ovr = NULL;
	app->SceneCreated = false;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 2;
	app->GpuLevel = 2;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;
	app->BackButtonDownLastFrame = false;

	app->Egl.display = 0;
	app->Egl.context = EGL_NO_CONTEXT;
	app->Egl.surface = EGL_NO_SURFACE;
}

static void ovrApp_PushBlackFinal( ovrApp * app )
{
#if MULTI_THREADED
	ovrRenderThread_Submit( &app->RenderThread, app->Ovr,
			RENDER_BLACK_FINAL, app->FrameIndex, app->DisplayTime, app->SwapInterval,
			NULL, NULL, NULL );
#else
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;

	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2 * layers[] =
	{
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = { 0 };
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = app->FrameIndex;
	frameDesc.DisplayTime = app->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2( app->Ovr, &frameDesc );
#endif
}

static void ovrApp_HandleVrModeChanges( ovrApp * app )
{
	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParms parms = vrapi_DefaultModeParms( &app->Java );
			// No need to reset the FLAG_FULLSCREEN window flag when using a View
			parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

			parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			parms.Display = (size_t)app->Egl.display;
			parms.WindowSurface = (size_t)app->NativeWindow;
			parms.ShareContext = (size_t)app->Egl.context;

			info( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			info( "        vrapi_EnterVrMode()" );

			app->Ovr = vrapi_EnterVrMode( &parms );

			info( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			// If entering VR mode failed then the ANativeWindow was not valid.
			if ( app->Ovr == NULL )
			{
				error( "Invalid ANativeWindow!" );
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if ( app->Ovr != NULL )
			{
				vrapi_SetClockLevels( app->Ovr, app->CpuLevel, app->GpuLevel );

				info( "		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid );

				info( "		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid );

				info( "		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid );
			}
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
			info( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			info( "        vrapi_LeaveVrMode()" );

			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;

			info( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
		}
	}
}

static void ovrApp_HandleInput( ovrApp * app )
{
	bool backButtonDownThisFrame = false;

	for ( int i = 0; ; i++ )
	{
		ovrInputCapabilityHeader cap;
		ovrResult result = vrapi_EnumerateInputDevices( app->Ovr, i, &cap );
		if ( result < 0 )
		{
			break;
		}

		if ( cap.Type == ovrControllerType_Headset )
		{
			ovrInputStateHeadset headsetInputState;
			headsetInputState.Header.ControllerType = ovrControllerType_Headset;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &headsetInputState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= headsetInputState.Buttons & ovrButton_Back;
			}
		}
		else if ( cap.Type == ovrControllerType_TrackedRemote )
		{
			ovrInputStateTrackedRemote trackedRemoteState;
			trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &trackedRemoteState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_B;
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Y;
			}
		}
		else if ( cap.Type == ovrControllerType_Gamepad )
		{
			ovrInputStateGamepad gamepadState;
			gamepadState.Header.ControllerType = ovrControllerType_Gamepad;
			result = vrapi_GetCurrentInputState( app->Ovr, cap.DeviceID, &gamepadState.Header );
			if ( result == ovrSuccess )
			{
				backButtonDownThisFrame |= ( ( gamepadState.Buttons & ovrButton_Back ) != 0 ) || ( ( gamepadState.Buttons & ovrButton_B ) != 0 );
			}
		}
	}

	bool backButtonDownLastFrame = app->BackButtonDownLastFrame;
	app->BackButtonDownLastFrame = backButtonDownThisFrame;

	if ( backButtonDownLastFrame && !backButtonDownThisFrame )
	{
		info( "back button short press" );
		info( "        ovrApp_PushBlackFinal()" );
		ovrApp_PushBlackFinal( app );
		info( "        vrapi_ShowSystemUI( confirmQuit )" );
		vrapi_ShowSystemUI( &app->Java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
	}
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd( struct android_app * app, int32_t cmd )
{
	ovrApp * appState = (ovrApp *)app->userData;

	switch ( cmd )
	{
		// There is no APP_CMD_CREATE. The ANativeActivity creates the
		// application thread from onCreate(). The application thread
		// then calls android_main().
		case APP_CMD_START:
		{
			info( "onStart()" );
			info( "    APP_CMD_START" );
			break;
		}
		case APP_CMD_RESUME:
		{
			info( "onResume()" );
			info( "    APP_CMD_RESUME" );
			appState->Resumed = true;
			break;
		}
		case APP_CMD_PAUSE:
		{
			info( "onPause()" );
			info( "    APP_CMD_PAUSE" );
			appState->Resumed = false;
			break;
		}
		case APP_CMD_STOP:
		{
			info( "onStop()" );
			info( "    APP_CMD_STOP" );
			break;
		}
		case APP_CMD_DESTROY:
		{
			info( "onDestroy()" );
			info( "    APP_CMD_DESTROY" );
			appState->NativeWindow = NULL;
			break;
		}
		case APP_CMD_INIT_WINDOW:
		{
			info( "surfaceCreated()" );
			info( "    APP_CMD_INIT_WINDOW" );
			appState->NativeWindow = app->window;
			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			info( "surfaceDestroyed()" );
			info( "    APP_CMD_TERM_WINDOW" );
			appState->NativeWindow = NULL;
			break;
		}
	}
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main( struct android_app * app )
{
	info( "----------------------------------------------------------------" );
	info( "android_app_entry()" );
	info( "    android_main()" );

	ANativeActivity_setWindowFlags( app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 );

	ovrJava java;
	java.Vm = app->activity->vm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = app->activity->clazz;

	// Note that AttachCurrentThread will reset the thread name.
	prctl( PR_SET_NAME, (long)"OVR::Main", 0, 0, 0 );

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}

	ovrApp appState;
	ovrApp_Clear( &appState );
	appState.Java = java;

	egl_create( &appState.Egl);

	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();

	ovrRenderer_Create( &appState.Renderer, &java );

	app->userData = &appState;
	app->onAppCmd = app_handle_cmd;

	const double startTime = GetTimeInSeconds();

	while ( app->destroyRequested == 0 )
	{
		// Read all pending events.
		for ( ; ; )
		{
			int events;
			struct android_poll_source * source;
			const int timeoutMilliseconds = ( appState.Ovr == NULL && app->destroyRequested == 0 ) ? -1 : 0;
			if ( ALooper_pollAll( timeoutMilliseconds, NULL, &events, (void **)&source ) < 0 )
			{
				break;
			}

			// Process this event.
			if ( source != NULL )
			{
				source->process( app, source );
			}

			ovrApp_HandleVrModeChanges( &appState );
		}

		ovrApp_HandleInput( &appState );

		if ( appState.Ovr == NULL )
		{
			continue;
		}

		// Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if ( !appState.SceneCreated )
		{
			// Show a loading icon.
			int frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			const ovrLayerHeader2 * layers[] =
			{
				&blackLayer.Header,
				&iconLayer.Header,
			};

			ovrSubmitFrameDescription2 frameDesc = { 0 };
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;

			vrapi_SubmitFrame2( appState.Ovr, &frameDesc );

			// Create the scene.
			appState.SceneCreated = true;
			program_create ( &appState.Program );
			geometry_create ( &appState.Geometry );
		}

		// This is the only place the frame index is incremented, right before
		// calling vrapi_GetPredictedDisplayTime().
		appState.FrameIndex++;

		// Get the HMD pose, predicted for the middle of the time period during which
		// the new eye images will be displayed. The number of frames predicted ahead
		// depends on the pipeline depth of the engine and the synthesis rate.
		// The better the prediction, the less black will be pulled in at the edges.
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime( appState.Ovr, appState.FrameIndex );
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2( appState.Ovr, predictedDisplayTime );

		appState.DisplayTime = predictedDisplayTime;

		// Render eye images and setup the primary layer using ovrTracking2.
		const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame( &appState.Renderer, &appState.Java,
				&appState.Program, &appState.Geometry, &tracking, appState.Ovr );

		const ovrLayerHeader2 * layers[] =
		{
			&worldLayer.Header
		};

		ovrSubmitFrameDescription2 frameDesc = { 0 };
		frameDesc.Flags = 0;
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 1;
		frameDesc.Layers = layers;

		// Hand over the eye images to the time warp.
		vrapi_SubmitFrame2( appState.Ovr, &frameDesc );
	}

	ovrRenderer_Destroy( &appState.Renderer );

	geometry_destroy( &appState.Geometry );
	program_destroy ( &appState.Program );
	egl_destroy( &appState.Egl );

	vrapi_Shutdown();

	(*java.Vm)->DetachCurrentThread( java.Vm );
}
