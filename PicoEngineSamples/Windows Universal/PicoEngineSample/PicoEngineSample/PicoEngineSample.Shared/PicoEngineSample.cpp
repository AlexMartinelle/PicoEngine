#include "pch.h"
#include "PicoEngineSample.h"
#include <GLES2/gl2.h>

namespace SampleApp
{

	//Just a simple example of how to use the SimpleGameObject class..
	class MoveableGameObject : public CogitareComputing::Pico::SimpleGameObject
	{
	public:
		MoveableGameObject(CogitareComputing::Pico::MeshPtr mesh)
			:CogitareComputing::Pico::SimpleGameObject(mesh)
		{}
		virtual ~MoveableGameObject() {}

		void MoveObject(const CogitareComputing::Pico::Vec3& pos)
		{
			auto newPos = Position();
			newPos.X += pos.X;
			newPos.Y += pos.Y;
			newPos.Z += pos.Z;
			SetPosition(newPos);
		}
	};

	void PicoEngineSample::Run(CogitareComputing::Pico::SystemSpecificData& sData, int width, int height)
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

		//The example...
		CogitareComputing::Pico::Engine engine("LD33", width, height, 1, vs, fs, sData);
		engine.PlayOggMusic("trudelutt2.ogg", false);
		auto sound = engine.GetSound("powerup.wav");
		auto msh = engine.LoadMesh("picocube.obj");
		auto gob = std::make_shared<MoveableGameObject>(msh);
		engine.AddGameObject(gob);
		engine.SetCameraPos(CogitareComputing::Pico::Vec3(0.0f, 0.0f, -2.0f));
		gob->SetPosition(CogitareComputing::Pico::Vec3(0.0f, 0.0f, 2.0f));
		engine.SetLight(CogitareComputing::Pico::Vec3(1.0f, 1.0f, 1.0f));
		auto angle = 0.0f;
		engine.Run([&engine, &gob, &sound, &angle](double elapsed)
		{
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			if (engine.IsKeyDown(27))
				return false;
			if (engine.IsKeyDown('a'))
				gob->MoveObject(CogitareComputing::Pico::Vec3(-0.1f, 0.0f, 0.0f));
			if (engine.IsKeyDown('d'))
				gob->MoveObject(CogitareComputing::Pico::Vec3(0.1f, 0.0f, 0.0f));
			if (engine.IsKeyDown('s'))
				sound->Play(false);

			gob->SetRotation(CogitareComputing::Pico::Vec3(angle, angle, angle));
			angle += 0.1f;
			return true;
		});
		engine.RemoveGameObject(gob->Id());

	}
}