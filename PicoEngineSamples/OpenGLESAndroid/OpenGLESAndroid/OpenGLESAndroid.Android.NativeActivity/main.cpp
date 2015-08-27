/*
* Copyright (C) 2010 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/
#include "picoengine.h"
#include <cmath>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

/**
* Our saved state data.
*/
struct saved_state {
	float angle;
	int32_t x;
	int32_t y;
};

/**
* Shared state for our app.
*/
struct engine {
	struct android_app* app;

	bool windowInitialised;
	struct saved_state state;
};

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	struct engine* engine = (struct engine*)app->userData;
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		engine->state.x = AMotionEvent_getX(event, 0);
		engine->state.y = AMotionEvent_getY(event, 0);
		return 1;
	}
	return 0;
}

/**
* Process the next main command.
*/

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	struct engine* engine = (struct engine*)app->userData;
	switch (cmd) {
	case APP_CMD_SAVE_STATE:
		// The system has asked us to save our current state.  Do so.
		engine->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)engine->app->savedState) = engine->state;
		engine->app->savedStateSize = sizeof(struct saved_state);
		break;
	case APP_CMD_INIT_WINDOW:
		// The window is being shown, get it ready.
		if (engine->app->window != NULL) {
			engine->windowInitialised = true;
		}
		break;
	case APP_CMD_TERM_WINDOW:
		break;
	case APP_CMD_GAINED_FOCUS:
		break;
	case APP_CMD_LOST_FOCUS:
		break;
	}
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;


	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	while (!engine.windowInitialised)
	{
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		while ((ident = ALooper_pollAll(0, NULL, &events,
			(void**)&source)) >= 0) {

			// Process this event.
			if (source != NULL) {
				source->process(state, source);
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				return;
			}
		}
	}

	//Shaders...
	const std::string vs = "								  \n\
			uniform mat4 mvp;								  \n\
			uniform mat4 model;								  \n\
			attribute vec3 a_position;						  \n\
			attribute vec2 a_texcoord;						  \n\
			varying vec2 v_texcoord;						  \n\
			varying vec4 v_position;						  \n\
			void main()										  \n\
			{												  \n\
				vec4 pos = vec4(a_position, 1.0);			  \n\
				gl_Position = pos * mvp;					  \n\
				v_texcoord = a_texcoord;					  \n\
				v_position = pos * model;					  \n\
			}";

	const std::string fs = "													   \n\
			precision mediump float;											   \n\
			varying vec2 v_texcoord;											   \n\
			varying vec4 v_position;											   \n\
			uniform sampler2D s_texture;										   \n\
			uniform vec3 g_light;												   \n\
			uniform bool g_useLighting;											   \n\
			void main()															   \n\
			{																	   \n\
				if (g_useLighting)												   \n\
				{																   \n\
					float l = length(v_position.xyz - g_light);					   \n\
					float atten = min(2.0, 10.0 / (1.0 + l*1.0 + 0.8*l*l));		   \n\
					gl_FragColor = texture2D(s_texture, v_texcoord) * atten;	   \n\
				}																   \n\
				else															   \n\
				{																   \n\
					gl_FragColor = texture2D(s_texture, v_texcoord);			   \n\
				}																   \n\
			}";

	//The example...
	CogitareComputing::Pico::Engine picoEngine("LD33", 640, 480, 60, vs, fs, CogitareComputing::Pico::SystemSpecificData(engine.app));
	auto msh = picoEngine.LoadMesh("picocube.obj");
	auto gob = std::make_shared<CogitareComputing::Pico::SimpleGameObject>(msh);
	picoEngine.AddGameObject(gob);
	picoEngine.SetCameraPos(CogitareComputing::Pico::Vec3(0.0f, 0.0f, -2.0f));
	gob->SetPosition(CogitareComputing::Pico::Vec3(0.0f, 0.0f, 2.0f));
	picoEngine.SetLight(CogitareComputing::Pico::Vec3(1.0f, 1.0f, 1.0f));
	auto angle = 0.0f;
	picoEngine.Run([&picoEngine, &gob, &angle](double elapsed)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (picoEngine.IsKeyDown(27))
			return false;

		gob->SetRotation(CogitareComputing::Pico::Vec3(angle, angle, angle));
		angle += 0.1f;
		return true;
	});
	picoEngine.RemoveGameObject(gob->Id());

}
