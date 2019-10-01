#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"
#include "VrApi_SystemUtils.h"
#include "android_native_app_glue.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/window.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

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
static const GLsizei NUM_INDICES = sizeof(INDICES) / sizeof(INDICES[0]);

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

struct renderer {
	struct framebuffer framebuffers[VRAPI_FRAME_LAYER_EYE_MAX];
	struct program program;
	struct geometry geometry;
};

static void renderer_create(struct renderer *renderer, GLsizei width, GLsizei height) {
	for (int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; ++i) {
		framebuffer_create(&renderer->framebuffers[i], width, height);
	}
	program_create(&renderer->program);
	geometry_create(&renderer->geometry);
}

static void renderer_destroy(struct renderer *renderer) {
	geometry_destroy(&renderer->geometry);
	program_destroy(&renderer->program);
	for (int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; ++i) {
		framebuffer_destroy(&renderer->framebuffers[i]);
	}
}

static ovrLayerProjection2 renderer_render_frame(struct renderer *renderer, ovrTracking2 *tracking) {
	ovrMatrix4f model_matrix = ovrMatrix4f_CreateTranslation(0.0, 0.0, -1.0);
	model_matrix = ovrMatrix4f_Transpose( &model_matrix );

	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
	layer.HeadPose = tracking->HeadPose;

	for (int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; ++i) {
		ovrMatrix4f view_matrix = ovrMatrix4f_Transpose(&tracking->Eye[i].ViewMatrix);
		ovrMatrix4f projection_matrix = ovrMatrix4f_Transpose(&tracking->Eye[i].ProjectionMatrix);

		struct framebuffer *framebuffer = &renderer->framebuffers[i];
		layer.Textures[i].ColorSwapChain = framebuffer->color_texture_swap_chain;
		layer.Textures[i].SwapChainIndex = framebuffer->swap_chain_index;
		layer.Textures[i].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking->Eye[i].ProjectionMatrix);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->framebuffers[framebuffer->swap_chain_index]);

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glViewport(0, 0, framebuffer->width, framebuffer->height);
		glScissor(0, 0, framebuffer->width, framebuffer->height);
		glClearColor(0.0, 0.0, 0.0, 0.0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(renderer->program.program);
		glUniformMatrix4fv(renderer->program.uniform_locations[UNIFORM_MODEL_MATRIX], 1, GL_FALSE, (const GLfloat *) &model_matrix );
		glUniformMatrix4fv(renderer->program.uniform_locations[UNIFORM_VIEW_MATRIX], 1, GL_FALSE, (const GLfloat *) &view_matrix );
		glUniformMatrix4fv(renderer->program.uniform_locations[UNIFORM_PROJECTION_MATRIX], 1, GL_FALSE, (const GLfloat *) &projection_matrix );
		glBindVertexArray(renderer->geometry.vertex_array);
		glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, NULL);
		glBindVertexArray(0);
		glUseProgram(0);

		glClearColor(0.0, 0.0, 0.0, 1.0);
		glScissor(0, 0, 1, framebuffer->height );
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(framebuffer->width - 1, 0, 1, framebuffer->height);
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(0, 0, framebuffer->width, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(0, framebuffer->height - 1, framebuffer->width, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		static const GLenum ATTACHMENTS[] = { GL_DEPTH_ATTACHMENT };
		static const GLsizei NUM_ATTACHMENTS = sizeof(ATTACHMENTS) / sizeof(ATTACHMENTS[0]);
		glInvalidateFramebuffer( GL_DRAW_FRAMEBUFFER, NUM_ATTACHMENTS, ATTACHMENTS);
		glFlush();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		framebuffer->swap_chain_index = ( framebuffer->swap_chain_index + 1 ) % framebuffer->swap_chain_length;
	}
	return layer;
}

struct app {
	ovrJava *java;
	struct egl egl;
	struct renderer renderer;
	bool resumed;
	ANativeWindow *window;
	ovrMobile *ovr;
	bool back_button_down_previous_frame;
	uint64_t frame_index;
};

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;

static void app_on_cmd(struct android_app *android_app, int32_t cmd) {
	struct app *app = (struct app *) android_app->userData;
	switch (cmd) {
	case APP_CMD_START:
		info("onStart()");
		break;
	case APP_CMD_RESUME:
		info("onResume()");
		app->resumed = true;
		break;
	case APP_CMD_PAUSE:
		info("onPause()");
		app->resumed = false;
		break;
	case APP_CMD_STOP:
		info("onStop()");
		break;
	case APP_CMD_DESTROY:
		info("onDestroy()");
		app->window = NULL;
		break;
	case APP_CMD_INIT_WINDOW:
		info("surfaceCreated()");
		app->window = android_app->window;
		break;
	case APP_CMD_TERM_WINDOW:
		info("surfaceDestroyed()");
		app->window = NULL;
		break;
	default:
		break;
	}
}

