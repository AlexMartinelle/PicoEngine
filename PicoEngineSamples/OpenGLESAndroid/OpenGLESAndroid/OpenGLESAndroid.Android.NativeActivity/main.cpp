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

class AndroidWrapper : public CogitareComputing::Pico::IAndroidWrapper
{
	static const int AccelerometerLooperId = LOOPER_ID_USER + 1;
public:
	AndroidWrapper(android_app* androidApp)
		: app(androidApp)
		, windowInitialised(false)
		, windowTerminated(false)
		, windowPause(false)
	{
		app->userData = this;

		// We are starting with a previous saved state; restore from it.
		if (androidApp != nullptr && androidApp->savedState != nullptr)
			state = *(struct saved_state*) androidApp->savedState;
	}

	virtual ~AndroidWrapper() {}


	bool Pump() override
	{
		return !windowTerminated;
	}

	bool HasKey(int k)
	{
		return false;
	}

	bool GetAsyncKeyState(char rk) override
	{
		return false;
	}

	bool ProcessEvents(int looperId) override
	{
		if (looperId == AccelerometerLooperId)
		{
			ASensorEvent event;
			while (ASensorEventQueue_getEvents(sensorEventQueue, &event, 1) > 0) {
			}
		}
		return true;
	}

	void PrepareAccelerometer()
	{
		//// Prepare to monitor accelerometer
		//sensorManager = ASensorManager_getInstance();
		//accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager,
		//	ASENSOR_TYPE_ACCELEROMETER);
		//sensorEventQueue = ASensorManager_createEventQueue(sensorManager,
		//	app->looper, AccelerometerLooperId, NULL, NULL);
	}

	android_app* AndroidApp() const override
	{
		return app;
	}

	int Width() const
	{
		if (app == nullptr || app->window == nullptr)
			return 0;

		return ANativeWindow_getWidth(app->window);
	}

	int Height() const
	{
		if (app == nullptr || app->window == nullptr)
			return 0;

		return ANativeWindow_getHeight(app->window);
	}

	bool WaitForInitialization()
	{
		while (!windowInitialised)
		{
			// Read all pending events.
			int ident;
			int events;
			struct android_poll_source* source;

			while ((ident = ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0)
			{
				// Process this event.
				if (source != nullptr)
					source->process(app, source);

				// Check if we are exiting.
				if (app->destroyRequested != 0) {
					return false;
				}
			}
		}

		return true;
	}

	struct android_app* app;
	bool windowInitialised;
	bool windowTerminated;
	bool windowPause;

	ASensorEventQueue* sensorEventQueue;

	struct saved_state state;
};

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	AndroidWrapper* w = reinterpret_cast<AndroidWrapper*>(app->userData);
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		w->state.x = AMotionEvent_getX(event, 0);
		w->state.y = AMotionEvent_getY(event, 0);
		return 1;
	}
	return 0;
}

/**
* Process the next main command.
*/

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	AndroidWrapper* w = reinterpret_cast<AndroidWrapper*>(app->userData);
	switch (cmd) {
	case APP_CMD_SAVE_STATE:
		// The system has asked us to save our current state.  Do so.
		w->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)w->app->savedState) = w->state;
		w->app->savedStateSize = sizeof(struct saved_state);
		break;
	case APP_CMD_INIT_WINDOW:
		// The window is being shown, get it ready.
		if (w->app->window != NULL) {
			w->windowInitialised = true;
		}
		break;
	case APP_CMD_TERM_WINDOW:
		// The window is being hidden or closed, clean it up.
		if (w->app->window != NULL) {
			w->windowTerminated = true;
		}
		break;
	case APP_CMD_GAINED_FOCUS:
		break;
	case APP_CMD_LOST_FOCUS:
		w->windowPause = true;
		break;
	}
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state)
{
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

	AndroidWrapper wrapper(state);
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;

	wrapper.PrepareAccelerometer();

	wrapper.WaitForInitialization();

	//The example...
	CogitareComputing::Pico::Engine picoEngine("LD33", 640, 480, 60, vs, fs, CogitareComputing::Pico::SystemSpecificData(&wrapper));
	picoEngine.PlayOggMusic("trudelutt2.ogg", false);
	auto sound = picoEngine.GetSound("powerup.wav");
	auto msh = picoEngine.LoadMesh("picocube.obj");
	auto gob = std::make_shared<CogitareComputing::Pico::SimpleGameObject>(msh);
	picoEngine.AddGameObject(gob);
	picoEngine.SetCameraPos(CogitareComputing::Pico::Vec3(0.0f, 0.0f, -2.0f));
	gob->SetPosition(CogitareComputing::Pico::Vec3(0.0f, 0.0f, 2.0f));
	picoEngine.SetLight(CogitareComputing::Pico::Vec3(1.0f, 1.0f, 1.0f));
	auto angle = 0.0f;
	sound->Play(false);
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
