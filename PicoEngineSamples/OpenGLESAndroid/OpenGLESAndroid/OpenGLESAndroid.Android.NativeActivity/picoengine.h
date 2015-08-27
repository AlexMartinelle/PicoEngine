#pragma once
#include <string>
#include <memory>
#include <functional>
#include <stdexcept>

namespace CogitareComputing
{
	//Pico::Engine
	//
	//A very small, very simple, base of an OpenGL ES 2.0 game rendering engine, that can load Wavefront OBJ files with PNG textures.
	//Inspired by picoPNG http://lodev.org/lodepng/ .
	//Uses picoPNG http://lodev.org/lodepng/ (obviously) & tiny_obj_loader https://github.com/syoyo/tinyobjloader
	//
	//Warning: Only use this engine for rapid development of prototypes. 
	//
	//
	// Copyright(c) 2015 Alexander Martinelle, Cogitare Computing
	// 
	// 	This software is provided 'as-is', without any express or implied
	// 	warranty.In no event will the authors be held liable for any damages
	// 	arising from the use of this software.
	// 
	// 	Permission is granted to anyone to use this software for any purpose,
	// 	including commercial applications, and to alter it and redistribute it
	// 	freely, subject to the following restrictions :
	// 
	//  1. The origin of this software must not be misrepresented; you must not
	// 	claim that you wrote the original software.If you use this software
	// 	in a product, an acknowledgment in the product documentation would be
	// 	appreciated but is not required.
	// 
	// 	2. Altered source versions must be plainly marked as such, and must not be
	// 	misrepresented as being the original software.
	// 
	// 	3. This notice may not be removed or altered from any source
	// 	distribution.
	//
	//
	//The following macros switch between different implementation details for different platforms
	//PICO_WINDOWS        - A windows version that requires ANGLE https://code.google.com/p/angleproject/
	//PICO_ANDROID        - An android version using native glue, very basic support
	//PICO_PI             - A Raspberry Pi version

#ifdef PICO_PI
#define override
#endif
	namespace Pico
	{
		//-------------------------------------------------------------------------------------------------
		
		class EngineException : std::runtime_error
		{
		public:
			EngineException(const std::string& msg);
			virtual ~EngineException() throw();
		};

		//-------------------------------------------------------------------------------------------------

		struct Vec3
		{
			Vec3(float x, float y ,float z)
				:X(x)
				,Y(y)
				,Z(z)
			{}

			Vec3()
			#ifdef PICO_PI
                :X(0.0f)
                ,Y(0.0f)
                ,Z(0.0f)
			#else
				:Vec3(0.0f, 0.0f, 0.0f)
            #endif
			{}

			Vec3(const Vec3&) = default;
			Vec3(Vec3&&) = default;
			Vec3& operator=(Vec3&&) = default;
			Vec3& operator=(const Vec3&) = default;

			float X;
			float Y;
			float Z;
		};

		//-------------------------------------------------------------------------------------------------

		class IMesh
		{
		public:
			virtual ~IMesh();
			size_t Id() const;
		protected:
			IMesh();

			static size_t s_meshIdCnt;
			size_t m_meshId;
		};
		typedef std::shared_ptr<IMesh> MeshPtr;

		//-------------------------------------------------------------------------------------------------

		class IGameObject
		{
		public:
			IGameObject(MeshPtr mesh);
			virtual ~IGameObject();
			size_t Id() const;
			void SetPosition(const Vec3& pos);
			const Vec3& Position() const;
			void SetRotation(const Vec3& pos);
			const Vec3& Rotation() const;
			void SetScale(const Vec3& pos);
			const Vec3& Scale() const;
			void UseLighting(const bool lighting);
			bool Update(const double elapsedTime);
			struct RenderDetails;
			void Render(const double elapsedTime, const RenderDetails& details);
		protected:
			virtual Vec3 UpdatePosition(const Vec3& pos) { return pos; }
			virtual Vec3 UpdateRotation(const Vec3& rot) { return rot; }
			virtual Vec3 UpdateScale(const Vec3& scale) { return scale; }
			virtual bool UpdateState(const double elapsedTime) = 0;
			virtual void AdditionalRenderInstructionsBefore(const double elapsedTime) {}
			virtual void AdditionalRenderInstructionsAfter(const double elapsedTime) {}
			const IMesh& Mesh() const;
		private:
			void UpdateTranslationMatrix(const Vec3 & pos);
			void UpdateScaleMatrix(const Vec3 & scale);
			Vec3 m_position;
			Vec3 m_rotation;
			Vec3 m_scale;
			bool m_lighting;
			MeshPtr m_mesh;
			static size_t s_gobId;
			size_t m_gobId;
			float m_rotationMatrix[4][4];
			float m_translationMatrix[4][4];
			float m_scaleMatrix[4][4];
		};
		typedef std::shared_ptr<IGameObject> GameObjectPtr;

		//-------------------------------------------------------------------------------------------------

		class SimpleGameObject : public IGameObject
		{
		public:
			SimpleGameObject(Pico::MeshPtr mesh);
			virtual ~SimpleGameObject();
		protected:
			Pico::Vec3 UpdatePosition(const Pico::Vec3& pos);
			bool UpdateState(const double elapsedTime);
		};
		typedef std::shared_ptr<SimpleGameObject> SimpleGameObjectPtr;

		//-------------------------------------------------------------------------------------------------

		class SimpleParticleObject : public IGameObject
		{
		public:
			SimpleParticleObject(Pico::MeshPtr mesh);
			virtual ~SimpleParticleObject();
		protected:
			Pico::Vec3 UpdatePosition(const Pico::Vec3& pos);
			bool UpdateState(const double elapsedTime);
			void AdditionalRenderInstructionsBefore(const double elapsedTime) override;
			void AdditionalRenderInstructionsAfter(const double elapsedTime) override;
		};
		typedef std::shared_ptr<SimpleParticleObject> SimpleParticleObjectPtr;

		//-------------------------------------------------------------------------------------------------
#ifdef PICO_WINDOWS
		struct SystemSpecificData {};
#endif
#ifdef PICO_PI
		struct SystemSpecificData {};
#endif
#ifdef PICO_ANDROID
		struct SystemSpecificData
		{
			SystemSpecificData(android_app* app):m_app(app){}
			android_app* m_app;
		};
#endif
		//-------------------------------------------------------------------------------------------------

		class Engine
		{
		public:
			Engine(const std::string & windowName, const size_t width, const size_t height, const size_t swapInterval, 
				   const std::string& vertexShader, const std::string& pixelShader, SystemSpecificData sData);
			bool Run(std::function<bool(const double)> callBack);
			bool AddGameObject(GameObjectPtr gob);
			bool RemoveGameObject(const size_t id);
			void SetCameraPos(const Vec3& pos);
			void SetCameraRotation(const Vec3& rot);
			MeshPtr LoadMesh(const std::string& meshName);
			bool IsKeyDown(char key) const;
			void SetLight(const Vec3& pos);
			~Engine();
		private:
			struct Impl; //Engine internals
			std::unique_ptr<Impl> m_impl;
		};

		//-------------------------------------------------------------------------------------------------

	}
}