static void app_update_vr_mode(struct app *app) {
	if (app->resumed && app->window != NULL) {
		if (app->ovr == NULL) {
			ovrModeParms mode_parms = vrapi_DefaultModeParms(app->java);
			mode_parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			mode_parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
			mode_parms.Display = (size_t) app->egl.display;
			mode_parms.WindowSurface = (size_t) app->window;
			mode_parms.ShareContext = (size_t) app->egl.context;

			info("enter vr mode");
			app->ovr = vrapi_EnterVrMode(&mode_parms);
			if (app->ovr == NULL) {
				error("can't enter vr mode");
				exit(EXIT_FAILURE);
			}

			vrapi_SetClockLevels(app->ovr, CPU_LEVEL, GPU_LEVEL );
			// vrapi_SetPerfThread(app->ovr, VRAPI_PERF_THREAD_TYPE_MAIN, gettid());
			// vrapi_SetPerfThread(app->ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, gettid());
		}
	} else {
		if (app->ovr != NULL) {
			info("leave vr mode");
			vrapi_LeaveVrMode(app->ovr);
			app->ovr = NULL;
		}
	}
}

static void app_handle_input(struct app *app) {
	bool back_button_down_current_frame = false;

	int i = 0;
	ovrInputCapabilityHeader capability;
	while (vrapi_EnumerateInputDevices(app->ovr, i, &capability) >= 0) {
		if (capability.Type == ovrControllerType_TrackedRemote) {
			ovrInputStateTrackedRemote input_state;
			input_state.Header.ControllerType = ovrControllerType_TrackedRemote;
			if (vrapi_GetCurrentInputState(app->ovr, capability.DeviceID, &input_state.Header) == ovrSuccess) {
				back_button_down_current_frame |= input_state.Buttons & ovrButton_Back;
				back_button_down_current_frame |= input_state.Buttons & ovrButton_B;
				back_button_down_current_frame |= input_state.Buttons & ovrButton_Y;
			}
		}
		++i;
	}

	if (app->back_button_down_previous_frame && !back_button_down_current_frame) {
		vrapi_ShowSystemUI(app->java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
	}
	app->back_button_down_previous_frame = back_button_down_current_frame;
}


static void app_create(struct app *app, ovrJava *java) {
	app->java = java;
	egl_create(&app->egl);
	renderer_create(
		&app->renderer,
		vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
		vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT)
	);
	app->resumed = false;
	app->window = NULL;
	app->ovr = NULL;
	app->back_button_down_previous_frame = false;
	app->frame_index = 0;
}

static void app_destroy(struct app *app) {
	egl_destroy(&app->egl);
	renderer_destroy(&app->renderer);
}

void android_main(struct android_app *android_app) {
	ANativeActivity_setWindowFlags(android_app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

	info("attach current thread");
	ovrJava java;
	java.Vm = android_app->activity->vm;
	(*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
	java.ActivityObject = android_app->activity->clazz;

	info("initialize vr api");
	const ovrInitParms init_parms = vrapi_DefaultInitParms(&java);
	if (vrapi_Initialize(&init_parms) != VRAPI_INITIALIZE_SUCCESS) {
		info("can't initialize vr api");
		exit(EXIT_FAILURE);
	}

	struct app app;
	app_create(&app, &java);

	android_app->userData = &app;
	android_app->onAppCmd = app_on_cmd;
	while (!android_app->destroyRequested) {
		for (;;) {
			int events = 0;
			struct android_poll_source *source = NULL;
			if (ALooper_pollAll(android_app->destroyRequested || app.ovr != NULL ? 0 : -1, NULL, &events, (void **) &source) < 0) {
				break;
			}
			if (source != NULL) {
				source->process(android_app, source);
			}

			app_update_vr_mode( &app );
		}

		app_handle_input( &app );

		if (app.ovr == NULL) {
			continue;
		}
		app.frame_index++;
		const double display_time = vrapi_GetPredictedDisplayTime(app.ovr, app.frame_index);
		ovrTracking2 tracking = vrapi_GetPredictedTracking2(app.ovr, display_time);
		const ovrLayerProjection2 layer = renderer_render_frame(&app.renderer, &tracking);
		const ovrLayerHeader2 * layers[] = {
			&layer.Header
		};
		ovrSubmitFrameDescription2 frame;
		frame.Flags = 0;
		frame.SwapInterval = 1;
		frame.FrameIndex = app.frame_index;
		frame.DisplayTime = display_time;
		frame.LayerCount = 1;
		frame.Layers = layers;
		vrapi_SubmitFrame2( app.ovr, &frame );
	}

	app_destroy(&app);

	info("shut down vr api");
	vrapi_Shutdown();

	info("detach current thread");
	(*java.Vm)->DetachCurrentThread( java.Vm );
}
