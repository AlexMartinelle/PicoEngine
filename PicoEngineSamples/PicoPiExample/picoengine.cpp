#include "picoengine.h"

#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "picopng.h"
#include "tiny_obj_loader.h"

#include <cmath>
#include <istream>
#include <fstream>
#include <unordered_map>
#include <deque>
#include <sstream>

#ifdef PICO_ANDROID
#include "../android_native_app_glue.h"
#include <time.h>
#include <streambuf>
#endif

#ifdef PICO_PI
#include <bcm_host.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <unordered_set>
#include <iostream>
#endif

#ifdef SOUND_SUPPORT
#include <al.h>
#include <alc.h>
#endif


#ifdef OGG_PLAYBACK
#include "stb_vorbis.c"
#endif

namespace CogitareComputing
{
	namespace Pico
	{
		EngineException::EngineException(const std::string& msg)
			:std::runtime_error(msg)
		{}

		EngineException::~EngineException() throw() /*Throw specificier is for Pi compatibility*/
		{}


#ifdef PICO_ANDROID
		template <class T>
		void LoadAssetFile(SystemSpecificData& sData, std::vector<T>& buffer, const std::string& filename) //designed for loading files from hard disk in an std::vector
		{
			AAssetManager* assetManager = sData.m_androidWrapper->AndroidApp()->activity->assetManager;
			AAsset* assetFile = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_BUFFER);
			if (!assetFile)
			{
				throw EngineException("Failed to open:" + filename);
			}

			uint8_t* data = (uint8_t*)AAsset_getBuffer(assetFile);
			int32_t size = AAsset_getLength(assetFile);
			if (data == NULL)
			{
				AAsset_close(assetFile);
				throw EngineException("Failed to load:" + filename);
			}

			buffer.reserve(size);
			buffer.assign(data, data + size);

			AAsset_close(assetFile);
		}
#endif
	}

	namespace Pico
	{
		Vec3& Vec3::operator+=(const Vec3& other)
		{
			X += other.X;
			Y += other.Y;
			Z += other.Z;
			return *this;
		}
		Vec3& Vec3::operator-=(const Vec3& other)
		{
			X -= other.X;
			Y -= other.Y;
			Z -= other.Z;
			return *this;
		}

		Vec3 operator+(Vec3 lhs, const Vec3& rhs)
		{
			lhs.X += rhs.X;
			lhs.Y += rhs.Y;
			lhs.Z += rhs.Z;
			return lhs;
		}
		Vec3 operator-(Vec3 lhs, const Vec3& rhs)
		{
			lhs.X -= rhs.X;
			lhs.Y -= rhs.Y;
			lhs.Z -= rhs.Z;
			return lhs;
		}

	}
	namespace MatrixTools
	{
		template <int N>
		void MatrixMul(const float(&lhs)[N][N], const float(&rhs)[N][N], float(&out)[N][N])
		{
			for (unsigned int i = 0; i != 4; ++i)
			{
				for (unsigned int j = 0; j != 4; ++j)
				{
					out[i][j] = lhs[i][0] * rhs[0][j] +
						lhs[i][1] * rhs[1][j] +
						lhs[i][2] * rhs[2][j] +
						lhs[i][3] * rhs[3][j];
				}
			}
		}

		template <int N>
		void UpdateRotationMatrix(const Pico::Vec3& rot, float(&rotationMatrix)[N][N])
		{
			float m_xrot[4][4];
			float m_yrot[4][4];
			float m_zrot[4][4];

			m_xrot[0][0] = 1.0f; m_xrot[0][1] = 0.0f;		 m_xrot[0][2] = 0.0f;	        m_xrot[0][3] = 0.0f;
			m_xrot[1][0] = 0.0f; m_xrot[1][1] = cosf(rot.X); m_xrot[1][2] = -sinf(rot.X);   m_xrot[1][3] = 0.0f;
			m_xrot[2][0] = 0.0f; m_xrot[2][1] = sinf(rot.X); m_xrot[2][2] = cosf(rot.X);    m_xrot[2][3] = 0.0f;
			m_xrot[3][0] = 0.0f; m_xrot[3][1] = 0.0f;		 m_xrot[3][2] = 0.0f;		    m_xrot[3][3] = 1.0f;

			m_yrot[0][0] = cosf(rot.Y); m_yrot[0][1] = 0.0f; m_yrot[0][2] = -sinf(rot.Y); m_yrot[0][3] = 0.0f;
			m_yrot[1][0] = 0.0f;		m_yrot[1][1] = 1.0f; m_yrot[1][2] = 0.0f;		  m_yrot[1][3] = 0.0f;
			m_yrot[2][0] = sinf(rot.Y); m_yrot[2][1] = 0.0f; m_yrot[2][2] = cosf(rot.Y);  m_yrot[2][3] = 0.0f;
			m_yrot[3][0] = 0.0f;		m_yrot[3][1] = 0.0f; m_yrot[3][2] = 0.0f;		  m_yrot[3][3] = 1.0f;

			m_zrot[0][0] = cosf(rot.Z); m_zrot[0][1] = -sinf(rot.Z); m_zrot[0][2] = 0.0f; m_zrot[0][3] = 0.0f;
			m_zrot[1][0] = sinf(rot.Z); m_zrot[1][1] = cosf(rot.Z);  m_zrot[1][2] = 0.0f; m_zrot[1][3] = 0.0f;
			m_zrot[2][0] = 0.0f;	    m_zrot[2][1] = 0.0f;	     m_zrot[2][2] = 1.0f; m_zrot[2][3] = 0.0f;
			m_zrot[3][0] = 0.0f;	    m_zrot[3][1] = 0.0f;	     m_zrot[3][2] = 0.0f; m_zrot[3][3] = 1.0f;

			float temp[4][4];
			MatrixMul(m_yrot, m_xrot, temp);
			MatrixMul(m_zrot, temp, rotationMatrix);
		}


	}

	//-------------------------------------------------------------------------------------------------

	namespace ShaderTools
	{
		GLuint Compile(GLenum type, const std::string &source)
		{
			auto shaderId = glCreateShader(type);

			const char* shaderStr[1] = { source.c_str() };
			glShaderSource(shaderId, 1, shaderStr, nullptr);
			glCompileShader(shaderId);

			GLint res;
			glGetShaderiv(shaderId, GL_COMPILE_STATUS, &res);
			if (res != 0)
				return shaderId;

			GLint infoLogSize;
			glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogSize);

			std::vector<GLchar> errorInfo(infoLogSize);
			glGetShaderInfoLog(shaderId, errorInfo.size(), nullptr, &errorInfo[0]);
			glDeleteShader(shaderId);
			throw Pico::EngineException(("Shader compile failed!:" + std::string(&errorInfo[0])));
		}

		struct SelfDeletingShader
		{
			GLuint m_shaderNo;
			const std::string& m_shader;
			GLenum m_shaderType;

			SelfDeletingShader(GLenum type, const std::string& shader)
				:m_shaderNo(0)
				, m_shader(shader)
				, m_shaderType(type)
			{
			}

			void Compile()
			{
				m_shaderNo = ShaderTools::Compile(m_shaderType, m_shader);
			}

			~SelfDeletingShader()
			{
				glDeleteShader(m_shaderNo);
			}
		};

		GLuint CompileProgram(const std::string &vertexShader, const std::string &pixelShader)
		{
			auto prog = glCreateProgram();
			try
			{
				SelfDeletingShader vShader(GL_VERTEX_SHADER, vertexShader);
				SelfDeletingShader fShader(GL_FRAGMENT_SHADER, pixelShader);
				vShader.Compile();
				fShader.Compile();
				glAttachShader(prog, vShader.m_shaderNo);
				glAttachShader(prog, fShader.m_shaderNo);
			}
			catch (Pico::EngineException e)
			{
				glDeleteProgram(prog);
				throw;
			}


			glLinkProgram(prog);

			GLint linkStat;
			glGetProgramiv(prog, GL_LINK_STATUS, &linkStat);
			if (linkStat != 0)
				return prog;

			GLint errorMsgLen;
			glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &errorMsgLen);

			std::vector<GLchar> errorInfo(errorMsgLen);
			glGetProgramInfoLog(prog, errorInfo.size(), nullptr, &errorInfo[0]);
			glDeleteProgram(prog);

			throw Pico::EngineException(("Shader program link failed:" + std::string(&errorInfo[0])));
		}
	}

	//-------------------------------------------------------------------------------------------------

	struct Event
	{
		enum class Type
		{
			None,
			WindowClosed
		};

		explicit Event(Type type = Type::None)
			:m_type(type)
		{}

		Type m_type;
	};

	//-------------------------------------------------------------------------------------------------

	namespace Pico
	{
#ifdef PICO_UNIVERSAL
		//-------------------------------------------------------------------------------------------------


		class TimeRetriever
		{
		public:
			TimeRetriever()
				:m_startTick(::GetTickCount64())
			{}

			double ElapsedTimeInSeconds() const
			{
				return static_cast<double>(::GetTickCount64() - static_cast<double>(m_startTick)) / 1000.0;
			}

		private:
			ULONGLONG m_startTick;
		};

		//-------------------------------------------------------------------------------------------------
		class NativeWindow
		{
		public:
			NativeWindow(const std::string& windowName, const size_t windowWidth, const size_t windowHeight, SystemSpecificData systemSpec)
				:m_name(windowName)
				, m_wndWrapper(systemSpec.WindowWrapper)
				, m_width(windowWidth)
				, m_height(windowHeight)
			{
				Reset();
			}

			~NativeWindow()
			{
				Reset();
			}

			void ShowWindow(bool isVisible)
			{
			}

			IWindowWrapperPtr Window() const
			{
				return m_wndWrapper;
			}

			//EGLNativeDisplayType Display() const
			//{
			//	return m_display;
			//}

			void MsgLoop()
			{
				if (!m_wndWrapper->Pump())
				{
					PushEvent(Event(Event::Type::WindowClosed));
				}
			}

			void SetWidth(const int width)
			{
				m_width = width;
			}

			void SetHeight(const int height)
			{
				m_height = height;
			}

			int Width() const
			{
				return m_width;
			}

			int Height() const
			{
				return m_height;
			}

			bool PopEvent(Event& event)
			{
				if (m_events.size() == 0)
					return false;

				event = m_events.front();
				m_events.pop_front();
				return true;
			}

			void PushEvent(Event event)
			{
				m_events.push_back(std::move(event));
			}

		private:
			void Reset()
			{
			}


			std::string m_name;

			IWindowWrapperPtr m_wndWrapper;

			size_t m_width;
			size_t m_height;

			std::deque<Event> m_events;
		};

		class EGLSupport
		{
		public:
			EGLSupport(NativeWindow& window, EGLint swapInterval = -1)
				:m_swapInterval(swapInterval)
				, m_window(window)
				, m_config(nullptr)
				, m_display(EGL_NO_DISPLAY)
				, m_surface(EGL_NO_SURFACE)
				, m_context(EGL_NO_CONTEXT)
			{
				Init(window, swapInterval);
			}

			void SwapBuffer()
			{
				// The call to eglSwapBuffers might not be successful (e.g. due to Device Lost)
				// If the call fails, then we must reinitialize EGL and the GL resources.
				if (eglSwapBuffers(m_display, m_surface) != GL_TRUE)
				{
					DestroyEGL();

					Init(m_window, m_swapInterval);
				}
			}

			void RefreshWindowData()
			{
				EGLint panelWidth = 0;
				EGLint panelHeight = 0;
				eglQuerySurface(m_display, m_surface, EGL_WIDTH, &panelWidth);
				eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &panelHeight);
				m_window.SetWidth(panelWidth);
				m_window.SetHeight(panelHeight);
			}


			~EGLSupport()
			{
				DestroyEGL();
			}

		private:

			void Init(const NativeWindow& window, EGLint swapInterval)
			{
				const EGLint configAttributes[] =
				{
					EGL_RED_SIZE, 8,
					EGL_GREEN_SIZE, 8,
					EGL_BLUE_SIZE, 8,
					EGL_ALPHA_SIZE, 8,
					EGL_DEPTH_SIZE, 8,
					EGL_STENCIL_SIZE, 8,
					EGL_NONE
				};

				const EGLint contextAttributes[] =
				{
					EGL_CONTEXT_CLIENT_VERSION, 2,
					EGL_NONE
				};

				const EGLint surfaceAttributes[] =
				{
					// EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (see above).
					// If you have compilation issues with it then please update your Visual Studio templates.
					EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
					EGL_NONE
				};

				const EGLint defaultDisplayAttributes[] =
				{
					// These are the default display attributes, used to request ANGLE's D3D11 renderer.
					// eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
					EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

					// EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER is an optimization that can have large performance benefits on mobile devices.
					// Its syntax is subject to change, though. Please update your Visual Studio templates if you experience compilation issues with it.
					EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,

					// EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call
					// the IDXGIDevice3::Trim method on behalf of the application when it gets suspended.
					// Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
					EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
					EGL_NONE,
				};

				const EGLint fl9_3DisplayAttributes[] =
				{
					// These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
					// These attributes are used if the call to eglInitialize fails with the default display attributes.
					EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
					EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
					EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
					EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
					EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
					EGL_NONE,
				};

				const EGLint warpDisplayAttributes[] =
				{
					// These attributes can be used to request D3D11 WARP.
					// They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
					EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
					EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
					EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
					EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
					EGL_NONE,
				};

				m_config = NULL;

				// eglGetPlatformDisplayEXT is an alternative to eglGetDisplay. It allows us to pass in display attributes, used to configure D3D11.
				PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
				if (!eglGetPlatformDisplayEXT)
					throw EngineException("Failed to get function eglGetPlatformDisplayEXT");

				//
				// To initialize the display, we make three sets of calls to eglGetPlatformDisplayEXT and eglInitialize, with varying
				// parameters passed to eglGetPlatformDisplayEXT:
				// 1) The first calls uses "defaultDisplayAttributes" as a parameter. This corresponds to D3D11 Feature Level 10_0+.
				// 2) If eglInitialize fails for step 1 (e.g. because 10_0+ isn't supported by the default GPU), then we try again
				//    using "fl9_3DisplayAttributes". This corresponds to D3D11 Feature Level 9_3.
				// 3) If eglInitialize fails for step 2 (e.g. because 9_3+ isn't supported by the default GPU), then we try again
				//    using "warpDisplayAttributes".  This corresponds to D3D11 Feature Level 11_0 on WARP, a D3D11 software rasterizer.
				//

				// This tries to initialize EGL to D3D11 Feature Level 10_0+. See above comment for details.
				m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
				if (m_display == EGL_NO_DISPLAY)
					throw EngineException("Failed to get EGL display");

				if (eglInitialize(m_display, NULL, NULL) == EGL_FALSE)
				{
					// This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile devices).
					m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
					if (m_display == EGL_NO_DISPLAY)
						throw EngineException("Failed to get EGL display");

					if (eglInitialize(m_display, NULL, NULL) == EGL_FALSE)
					{
						// This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
						m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
						if (m_display == EGL_NO_DISPLAY)
							throw EngineException("Failed to get EGL display");

						if (eglInitialize(m_display, NULL, NULL) == EGL_FALSE)
						{
							// If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
							throw EngineException("Failed to initialize EGL");
						}
					}
				}

				EGLint numConfigs = 0;
				if ((eglChooseConfig(m_display, configAttributes, &m_config, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0))
					throw EngineException("Failed to choose first EGLConfig");

				m_surface = eglCreateWindowSurface(m_display, m_config, m_window.Window()->NativeWindowType(), surfaceAttributes);
				if (m_surface == EGL_NO_SURFACE)
					throw EngineException("Failed to create EGL fullscreen surface");

				m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttributes);
				if (m_context == EGL_NO_CONTEXT)
					throw EngineException("Failed to create EGL context");

				if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE)
					throw EngineException("Failed to make fullscreen EGLSurface current");

				if (m_swapInterval != -1)
					eglSwapInterval(m_display, m_swapInterval);
			}

			void DestroyEGL()
			{
				if (m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE)
				{
					eglDestroySurface(m_display, m_surface);
					m_surface = EGL_NO_SURFACE;
				}

				if (m_display != EGL_NO_DISPLAY && m_context != EGL_NO_CONTEXT)
				{
					eglDestroyContext(m_display, m_context);
					m_context = EGL_NO_CONTEXT;
				}

				if (m_display != EGL_NO_DISPLAY)
				{
					eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
					eglTerminate(m_display);
					m_display = EGL_NO_DISPLAY;
				}
			}

			EGLint m_swapInterval;
			NativeWindow& m_window;
			EGLConfig m_config;
			EGLDisplay m_display;
			EGLSurface m_surface;
			EGLContext m_context;
		};

#endif

#ifdef PICO_WINDOWS
		LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

		class NativeWindow
		{
		public:
			NativeWindow(const std::string& windowName, const size_t windowWidth, const size_t windowHeight, SystemSpecificData)
				:m_name(windowName)
				, m_wndClassName(m_name + "Class")
				, m_wnd(nullptr)
				, m_display(nullptr)
				, m_width(windowWidth)
				, m_height(windowHeight)
			{
				Reset();

				const LPSTR idcArrow = MAKEINTRESOURCEA(32512);

				WNDCLASSEXA wndClass = { 0 };
				wndClass.cbSize = sizeof(WNDCLASSEXA);
				wndClass.style = 0;
				wndClass.lpfnWndProc = Pico::WindowProc;
				wndClass.cbClsExtra = 0;
				wndClass.cbWndExtra = 0;
				wndClass.hInstance = GetModuleHandle(NULL);
				wndClass.hIcon = NULL;
				wndClass.hCursor = LoadCursorA(NULL, idcArrow);
				wndClass.hbrBackground = 0;
				wndClass.lpszMenuName = NULL;
				wndClass.lpszClassName = m_wndClassName.c_str();
				if (!RegisterClassExA(&wndClass))
					throw EngineException("Failed to register window class!");

				DWORD parentStyle = WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
				DWORD parentExtendedStyle = WS_EX_APPWINDOW;

				RECT sizeRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
				AdjustWindowRectEx(&sizeRect, parentStyle, FALSE, parentExtendedStyle);

				m_wnd = CreateWindowExA(parentExtendedStyle, m_wndClassName.c_str(), m_name.c_str(), parentStyle,
					CW_USEDEFAULT, CW_USEDEFAULT,
					sizeRect.right - sizeRect.left, sizeRect.bottom - sizeRect.top, NULL, NULL,
					GetModuleHandle(NULL), this);
				if (!m_wnd)
				{
					Reset();
					throw EngineException("Failed to create window");
				}

				m_display = GetDC(m_wnd);
				if (!m_display)
				{
					Reset();
					throw EngineException("Failed to get DC");
				}
			}

			~NativeWindow()
			{
				Reset();
			}

			void ShowWindow(bool isVisible)
			{
				::ShowWindow(m_wnd, isVisible ? SW_SHOW : SW_HIDE);
			}

			EGLNativeWindowType Window() const
			{
				return m_wnd;
			}

			EGLNativeDisplayType Display() const
			{
				return m_display;
			}

			void MsgLoop()
			{
				MSG msg;
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			int Width() const
			{
				return m_width;
			}

			int Height() const
			{
				return m_height;
			}

			bool PopEvent(Event& event)
			{
				if (m_events.size() == 0)
					return false;

				event = m_events.front();
				m_events.pop_front();
				return true;
			}

			void PushEvent(Event event)
			{
				m_events.push_back(std::move(event));
			}

		private:
			void Reset()
			{
				if (m_display)
				{
					ReleaseDC(m_wnd, m_display);
					m_display = nullptr;
				}

				if (m_wnd)
				{
					DestroyWindow(m_wnd);
					m_wnd = nullptr;
				}

				UnregisterClassA(m_wndClassName.c_str(), nullptr);
			}


			std::string m_name;
			std::string m_wndClassName;

			EGLNativeWindowType m_wnd;
			EGLNativeDisplayType m_display;

			size_t m_width;
			size_t m_height;

			std::deque<Event> m_events;
		};

		//-------------------------------------------------------------------------------------------------

		LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_NCCREATE:
				SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
				return DefWindowProcA(hWnd, msg, wParam, lParam);
			case WM_DESTROY:
			case WM_CLOSE:
			{
				if (auto wnd = reinterpret_cast<Pico::NativeWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
					wnd->PushEvent(Event(Event::Type::WindowClosed));
				break;
			}
			}

			return DefWindowProcA(hWnd, msg, wParam, lParam);
		}

		//-------------------------------------------------------------------------------------------------


		class TimeRetriever
		{
		public:
			TimeRetriever()
				:m_startTick(::GetTickCount64())
			{}

			double ElapsedTimeInSeconds() const
			{
				return static_cast<double>(::GetTickCount64() - static_cast<double>(m_startTick)) / 1000.0;
			}

		private:
			ULONGLONG m_startTick;
		};

		//-------------------------------------------------------------------------------------------------

		class EGLSupport
		{
		public:
			EGLSupport(const NativeWindow& window, EGLint swapInterval = -1)
				:m_swapInterval(swapInterval)
				, m_config(nullptr)
				, m_display(EGL_NO_DISPLAY)
				, m_surface(EGL_NO_SURFACE)
				, m_context(EGL_NO_CONTEXT)
			{
				PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
				if (!eglGetPlatformDisplayEXT)
					throw EngineException("eglGetPlatformDisplayEXT not found!");

				const std::vector<EGLint> displayAttributes
					= { EGL_PLATFORM_ANGLE_TYPE_ANGLE , EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE ,
					EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE,
					EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE,
					EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_FALSE, EGL_NONE };

				m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, window.Display(), &displayAttributes[0]);
				if (m_display == EGL_NO_DISPLAY)
					throw EngineException("No EGL DISPLAY");

				EGLint majorVer;
				EGLint minorVer;
				if (eglInitialize(m_display, &majorVer, &minorVer) == EGL_FALSE)
					throw EngineException("Failed to initialize EGL");

				eglBindAPI(EGL_OPENGL_ES_API);
				if (eglGetError() != EGL_SUCCESS)
					throw EngineException("Failed to bind EGL_OPENGL_ES_API");

				const EGLint configAttributes[] =
				{
					EGL_RED_SIZE,       8,
					EGL_GREEN_SIZE,     8,
					EGL_BLUE_SIZE,      8,
					EGL_ALPHA_SIZE,     8,
					EGL_DEPTH_SIZE,     24,
					EGL_STENCIL_SIZE,   8,
					EGL_SAMPLE_BUFFERS, 0,
					EGL_NONE
				};

				EGLint configCount;
				if (!eglChooseConfig(m_display, configAttributes, &m_config, 1, &configCount) || (configCount != 1))
					throw EngineException("Failed to configure EGL");

				std::vector<EGLint> surfaceAttributes;
				if (strstr(eglQueryString(m_display, EGL_EXTENSIONS), "EGL_NV_post_sub_buffer") != nullptr)
				{
					surfaceAttributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
					surfaceAttributes.push_back(EGL_TRUE);
				}

				surfaceAttributes.push_back(EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER);
				surfaceAttributes.push_back(EGL_FALSE);

				surfaceAttributes.push_back(EGL_NONE);

				m_surface = eglCreateWindowSurface(m_display, m_config, window.Window(), &surfaceAttributes[0]);
				if (eglGetError() != EGL_SUCCESS)
					throw EngineException("Failed to create EGL surface");

				EGLint contextAttibutes[] =
				{
					EGL_CONTEXT_CLIENT_VERSION, 2,
					EGL_NONE
				};

				m_context = eglCreateContext(m_display, m_config, nullptr, contextAttibutes);
				if (eglGetError() != EGL_SUCCESS)
					throw EngineException("Failed to create EGL context");

				eglMakeCurrent(m_display, m_surface, m_surface, m_context);
				if (eglGetError() != EGL_SUCCESS)
					throw EngineException("Failed to make current EGL setup");

				if (m_swapInterval != -1)
					eglSwapInterval(m_display, m_swapInterval);
			}

			void RefreshWindowData()
			{
			}

			void SwapBuffer()
			{
				eglSwapBuffers(m_display, m_surface);
			}

			~EGLSupport()
			{
				DestroyEGL();
			}

		private:
			void DestroyEGL()
			{
				if (m_surface != EGL_NO_SURFACE)
				{
					eglDestroySurface(m_display, m_surface);
					m_surface = EGL_NO_SURFACE;
				}

				if (m_context != EGL_NO_CONTEXT)
				{
					eglDestroyContext(m_display, m_context);
					m_context = EGL_NO_CONTEXT;
				}

				if (m_display != EGL_NO_DISPLAY)
				{
					eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
					eglTerminate(m_display);
					m_display = EGL_NO_DISPLAY;
				}
			}

			EGLint m_swapInterval;
			EGLConfig m_config;
			EGLDisplay m_display;
			EGLSurface m_surface;
			EGLContext m_context;
		};
#endif

#ifdef PICO_ANDROID
		class NativeWindow
		{
		public:
			NativeWindow(const std::string& windowName, const size_t windowWidth, const size_t windowHeight, SystemSpecificData sData)
				:m_androidWrapper(sData.m_androidWrapper)
				, m_name(windowName)
				, m_width(windowWidth)
				, m_height(windowHeight)
			{}

			~NativeWindow()
			{
			}

			void ShowWindow(bool isVisible)
			{
			}

			EGLNativeWindowType Window() const
			{
				return m_wnd;
			}

			ANativeWindow* AppWindow() const
			{
				return m_androidWrapper->AndroidApp()->window;
			}

			EGLNativeDisplayType Display() const
			{
				return m_display;
			}

			void MsgLoop()
			{
				int ident;
				int events;
				int fdesc;

				struct android_poll_source* source;
				IAndroidWrapper* w = m_androidWrapper;
				if (w == nullptr)
					return;

				android_app* app = w->AndroidApp();

				while ((ident = ALooper_pollAll(0, &fdesc, &events,
					(void**)&source)) >= 0)
				{

					if (source != NULL) {
						source->process(app, source);
					}

					if (!w->ProcessEvents(ident))
					{
						PushEvent(Event(Event::Type::WindowClosed));
						break;
					}

					if (app->destroyRequested != 0)
					{
						PushEvent(Event(Event::Type::WindowClosed));
						break;
					}
				}

				if (!m_androidWrapper->Pump())
					PushEvent(Event(Event::Type::WindowClosed));
			}

			EGLint Width() const
			{
				return m_width;
			}

			EGLint Height() const
			{
				return m_height;
			}

			void SetWidth(EGLint width)
			{
				m_width = width;;
			}

			void SetHeight(EGLint height)
			{
				m_height = height;
			}

			bool PopEvent(Event& event)
			{
				if (m_events.size() == 0)
					return false;

				event = m_events.front();
				m_events.pop_front();
				return true;
			}

			void PushEvent(Event event)
			{
				m_events.push_back(std::move(event));
			}

		private:
			IAndroidWrapper* m_androidWrapper;
			std::string m_name;

			EGLNativeWindowType m_wnd;
			EGLNativeDisplayType m_display;

			EGLint m_width;
			EGLint m_height;

			std::deque<Event> m_events;
		};

		class EGLSupport
		{
		public:
			EGLSupport(NativeWindow& window, EGLint swapInterval = -1)
				:m_swapInterval(swapInterval)
				, m_config(nullptr)
				, m_display(EGL_NO_DISPLAY)
				, m_surface(EGL_NO_SURFACE)
				, m_context(EGL_NO_CONTEXT)
			{
				const EGLint attribs[] = {
					EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
					EGL_RED_SIZE,       8,
					EGL_GREEN_SIZE,     8,
					EGL_BLUE_SIZE,      8,
					EGL_ALPHA_SIZE,     8,
					EGL_DEPTH_SIZE,     8,
					//EGL_STENCIL_SIZE,   8,
					//EGL_SAMPLE_BUFFERS, 0,
					EGL_NONE
				};

				EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
				EGLint format;
				EGLint numConfigs;

				m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

				eglInitialize(m_display, 0, 0);
				eglChooseConfig(m_display, attribs, &m_config, 1, &numConfigs);

				auto res = eglGetConfigAttrib(m_display, m_config, EGL_NATIVE_VISUAL_ID, &format);

				if (res == EGL_FALSE)
					throw EngineException("Failed to get EGL config attrib");
				if (res == EGL_BAD_DISPLAY)
					throw EngineException("Bad display EGL config attrib");
				if (res == EGL_NOT_INITIALIZED)
					throw EngineException("EGL_NOT_INITIALIZED EGL config attrib");
				if (res == EGL_BAD_CONFIG)
					throw EngineException("EGL_BAD_CONFIG to get EGL config attrib");
				if (res == EGL_BAD_ATTRIBUTE)
					throw EngineException("EGL_BAD_ATTRIBUTE  to get EGL config attrib");

				ANativeWindow_setBuffersGeometry(window.AppWindow(), 0, 0, format);

				m_surface = eglCreateWindowSurface(m_display, m_config, window.AppWindow(), NULL);
				if (m_surface == 0)
					throw EngineException("Failed to create window surface");

				m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttribs);
				if (m_context == 0)
					throw EngineException("Failed to create context");

				if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE)
					throw EngineException("Unable to eglMakeCurrent");

				EGLint width = 0;
				EGLint height = 0;
				eglQuerySurface(m_display, m_surface, EGL_WIDTH, &width);
				eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &height);
				window.SetWidth(width);
				window.SetHeight(height);

				if (m_swapInterval != -1)
					eglSwapInterval(m_display, swapInterval);
			}

			void RefreshWindowData()
			{
			}

			void SwapBuffer()
			{
				eglSwapBuffers(m_display, m_surface);
			}

			~EGLSupport()
			{
				DestroyEGL();
			}

		private:
			void DestroyEGL()
			{
				if (m_surface != EGL_NO_SURFACE)
				{
					eglDestroySurface(m_display, m_surface);
					m_surface = EGL_NO_SURFACE;
				}

				if (m_context != EGL_NO_CONTEXT)
				{
					eglDestroyContext(m_display, m_context);
					m_context = EGL_NO_CONTEXT;
				}

				if (m_display != EGL_NO_DISPLAY)
				{
					eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
					eglTerminate(m_display);
					m_display = EGL_NO_DISPLAY;
				}
			}

			EGLint m_swapInterval;
			EGLConfig m_config;
			EGLDisplay m_display;
			EGLSurface m_surface;
			EGLContext m_context;
		};

		//-------------------------------------------------------------------------------------------------


		class TimeRetriever
		{
		public:
			TimeRetriever()
				:m_startTick(0)
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);
				m_startTick = t.tv_sec*1000000000.0 + t.tv_nsec;
			}

			double ElapsedTimeInSeconds() const
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);
				return (static_cast<double>(t.tv_sec*1000000000.0 + t.tv_nsec) - static_cast<double>(m_startTick)) / 1000000000.0;
			}

		private:
			long m_startTick;
		};

		//---------------------------------------------------------------------------

#endif

#ifdef PICO_PI
		class NativeWindow
		{
		public:
			NativeWindow(const std::string& windowName, const size_t windowWidth, const size_t windowHeight, SystemSpecificData sData)
				:m_name(windowName)
				, m_width(windowWidth)//Set this to the internal resolution you wish
				, m_height(windowHeight)//the screen to have
			{
				static bool oneTimeCraziness = true;
				if (oneTimeCraziness)
				{
					oneTimeCraziness = false;
					bcm_host_init();
				}

				VC_RECT_T dst_rect;
				VC_RECT_T src_rect;

				uint32_t width = m_width;
				uint32_t height = m_height;

				auto success = graphics_get_display_size(0 /* LCD */,
					&width, &height);
				if (success < 0)
					throw EngineException("Failed to get display");

				dst_rect.x = 0;
				dst_rect.y = 0;
				dst_rect.width = windowWidth;
				dst_rect.height = windowHeight;


				src_rect.x = 0;
				src_rect.y = 0;
				src_rect.width = m_width << 16;
				src_rect.height = m_height << 16;

				DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0 /* LCD */);
				DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

				VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,255,0 };
				DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(dispman_update,
					dispman_display, 0/*layer*/, &dst_rect, 0/*src*/,
					&src_rect, DISPMANX_PROTECTION_NONE, &alpha /*alpha*/,
					0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

				s_nativewindow.element = dispman_element;
				s_nativewindow.width = m_width;
				s_nativewindow.height = m_height;
				vc_dispmanx_update_submit_sync(dispman_update);
			}

			~NativeWindow()
			{
			}

			void ShowWindow(bool isVisible)
			{
			}

			EGLNativeWindowType Window() const
			{
				return m_wnd;
			}

			EGL_DISPMANX_WINDOW_T* AppWindow() const
			{
				return &s_nativewindow;
			}

			EGLNativeDisplayType Display() const
			{
				return m_display;
			}

			void MsgLoop()
			{
			}

			EGLint Width() const
			{
				return m_width;
			}

			EGLint Height() const
			{
				return m_height;
			}

			bool PopEvent(Event& event)
			{
				if (m_events.size() == 0)
					return false;

				event = m_events.front();
				m_events.pop_front();
				return true;
			}

			void PushEvent(Event event)
			{
				m_events.push_back(std::move(event));
			}

		private:
			std::string m_name;

			EGLNativeWindowType m_wnd;
			EGLNativeDisplayType m_display;

			EGLint m_width;
			EGLint m_height;

			std::deque<Event> m_events;

			static EGL_DISPMANX_WINDOW_T s_nativewindow;
		};
		EGL_DISPMANX_WINDOW_T NativeWindow::s_nativewindow;

		class EGLSupport
		{
		public:
			EGLSupport(const NativeWindow& window, EGLint swapInterval = -1)
				:m_swapInterval(swapInterval)
				, m_config(nullptr)
				, m_display(EGL_NO_DISPLAY)
				, m_surface(EGL_NO_SURFACE)
				, m_context(EGL_NO_CONTEXT)
			{

				const EGLint attribs[] = {
					EGL_RED_SIZE,       8,
					EGL_GREEN_SIZE,     8,
					EGL_BLUE_SIZE,      8,
					EGL_ALPHA_SIZE,     8,
					EGL_DEPTH_SIZE,     24,
					EGL_STENCIL_SIZE,   8,
					EGL_SAMPLE_BUFFERS, 0,
					EGL_NONE
				};
				EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
				EGLint numConfigs;

				m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);


				eglInitialize(m_display, 0, 0);

				eglChooseConfig(m_display, attribs, &m_config, 1, &numConfigs);

				m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttribs);
				if (m_context == 0)
					throw EngineException("Failed to create context");

				m_surface = eglCreateWindowSurface(m_display, m_config, window.AppWindow(), NULL);
				if (m_surface == 0)
					throw EngineException("Failed to create window surface");

				if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE)
					throw EngineException("Unable to eglMakeCurrent");

				if (m_swapInterval != -1)
					eglSwapInterval(m_display, m_swapInterval);

			}

			void RefreshWindowData()
			{
			}

			void SwapBuffer()
			{
				eglSwapBuffers(m_display, m_surface);
			}

			~EGLSupport()
			{
				DestroyEGL();
			}

		private:
			void DestroyEGL()
			{
				if (m_surface != EGL_NO_SURFACE)
				{
					eglDestroySurface(m_display, m_surface);
					m_surface = EGL_NO_SURFACE;
				}

				if (m_context != EGL_NO_CONTEXT)
				{
					eglDestroyContext(m_display, m_context);
					m_context = EGL_NO_CONTEXT;
				}

				if (m_display != EGL_NO_DISPLAY)
				{
					eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
					eglTerminate(m_display);
					m_display = EGL_NO_DISPLAY;
				}
			}

			EGLint m_swapInterval;
			EGLConfig m_config;
			EGLDisplay m_display;
			EGLSurface m_surface;
			EGLContext m_context;
		};

		//-------------------------------------------------------------------------------------------------


		class TimeRetriever
		{
		public:
			TimeRetriever()
				:m_startTick(0)
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);
				m_startTick = t.tv_sec*1000000000.0 + t.tv_nsec;
			}

			double ElapsedTimeInSeconds() const
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);
				return (static_cast<double>(t.tv_sec*1000000000.0 + t.tv_nsec) - static_cast<double>(m_startTick)) / 1000000000.0;
			}

		private:
			long m_startTick;
		};

		//---------------------------------------------------------------------------

#endif

	}

	//-------------------------------------------------------------------------------------------------
	namespace Pico
	{
		IMesh::IMesh()
			:m_meshId(s_meshIdCnt++)
		{
		}

		IMesh::~IMesh()
		{
		}

		size_t IMesh::Id() const
		{
			return m_meshId;
		}

		size_t IMesh::s_meshIdCnt = 0;

		//-------------------------------------------------------------------------------------------------

		struct IGameObject::RenderDetails
		{
			RenderDetails(const RenderDetails&) = delete;

			GLuint m_program;
			GLuint m_positionLoc;
			GLuint m_texCoordLoc;
			GLuint m_samplerLoc;
			GLuint m_mvpLoc;
			GLuint m_modelLoc;
			GLuint m_useLightingLocation;
			GLuint m_alphaLocation;

			float m_viewMatrix[4][4];
			float m_perspectiveMatrix[4][4];

			RenderDetails()
				:m_program(0)
				, m_positionLoc(0)
				, m_texCoordLoc(0)
				, m_samplerLoc(0)
				, m_mvpLoc(0)
				, m_modelLoc(0)
				, m_useLightingLocation(0)
				, m_alphaLocation(0)
			{}

			~RenderDetails()
			{
				if (m_program != 0)
					glDeleteProgram(m_program);
			}
		};

		//-------------------------------------------------------------------------------------------------
#ifdef PICO_ANDROID
		template<typename CharType, typename TraitsType = std::char_traits<CharType> >
		class VectorStream : public std::basic_streambuf<CharType, TraitsType>
		{
		public:
			VectorStream(std::vector<CharType> &vec)
			{
				std::basic_streambuf<CharType, TraitsType>::setg(vec.data(), vec.data(), vec.data() + vec.size());
			}
		};


		class PicoMaterialReader : public tinyobj::MaterialReader
		{
		public:
			typedef std::function<void(std::vector<char>&, const std::string&)> FileReaderFn;

			PicoMaterialReader(FileReaderFn fileReader)
				:m_fileReaderFn(fileReader)
			{
			}

			std::string operator()(const std::string &matId,
				std::vector<tinyobj::material_t> &materials,
				std::map<std::string, int> &matMap) override
			{
				std::vector<char> buffer;
				m_fileReaderFn(buffer, matId);
				VectorStream<char> vectorBuffer(buffer);
				std::istream matIStream(&vectorBuffer);

				std::string err = LoadMtl(matMap, materials, matIStream);
				if (!matIStream)
				{
					std::stringstream ss;
					ss << "WARN: Material file [ " << matId << " ] not found. Created a default material.";
					err += ss.str();
				}
				return err;
			}
		private:
			FileReaderFn m_fileReaderFn;
		};
#endif
		//-------------------------------------------------------------------------------------------------

		class Mesh : public IMesh
		{
		public:
			Mesh(SystemSpecificData data)
				:m_systemData(data)
			{
			}

			template <int N>
			void Render(const IGameObject::RenderDetails& renderDetails
				, const float(&rotationMatrix)[N][N]
				, const float(&scaleMatrix)[N][N]
				, const float(&translationMatrix)[N][N]
				, const bool lighting
				, const bool skipViewMatrix
				, const float alpha) const
			{
				float matrix[4][4];
				float model[4][4];
				float temp[4][4];
				MatrixTools::MatrixMul(rotationMatrix, scaleMatrix, temp);
				MatrixTools::MatrixMul(translationMatrix, temp, model);
				glUniformMatrix4fv(renderDetails.m_modelLoc, 1, GL_FALSE, static_cast<const GLfloat*>(&model[0][0]));
				if (skipViewMatrix)
				{
					MatrixTools::MatrixMul(renderDetails.m_perspectiveMatrix, model, matrix);
				}
				else
				{
					MatrixTools::MatrixMul(renderDetails.m_viewMatrix, model, temp);
					MatrixTools::MatrixMul(renderDetails.m_perspectiveMatrix, temp, matrix);
				}

				glUniformMatrix4fv(renderDetails.m_mvpLoc, 1, GL_FALSE, static_cast<const GLfloat*>(&matrix[0][0]));
				glUniform1i(renderDetails.m_useLightingLocation, lighting);
				glUniform1f(renderDetails.m_alphaLocation, alpha);
				glEnableVertexAttribArray(renderDetails.m_positionLoc);
				glEnableVertexAttribArray(renderDetails.m_texCoordLoc);
				glUniform1i(renderDetails.m_samplerLoc, 0);
				const MaterialId2TextureIdMap::const_iterator& endIter = std::end(m_materialId2TextureId);

				int lastestMaterialId = -1;
#ifdef USE_ARRAY_BUFFERS
				const auto shapeCount = m_vertexDataBuffer.size();
#else
				const auto shapeCount = m_vertexData.size();
#endif
				for (size_t shapeNo = 0; shapeNo != shapeCount; ++shapeNo)
				{
#ifdef USE_ARRAY_BUFFERS
					glBindBuffer(GL_ARRAY_BUFFER, m_vertexDataBuffer[shapeNo]);
					glVertexAttribPointer(renderDetails.m_positionLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
					glBindBuffer(GL_ARRAY_BUFFER, m_textureCoordDataBuffer[shapeNo]);
					glVertexAttribPointer(renderDetails.m_texCoordLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
					const auto& indexData = m_indexDataBuffers[shapeNo];
#else
					glVertexAttribPointer(renderDetails.m_positionLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), &m_vertexData[shapeNo][0]);
					glVertexAttribPointer(renderDetails.m_texCoordLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), &m_textureCoordData[shapeNo][0]);
					const auto& indexData = m_indexData[shapeNo];
#endif


					for (const auto& iData : indexData)
					{
						const auto materialId = iData.first;
						const auto& indices = iData.second;
						if (lastestMaterialId != materialId)
						{
							lastestMaterialId = materialId;
							glActiveTexture(GL_TEXTURE0);
							const auto& textureIter = m_materialId2TextureId.find(materialId);
							if (textureIter != endIter)
								glBindTexture(GL_TEXTURE_2D, textureIter->second);
						}

#ifdef USE_ARRAY_BUFFERS
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices.second);
						glDrawElements(GL_TRIANGLES, indices.first, GL_UNSIGNED_SHORT, nullptr);
#else
						glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0]);
#endif
					}
				}

#ifdef USE_ARRAY_BUFFERS
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
			}


			bool Load(const std::string& meshName)
			{
				std::vector<tinyobj::shape_t> shapes;
				std::vector<tinyobj::material_t> materials;
#ifdef PICO_ANDROID
				std::vector<char> buffer;
				LoadFile<char>(buffer, meshName);
				VectorStream<char> vectorBuffer(buffer);
				std::istream objIStream(&vectorBuffer);
				PicoMaterialReader::FileReaderFn fn([this](std::vector<char>& buffer, const std::string& filename) { LoadFile<char>(buffer, filename); });
				PicoMaterialReader materialReader(fn);
				std::string err = tinyobj::LoadObj(shapes, materials, objIStream, materialReader);
#else
				std::string err = tinyobj::LoadObj(shapes, materials, meshName.c_str());
#endif
				if (!err.empty())
					throw EngineException(("Failed to load objfile:" + err).c_str());

#ifdef USE_ARRAY_BUFFERS
				//These are local when using array buffers
				FloatSeries m_vertexData;
				FloatSeries m_textureCoordData;
				MaterialId2IndexDataMap m_indexData;
#endif
				m_vertexData.clear();
				m_textureCoordData.clear();

				MateralId2TextureNameMap textureNames;
				int i = 0;
				for (const auto& material : materials)
					textureNames[i++] = material.diffuse_texname;

				const auto shapeCount = shapes.size();
				m_vertexData.resize(shapeCount);
				m_textureCoordData.resize(shapeCount);
				m_indexData.resize(shapeCount);

				int shapeCnt = 0;
				for (const auto& shape : shapes)
				{
					const auto& mesh = shape.mesh;
					auto& vertexData = m_vertexData[shapeCnt];
					auto& texCoordData = m_textureCoordData[shapeCnt];
					vertexData.insert(std::end(vertexData), mesh.positions.begin(), mesh.positions.end());
					texCoordData.insert(std::end(texCoordData), mesh.texcoords.begin(), mesh.texcoords.end());

					auto& indices = m_indexData[shapeCnt];
					const auto faceCount = mesh.indices.size() / 3;
					for (size_t f = 0; f != faceCount; ++f)
					{
						auto& indexData = indices[mesh.material_ids[f]];
						for (auto i = 0; i != 3; ++i)
							indexData.push_back(mesh.indices[i + f * 3]);
					}

					++shapeCnt;
				}

#ifdef USE_ARRAY_BUFFERS
				for (const auto& vData : m_vertexData)
				{
					GLuint buffer;
					glGenBuffers(1, &buffer);
					glBindBuffer(GL_ARRAY_BUFFER, buffer);
					glBufferData(GL_ARRAY_BUFFER, vData.size() * sizeof(GLfloat), &vData[0], GL_STATIC_DRAW);
					m_vertexDataBuffer.push_back(buffer);
				}

				for (const auto& tData : m_textureCoordData)
				{
					GLuint buffer;
					glGenBuffers(1, &buffer);
					glBindBuffer(GL_ARRAY_BUFFER, buffer);
					glBufferData(GL_ARRAY_BUFFER, tData.size() * sizeof(GLfloat), &tData[0], GL_STATIC_DRAW);
					m_textureCoordDataBuffer.push_back(buffer);
				}

				m_indexDataBuffers.resize(shapeCount);
				for (int shapeCnt = 0; shapeCnt != shapes.size(); ++shapeCnt)
				{
					auto& indices = m_indexData[shapeCnt];
					for (const auto& indexData : indices)
					{
						auto& indexBuffers = m_indexDataBuffers[shapeCnt][indexData.first];
						const auto& theData = indices[indexData.first];
						glGenBuffers(1, &indexBuffers.second);
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffers.second);
						glBufferData(GL_ELEMENT_ARRAY_BUFFER, theData.size() * sizeof(GLushort), &theData[0], GL_STATIC_DRAW);
						indexBuffers.first = theData.size();
					}
				}
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
				m_materialId2TextureId = LoadTextures(textureNames);
				return true;
			}

		private:
			typedef std::vector<std::vector<GLfloat>> FloatSeries;
			typedef std::vector<std::map<int, std::vector<GLushort>>> MaterialId2IndexDataMap;

#ifdef USE_ARRAY_BUFFERS
			std::vector<GLuint> m_vertexDataBuffer;
			std::vector<GLuint> m_textureCoordDataBuffer;
			std::vector<std::map<int, std::pair<int, GLuint>>> m_indexDataBuffers;
#else
			FloatSeries m_vertexData;
			FloatSeries m_textureCoordData;
			MaterialId2IndexDataMap m_indexData;
#endif

			typedef std::map<int, std::string> MateralId2TextureNameMap;
			typedef std::map<int, int> MaterialId2TextureIdMap;
			MaterialId2TextureIdMap m_materialId2TextureId;

			//Ripped out of picopng example with additional Android assetmanager support
			template<typename T>
			void LoadFile(std::vector<T>& buffer, const std::string& filename) //designed for loading files from hard disk in an std::vector
			{
#ifdef PICO_ANDROID
				LoadAssetFile(m_systemData, buffer, filename);
#else
				std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

				//get filesize
				std::streamsize size = 0;
				if (file.seekg(0, std::ios::end).good()) size = file.tellg();
				if (file.seekg(0, std::ios::beg).good()) size -= file.tellg();

				//read contents of the file into the vector
				if (size > 0)
				{
					buffer.resize((size_t)size);
					file.read((char*)(&buffer[0]), size);
				}
				else buffer.clear();
#endif
			}

			MaterialId2TextureIdMap LoadTextures(const MateralId2TextureNameMap& textureNames)
			{
				MaterialId2TextureIdMap materialId2TextureId;
				std::vector<GLuint> textureIds;
				textureIds.resize(textureNames.size());
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

				glGenTextures(textureIds.size(), &textureIds[0]);

				int i = 0;
				for (const auto& texture : textureNames)
				{
					auto materialId = texture.first;
					auto textureName = texture.second;

					std::vector<unsigned char> buffer, imageUpsideDown;
					LoadFile<unsigned char>(buffer, textureName);
					unsigned long width, height;
					int error = pico::decodePNG(imageUpsideDown, width, height, buffer.empty() ? 0 : &buffer[0], (unsigned long)buffer.size());
					if (error != 0)
						throw EngineException(("Failed to decode texture:" + textureName).c_str());

					//Flipping the image on Y...
					std::vector<unsigned char> image;
					image.reserve(imageUpsideDown.size());

					for (unsigned long y = 0; y != height; ++y)
					{
						for (unsigned long x = 0; x != width * 4; ++x)
							image.push_back(imageUpsideDown[(height - 1 - y)*width * 4 + x]);
					}

					glBindTexture(GL_TEXTURE_2D, textureIds[i]);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

					materialId2TextureId[materialId] = textureIds[i++];
				}

				return std::move(materialId2TextureId);
			}

			SystemSpecificData m_systemData;
		};

		//-------------------------------------------------------------------------------------------------

		IGameObject::IGameObject(MeshPtr mesh)
			:m_lighting(true)
			, m_skipViewMatrix(false)
			, m_alpha(1.0f)
			, m_mesh(mesh)
			, m_gobId(s_gobId++)
			, m_pass(0)
		{
			MatrixTools::UpdateRotationMatrix(Vec3(), m_rotationMatrix);
			UpdateScaleMatrix(Vec3(1.0f, 1.0f, 1.0f));
			UpdateTranslationMatrix(Vec3());
		}

		IGameObject::~IGameObject()
		{
		}

		void IGameObject::SetPosition(const Vec3& pos)
		{
			m_position = UpdatePosition(pos);
			UpdateTranslationMatrix(m_position);
		}

		const Vec3& IGameObject::Position() const
		{
			return m_position;
		}

		void IGameObject::SetRotation(const Vec3& rot)
		{
			m_rotation = UpdateRotation(rot);
			MatrixTools::UpdateRotationMatrix(m_rotation, m_rotationMatrix);
		}

		const Vec3& IGameObject::Rotation() const
		{
			return m_rotation;
		}

		void IGameObject::SetScale(const Vec3& scale)
		{
			m_scale = UpdateScale(scale);
			UpdateScaleMatrix(m_scale);
		}

		const Vec3& IGameObject::Scale() const
		{
			return m_scale;
		}

		void IGameObject::UseLighting(const bool lighting)
		{
			m_lighting = lighting;
		}


		void IGameObject::SkipViewMatrix(const bool skipViewMatrix)
		{
			m_skipViewMatrix = skipViewMatrix;
		}

		void IGameObject::SetAlpha(const float alpha)
		{
			m_alpha = alpha;
		}

		bool IGameObject::Update(const double elapsedTime)
		{
			return UpdateState(elapsedTime);
		}

		void IGameObject::SetPass(const int pass)
		{
			m_pass = pass;
		}

		int IGameObject::Pass() const
		{
			return m_pass;
		}

		void IGameObject::Render(const double elapsedTime, const RenderDetails& renderDetails)
		{
			AdditionalRenderInstructionsBefore(elapsedTime);
#ifdef PICO_ANDROID
			auto m = (const Pico::Mesh*)&Mesh();//No rtti on android?
#else
			auto m = dynamic_cast<const Pico::Mesh*>(&Mesh());
#endif
			if (m != nullptr)//This only supports normal Meshes right now, no overloads...
				m->Render(renderDetails, m_rotationMatrix, m_scaleMatrix, m_translationMatrix, m_lighting, m_skipViewMatrix, m_alpha);
			AdditionalRenderInstructionsAfter(elapsedTime);
		}

		const IMesh& IGameObject::Mesh() const
		{
			return *m_mesh;
		}

		size_t IGameObject::Id() const
		{
			return m_gobId;
		}

		void IGameObject::UpdateTranslationMatrix(const Vec3& pos)
		{
			m_translationMatrix[0][0] = 1.0f; m_translationMatrix[0][1] = 0.0f; m_translationMatrix[0][2] = 0.0f; m_translationMatrix[0][3] = pos.X;
			m_translationMatrix[1][0] = 0.0f; m_translationMatrix[1][1] = 1.0f; m_translationMatrix[1][2] = 0.0f; m_translationMatrix[1][3] = pos.Y;
			m_translationMatrix[2][0] = 0.0f; m_translationMatrix[2][1] = 0.0f; m_translationMatrix[2][2] = 1.0f; m_translationMatrix[2][3] = pos.Z;
			m_translationMatrix[3][0] = 0.0f; m_translationMatrix[3][1] = 0.0f; m_translationMatrix[3][2] = 0.0f; m_translationMatrix[3][3] = 1.0f;
		}

		void IGameObject::UpdateScaleMatrix(const Vec3& scale)
		{
			m_scaleMatrix[0][0] = scale.X; m_scaleMatrix[0][1] = 0.0f;   m_scaleMatrix[0][2] = 0.0f;   m_scaleMatrix[0][3] = 0.0f;
			m_scaleMatrix[1][0] = 0.0f;   m_scaleMatrix[1][1] = scale.Y; m_scaleMatrix[1][2] = 0.0f;   m_scaleMatrix[1][3] = 0.0f;
			m_scaleMatrix[2][0] = 0.0f;   m_scaleMatrix[2][1] = 0.0f;   m_scaleMatrix[2][2] = scale.Z; m_scaleMatrix[2][3] = 0.0f;
			m_scaleMatrix[3][0] = 0.0f;   m_scaleMatrix[3][1] = 0.0f;   m_scaleMatrix[3][2] = 0.0f;   m_scaleMatrix[3][3] = 1.0f;
		}


		size_t IGameObject::s_gobId = 0;

		//-------------------------------------------------------------------------------------------------

		SimpleParticleObject::SimpleParticleObject(Pico::MeshPtr mesh)
			:IGameObject(mesh)
		{
			UseLighting(false);
		}

		SimpleParticleObject::~SimpleParticleObject()
		{
		}

		inline Pico::Vec3 SimpleParticleObject::UpdatePosition(const Pico::Vec3 & pos) { return pos; }

		inline bool SimpleParticleObject::UpdateState(const double elapsedTime) { return true; }

		void SimpleParticleObject::AdditionalRenderInstructionsBefore(const double elapsedTime)
		{
			//glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		void SimpleParticleObject::AdditionalRenderInstructionsAfter(const double elapsedTime)
		{
			//glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
		}

		//-------------------------------------------------------------------------------------------------

		SimpleGameObject::SimpleGameObject(Pico::MeshPtr mesh)
			:IGameObject(mesh)
		{
		}

		SimpleGameObject::~SimpleGameObject()
		{
		}

		inline Pico::Vec3 SimpleGameObject::UpdatePosition(const Pico::Vec3& pos) { return pos; }

		inline bool SimpleGameObject::UpdateState(const double elapsedTime) { return true; }

		//-------------------------------------------------------------------------------------------------
#ifdef SOUND_SUPPORT
		class SimpleSound : public ISound
		{
		public:
			SimpleSound(ALuint buffer)
				:ISound()
			{
				alGenSources(static_cast<ALuint>(1), &m_source);
				alSourcef(m_source, AL_PITCH, 1);
				alSourcef(m_source, AL_GAIN, 1);
				alSource3f(m_source, AL_POSITION, 0, 0, 0);
				alSource3f(m_source, AL_VELOCITY, 0, 0, 0);
				alSourcei(m_source, AL_LOOPING, AL_FALSE);
				alSourcei(m_source, AL_BUFFER, buffer);
			}

			virtual ~SimpleSound()
			{
				alDeleteSources(1, &m_source);
			}

			bool IsPlayingImpl() const override
			{
				ALint sourceState;
				alGetSourcei(m_source, AL_SOURCE_STATE, &sourceState);

				return sourceState == AL_PLAYING;
			}

			void PlayImpl(const bool loop) override
			{
				alSourcei(m_source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
				alSourcePlay(m_source);
			}

			void PauseImpl() override
			{
				alSourcePause(m_source);
			}

			void StopImpl() override
			{
				alSourceStop(m_source);
			}
		private:
			ALuint m_source;
		};
#endif
		//-------------------------------------------------------------------------------------------------

		struct Engine::Impl
		{
			IGameObject::RenderDetails m_renderDetails;
			float m_light[3];
			GLuint m_lightLocation;

			float m_viewTranslationMatrix[4][4];
			float m_viewRotationMatrix[4][4];

			const std::string m_windowName;
			size_t m_windowWidth;
			size_t m_windowHeight;
			const size_t m_swapInterval;
			SystemSpecificData m_systemData;

			NativeWindow m_window;
			EGLSupport m_egl;

			std::unordered_map<size_t, GameObjectPtr> m_objectsToAdd;
			std::vector<size_t> m_objectsToRemove;
			std::unordered_map<size_t, GameObjectPtr> m_gameObjects;

#ifdef PICO_PI
			int m_keyboardFd;
			std::unordered_set<char> m_keys;

			bool SetupKeyboard()
			{
				m_keyboardFd = -1;
				DIR *dirp;
				struct dirent *dp;
				regex_t kbd;

				char fullPath[1024];
				static char *dirName = "/dev/input/by-id";
				int result;

				if (regcomp(&kbd, "event-kbd", 0) != 0)
				{
					printf("regcomp for kbd failed\n");
					return false;
				}

				if ((dirp = opendir(dirName)) == NULL) {
					perror("couldn't open '/dev/input/by-id'");
					return false;
				}

				do {
					errno = 0;
					if ((dp = readdir(dirp)) != NULL)
					{
						printf("readdir (%s)\n", dp->d_name);
						if (regexec(&kbd, dp->d_name, 0, NULL, 0) == 0)
						{
							printf("match for the kbd = %s\n", dp->d_name);
							sprintf(fullPath, "%s/%s", dirName, dp->d_name);
							m_keyboardFd = open(fullPath, O_RDONLY | O_NONBLOCK);
							printf("%s Fd = %d\n", fullPath, m_keyboardFd);
							printf("Getting exclusive access: ");
							result = ioctl(m_keyboardFd, EVIOCGRAB, 1);
							printf("%s\n", (result == 0) ? "SUCCESS" : "FAILURE");
							break;
						}
					}
				} while (dp != NULL);

				closedir(dirp);
				regfree(&kbd);
				if ((m_keyboardFd == -1))
				{
					printf("Failed to obtain keyboard access");
					return false;
				}
				return true;
			}

			bool HasKey(char key) const
			{
				return m_keys.find(key) != std::end(m_keys);
			}

			void GetKeys()
			{
				input_event ev[64];
				auto rd = read(m_keyboardFd, ev, sizeof(ev));
				if (rd > 0)
				{
					const auto count = rd / sizeof(struct input_event);
					for (const auto& evp : ev)
					{
						if (evp.type == 1)
						{
							switch (evp.value)
							{
							case 1:
							case 2:
								m_keys.insert(evp.code);
								break;
							case 0:
								m_keys.erase(evp.code);
								break;
							default:
								break;
							}
						}

					}

				}
			}

#endif
			float m_perspectiveScaling;

			template<int N>
			void InitPerspective(float(&perspectiveMatrix)[N][N], float screenWidth, float screenHeight)
			{
				float zNear = 0.1f;
				float zFar = 1000.0f;

				const float ar = screenWidth / screenHeight;
				const float zRange = zNear - zFar;
				const float tanHalfFOV = tanf(1.04f / 2.0f);

				perspectiveMatrix[0][0] = m_perspectiveScaling / (tanHalfFOV * ar); perspectiveMatrix[0][1] = 0.0f;              perspectiveMatrix[0][2] = 0.0f;            perspectiveMatrix[0][3] = 0.0;
				perspectiveMatrix[1][0] = 0.0f;                     perspectiveMatrix[1][1] = m_perspectiveScaling / tanHalfFOV; perspectiveMatrix[1][2] = 0.0f;            perspectiveMatrix[1][3] = 0.0;
				perspectiveMatrix[2][0] = 0.0f;                     perspectiveMatrix[2][1] = 0.0f;			     perspectiveMatrix[2][2] = (-zNear - zFar) / zRange; perspectiveMatrix[2][3] = 2.0f*zFar*zNear / zRange;
				perspectiveMatrix[3][0] = 0.0f;                     perspectiveMatrix[3][1] = 0.0f;              perspectiveMatrix[3][2] = 1.0f;            perspectiveMatrix[3][3] = 0.0;
			}

			template<int N>
			void InitViewMatrix(float(&viewMatrix)[N][N])
			{
				viewMatrix[0][0] = 1.0f; viewMatrix[0][1] = 0.0f; viewMatrix[0][2] = 0.0f; viewMatrix[0][3] = 0.0f;
				viewMatrix[1][0] = 0.0f; viewMatrix[1][1] = 1.0f; viewMatrix[1][2] = 0.0f; viewMatrix[1][3] = 0.0f;
				viewMatrix[2][0] = 0.0f; viewMatrix[2][1] = 0.0f; viewMatrix[2][2] = 1.0f; viewMatrix[2][3] = 0.0f;
				viewMatrix[3][0] = 0.0f; viewMatrix[3][1] = 0.0f; viewMatrix[3][2] = 0.0f; viewMatrix[3][3] = 1.0f;
			}

			void UpdateViewMatrix()
			{
				MatrixTools::MatrixMul(m_viewRotationMatrix, m_viewTranslationMatrix, m_renderDetails.m_viewMatrix);
			}

			void UpdateViewMatrixTranslation(const Vec3& pos)
			{
				m_viewTranslationMatrix[0][3] = -pos.X;
				m_viewTranslationMatrix[1][3] = -pos.Y;
				m_viewTranslationMatrix[2][3] = -pos.Z;
				UpdateViewMatrix();
			}

			void UpdateViewMatrixRotation(const Vec3& rot)
			{
				MatrixTools::UpdateRotationMatrix(rot, m_viewRotationMatrix);
				UpdateViewMatrix();
			}

			Impl(const std::string & windowName, const size_t width, const size_t height, const size_t swapInterval,
				const std::string& vertexShader, const std::string& pixelShader, SystemSpecificData sData)
				:m_windowName(windowName)
				, m_windowWidth(width)
				, m_windowHeight(height)
				, m_swapInterval(swapInterval)
				, m_systemData(sData)
				, m_window(m_windowName, m_windowWidth, m_windowHeight, sData)
				, m_egl(m_window, m_swapInterval)
				, m_perspectiveScaling(1.0f)
#if defined(PICO_ANDROID) || defined(PICO_PI)
				, m_timeDiffForVsync(0.0)
#endif
			{
#ifdef SOUND_SUPPORT
				InitialiseSound();
#endif

#ifdef PICO_PI
				SetupKeyboard();
#endif
				InitPerspective(m_renderDetails.m_perspectiveMatrix, static_cast<float>(m_windowWidth), static_cast<float>(m_windowHeight));
				InitViewMatrix(m_viewTranslationMatrix);
				UpdateViewMatrixRotation(Vec3());

				m_window.ShowWindow(true);

				m_renderDetails.m_program = ShaderTools::CompileProgram(vertexShader, pixelShader);
				if (!m_renderDetails.m_program)
					throw EngineException("Failed to compile shaders!");

				m_renderDetails.m_positionLoc = glGetAttribLocation(m_renderDetails.m_program, "a_position");
				m_renderDetails.m_texCoordLoc = glGetAttribLocation(m_renderDetails.m_program, "a_texcoord");
				m_renderDetails.m_samplerLoc = glGetUniformLocation(m_renderDetails.m_program, "s_texture");
				m_renderDetails.m_mvpLoc = glGetUniformLocation(m_renderDetails.m_program, "mvp");
				m_renderDetails.m_modelLoc = glGetUniformLocation(m_renderDetails.m_program, "model");
				m_lightLocation = glGetUniformLocation(m_renderDetails.m_program, "g_light");
				m_renderDetails.m_useLightingLocation = glGetUniformLocation(m_renderDetails.m_program, "g_useLighting");
				m_renderDetails.m_alphaLocation = glGetUniformLocation(m_renderDetails.m_program, "g_alpha");

				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);
				glEnable(GL_DEPTH_TEST);
			}

			void HandleObjectMapUpdate()
			{
				for (const auto& objPair : m_objectsToAdd)
					m_gameObjects[objPair.first] = objPair.second;

				m_objectsToAdd.clear();

				for (const auto objId : m_objectsToRemove)
					m_gameObjects.erase(objId);

				m_objectsToRemove.clear();
			}

#if defined(PICO_ANDROID) || defined(PICO_PI)
			double m_timeDiffForVsync;
#endif
			bool Run(std::function<bool(double)> callBack)
			{
				TimeRetriever timer;
				std::map<int, std::vector<size_t>> objectsToRender;

				HandleObjectMapUpdate();

				auto isRunning = true;
				while (isRunning)
				{
					auto elapsedTime = timer.ElapsedTimeInSeconds();
#if defined(PICO_ANDROID) || defined(PICO_PI)
					//It seems a lot of android GL implementations don't handle eglSwapInterval properly... (including Ouya)
					auto diff = (elapsedTime - m_timeDiffForVsync) / 1000.0;
					if (diff < 0.033)//Let's try and hold the arbitrary rate of 30 fps
					{
						diff = 0.033 - diff;
						usleep(diff * 1000000.0);
					}
					elapsedTime = timer.ElapsedTimeInSeconds();
					m_timeDiffForVsync = elapsedTime;
#endif
					m_egl.RefreshWindowData();

					if (m_window.Width() != m_windowWidth || m_window.Height() != m_windowHeight)
						InitPerspective(m_renderDetails.m_perspectiveMatrix, static_cast<float>(m_window.Width()), static_cast<float>(m_window.Height()));

					glUseProgram(m_renderDetails.m_program);
					glViewport(0, 0, m_window.Width(), m_window.Height());
					m_windowWidth = m_window.Width();
					m_windowHeight = m_window.Height();

#ifdef PICO_PI
					GetKeys();
#endif
					isRunning = callBack(elapsedTime);

					if (isRunning)
					{
						HandleObjectMapUpdate();

						objectsToRender.clear();
						for (auto& gob : m_gameObjects)
						{
							if (gob.second->Update(elapsedTime))
								objectsToRender[gob.second->Pass()].push_back(gob.first);
						}

						glUniform3fv(m_lightLocation, 1, m_light);

						for (auto pass : objectsToRender)
						{
							for (auto gobId : pass.second)
								m_gameObjects[gobId]->Render(elapsedTime, m_renderDetails);
						}

						Event event;
						while (m_window.PopEvent(event))
						{
							if (event.m_type == Event::Type::WindowClosed)
								return false;
						}
					}
					else
					{
						HandleObjectMapUpdate();
						return true;
					}

					m_egl.SwapBuffer();

					m_window.MsgLoop();

					HandleObjectMapUpdate();

#ifdef OGG_PLAYBACK
					MainOggPlaybackLoop();
#endif
				}
				return false;
			}


			bool AddGameObject(GameObjectPtr gob)
			{
				m_objectsToAdd[gob->Id()] = gob;
				return true;
			}

			GameObjectPtr GameObject(const size_t id)
			{
				auto iter = m_gameObjects.find(id);
				if (iter == std::end(m_gameObjects))
					return GameObjectPtr();

				return iter->second;
			}

			bool RemoveGameObject(const size_t id)
			{
				auto iter = m_objectsToAdd.find(id);
				if (iter != std::end(m_objectsToAdd))
					m_objectsToAdd.erase(iter);

				if (m_gameObjects.find(id) == std::end(m_gameObjects))
					return false;

				m_objectsToRemove.push_back(id);
				//m_objectsToAdd.erase(id);
				return true;
			}

#ifdef SOUND_SUPPORT
			ALCdevice* m_alDevice;
			ALCcontext* m_alContext;
			std::unordered_map<std::string, ALuint> m_soundBuffers;
			std::vector<unsigned char> m_oggSoundBuffer;

#ifdef OGG_PLAYBACK
			ALuint m_oggPlaybackSource;
			bool m_isPlayingOgg;
			bool m_loopOgg;
			stb_vorbis* m_vorbisFileInfo;
			std::vector<short> m_oggShortBuffer;
			std::string m_oggFilename;

			int FillBuffer(ALuint buffer)
			{
				auto format = GetALFormat(m_vorbisFileInfo);
				auto samples = stb_vorbis_get_samples_short_interleaved(m_vorbisFileInfo, m_vorbisFileInfo->channels, &m_oggShortBuffer[0], m_oggShortBuffer.size());
				alBufferData(buffer, format, &m_oggShortBuffer[0], m_oggShortBuffer.size()*sizeof(ALshort), m_vorbisFileInfo->sample_rate);
				return samples;
			}

			void StartOggPlayBack()
			{
				auto buffer1 = GetSoundBuffer("oggbuffer1");
				auto buffer2 = GetSoundBuffer("oggbuffer2");
				auto buffer3 = GetSoundBuffer("oggbuffer3");

				int error;
#ifdef PICO_ANDROID
				LoadAssetFile(m_systemData, m_oggSoundBuffer, m_oggFilename);
				m_vorbisFileInfo = stb_vorbis_open_memory(&m_oggSoundBuffer[0], m_oggSoundBuffer.size(), &error, nullptr);
#else
				m_vorbisFileInfo = stb_vorbis_open_filename(m_oggFilename.c_str(), &error, nullptr);
#endif
				if (m_vorbisFileInfo == nullptr)
					throw EngineException("Failed to load ogg file:" + m_oggFilename);

				FillBuffer(buffer1);
				FillBuffer(buffer2);
				FillBuffer(buffer3);

				alSourceQueueBuffers(m_oggPlaybackSource, 1, &buffer1);
				alSourceQueueBuffers(m_oggPlaybackSource, 1, &buffer2);
				alSourceQueueBuffers(m_oggPlaybackSource, 1, &buffer3);

				int val = 0;
				alGetSourcei(m_oggPlaybackSource, AL_SOURCE_STATE, &val);
				if (val != AL_PLAYING)
					alSourcePlay(m_oggPlaybackSource);

				alSourcef(m_oggPlaybackSource, AL_GAIN, 0.2f);

				m_isPlayingOgg = true;
			}

			void MainOggPlaybackLoop()
			{
				if (m_isPlayingOgg)
				{
					int val = 0;
					alGetSourcei(m_oggPlaybackSource, AL_BUFFERS_PROCESSED, &val);
					if (val <= 0)
						return;

					ALuint buffer;
					alSourceUnqueueBuffers(m_oggPlaybackSource, 1, &buffer);

					auto samples = FillBuffer(buffer);
					alSourceQueueBuffers(m_oggPlaybackSource, 1, &buffer);

					if (samples == 0)
					{
						alSourceStop(m_oggPlaybackSource);
						m_isPlayingOgg = false;
						stb_vorbis_close(m_vorbisFileInfo);

						if (m_loopOgg)
							StartOggPlayBack();
					}
					else
					{
						alGetSourcei(m_oggPlaybackSource, AL_SOURCE_STATE, &val);
						if (val != AL_PLAYING)
							alSourcePlay(m_oggPlaybackSource);
					}
				}
			}

#endif
			void InitialiseSound()
			{
				m_alDevice = alcOpenDevice(nullptr);
				if (m_alDevice == nullptr)
					throw EngineException("Could not open alc device");

				m_alContext = alcCreateContext(m_alDevice, nullptr);
				if (!alcMakeContextCurrent(m_alContext))
					throw EngineException("Failed to create al context");


				ALfloat listenerOrientation[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
				alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
				alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
				alListenerfv(AL_ORIENTATION, listenerOrientation);
#ifdef OGG_PLAYBACK
				alGenSources(static_cast<ALuint>(1), &m_oggPlaybackSource);
				alSourcef(m_oggPlaybackSource, AL_PITCH, 1);
				alSourcef(m_oggPlaybackSource, AL_GAIN, 1);
				alSource3f(m_oggPlaybackSource, AL_POSITION, 0, 0, 0);
				alSource3f(m_oggPlaybackSource, AL_VELOCITY, 0, 0, 0);
				alSourcei(m_oggPlaybackSource, AL_LOOPING, AL_FALSE);
				m_isPlayingOgg = false;
				m_vorbisFileInfo = nullptr;
				m_oggShortBuffer.resize(65536);
				m_loopOgg = false;
#endif
			}

			void TeardownSound()
			{
				for (auto buffer : m_soundBuffers)
					alDeleteBuffers(1, &buffer.second);

				auto device = alcGetContextsDevice(m_alContext);
				alcMakeContextCurrent(nullptr);
				alcDestroyContext(m_alContext);
				if (device != m_alDevice) //How would this really happen?
					alcCloseDevice(m_alDevice);

				alcCloseDevice(device);
			}


			struct WAVData
			{
				std::vector<char> Data;
				unsigned short AudioFormat;
				unsigned short NumberOfChannels;
				unsigned long SampleRate;
				unsigned short BitsPerSample;
			};

#ifdef PICO_ANDROID
			template<typename CharT, typename TraitsT = std::char_traits<CharT> >
			class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT>
			{
			public:
				vectorwrapbuf(std::vector<CharT> &vec)
				{
					std::streambuf::setg(vec.data(), vec.data(), vec.data() + vec.size());
				}
			};
#endif
			WAVData LoadWAVFile(const std::string& filename)
			{
				WAVData waveData;
#ifdef PICO_ANDROID
				std::vector<char> buffer;
				LoadAssetFile(m_systemData, buffer, filename);
				vectorwrapbuf<char> databuf(buffer);
				std::istream waveFile(&databuf);
#else

				std::ifstream waveFile(filename, std::ios::in | std::ios::binary);
				if (!waveFile.is_open())
					throw EngineException("Could not open wave file:" + filename);
#endif
				unsigned int idHolder = 0;
				waveFile.read(reinterpret_cast<char*>(&idHolder), 4);
				if (idHolder != 'FFIR')
					throw EngineException("Wave file has no RIFF header");

				unsigned int dataSize = 0;
				waveFile.read(reinterpret_cast<char*>(&dataSize), 4);

				waveFile.read(reinterpret_cast<char*>(&idHolder), 4);
				if (idHolder != 'EVAW')
					throw EngineException("Wave file is not WAVE format");

				waveFile.read(reinterpret_cast<char*>(&idHolder), 4);
				if (idHolder != ' tmf')
					throw EngineException("Wave file is missing format info");

				waveFile.read(reinterpret_cast<char*>(&dataSize), 4);
				if (dataSize != 16)
					throw EngineException("This reader only supports PCM files");

				waveFile.read(reinterpret_cast<char*>(&waveData.AudioFormat), 2);
				waveFile.read(reinterpret_cast<char*>(&waveData.NumberOfChannels), 2);
				waveFile.read(reinterpret_cast<char*>(&waveData.SampleRate), 4);
				waveFile.read(reinterpret_cast<char*>(&dataSize), 4);//ByteRate
				unsigned short blockAlign;
				waveFile.read(reinterpret_cast<char*>(&blockAlign), 2);
				waveFile.read(reinterpret_cast<char*>(&waveData.BitsPerSample), 2);

				//Assuming PCM so there's no check for extra params

				waveFile.read(reinterpret_cast<char*>(&idHolder), 4);
				if (idHolder != 'atad')
					throw EngineException("Wave file is missing data chunk");

				waveFile.read(reinterpret_cast<char*>(&dataSize), 4);
				waveData.Data = std::vector<char>(dataSize);
				waveFile.read(&waveData.Data[0], dataSize);

				return waveData;
			}

#ifdef OGG_PLAYBACK
			unsigned int GetALFormat(stb_vorbis* vorbisInfo)
			{
				switch (vorbisInfo->channels)
				{
				case 1:
					return AL_FORMAT_MONO16;
				case 2:
					return AL_FORMAT_STEREO16;
					break;
				default:
					throw EngineException("Wave support is only for up to 2 channels");
				}
				return AL_FORMAT_STEREO16;//This should be unreachable
			}
#endif

			unsigned int GetALFormat(WAVData waveData)
			{
				switch (waveData.NumberOfChannels)
				{
				case 1:
					switch (waveData.BitsPerSample)
					{
					case 8:
						return AL_FORMAT_MONO8;
					case 16:
						return AL_FORMAT_MONO16;
					default:
						throw EngineException("Wave support is only for 8 or 16 bit mono samples");
					}
					break;
				case 2:
					switch (waveData.BitsPerSample)
					{
					case 8:
						return AL_FORMAT_STEREO8;
					case 16:
						return AL_FORMAT_STEREO16;
					default:
						throw EngineException("Wave support is only for 8 or 16 bit stereo samples");
					}
					break;
				default:
					throw EngineException("Wave support is only for up to 2 channels");
				}
				return AL_FORMAT_STEREO16;//This should be unreachable
			}

			ALuint GetSoundBuffer(const std::string& filename)
			{
				auto iter = m_soundBuffers.find(filename);
				if (iter != std::end(m_soundBuffers))
					return iter->second;

				ALuint buffer;
				alGenBuffers(static_cast<ALuint>(1), &buffer);
				return buffer;
			}

			ALuint GetSoundBufferForWAV(const std::string& filename)
			{
				auto buffer = GetSoundBuffer(filename);
				const auto waveData = LoadWAVFile(filename);
				const auto format = GetALFormat(waveData);

				alBufferData(buffer, format, &waveData.Data[0], waveData.Data.size(), waveData.SampleRate);
				m_soundBuffers[filename] = buffer;
				return buffer;
			}

			ISoundPtr CreateSoundFromWAV(const std::string& filename)
			{
				auto buffer = GetSoundBufferForWAV(filename);
				return std::shared_ptr<ISound>(new Pico::SimpleSound(buffer));
			}
#endif
			~Impl()
			{
#ifdef SOUND_SUPPORT
				TeardownSound();
#endif
			}
		};

		//-------------------------------------------------------------------------------------------------

		Engine::Engine(const std::string & windowName, const size_t width, const size_t height, const size_t swapInterval,
			const std::string& vertexShader, const std::string& pixelShader, SystemSpecificData sData)
			:m_impl(/*std::make_unique<Impl>*/new Impl(windowName, width, height, swapInterval, vertexShader, pixelShader, sData))//Android C++11 support is a bit lacking in Visual studio
		{
		}

		bool Engine::Run(std::function<bool(double)> callBack)
		{
			if (!m_impl)
				return false;

			return m_impl->Run(callBack);
		}

		bool Engine::AddGameObject(GameObjectPtr gob)
		{
			if (!m_impl)
				return false;

			return m_impl->AddGameObject(gob);
		}

		GameObjectPtr Engine::GameObject(const size_t id)
		{
			if (!m_impl)
				return GameObjectPtr();

			return m_impl->GameObject(id);
		}

		bool Engine::RemoveGameObject(const size_t id)
		{
			if (!m_impl)
				return false;

			return m_impl->RemoveGameObject(id);
		}

		void Engine::SetCameraPos(const Vec3 & pos)
		{
			if (!m_impl)
				return;

			m_impl->UpdateViewMatrixTranslation(pos);
		}

		void Engine::SetCameraRotation(const Vec3& rot)
		{
			if (!m_impl)
				return;

			m_impl->UpdateViewMatrixRotation(Vec3(-rot.X, -rot.Y, -rot.Z));
		}

		MeshPtr Engine::LoadMesh(const std::string& meshName)
		{
			auto mesh = std::make_shared<Mesh>(m_impl->m_systemData);
			mesh->Load(meshName);
			return mesh;
		}

		bool Engine::IsKeyDown(char key) const
		{
#ifdef PICO_WINDOWS
			return ::GetAsyncKeyState(key) != 0;
#endif
#ifdef PICO_UNIVERSAL
			return m_impl->m_systemData.WindowWrapper->GetAsyncKeyState(key);
#endif
#ifdef PICO_ANDROID
			return m_impl->m_systemData.m_androidWrapper->GetAsyncKeyState(key);
#endif
#ifdef PICO_PI
			return m_impl->HasKey(key);
#endif
		}

#ifdef SOUND_SUPPORT
		ISoundPtr Engine::GetSound(const std::string& filename)
		{
			return m_impl->CreateSoundFromWAV(filename);
		}
#endif
#ifdef OGG_PLAYBACK
		void Engine::PlayOggMusic(const std::string& filename, const bool looping)
		{
			StopOggMusic();
			m_impl->m_oggFilename = filename;
			m_impl->m_loopOgg = looping;
			m_impl->StartOggPlayBack();
		}

		void Engine::StopOggMusic()
		{
			alSourceStop(m_impl->m_oggPlaybackSource);
			stb_vorbis_close(m_impl->m_vorbisFileInfo);
		}

		void Engine::PauseOggMusic()
		{
			alSourcePause(m_impl->m_oggPlaybackSource);
		}
#endif

		void Engine::SetLight(const Vec3& pos)
		{
			if (!m_impl)
				return;

			m_impl->m_light[0] = pos.X;
			m_impl->m_light[1] = pos.Y;
			m_impl->m_light[2] = pos.Z;
		}

		void Engine::SetPerspectiveScaling(const float perspectiveScaling)
		{
			m_impl->m_perspectiveScaling = perspectiveScaling;
			m_impl->InitPerspective(m_impl->m_renderDetails.m_perspectiveMatrix, static_cast<float>(m_impl->m_window.Width()), static_cast<float>(m_impl->m_window.Height()));
		}

		Engine::~Engine()
		{
		}

		//-------------------------------------------------------------------------------------------------

#ifdef SOUND_SUPPORT
		ISound::ISound()
			:m_id(s_id++)
		{
		}

		ISound::~ISound()
		{
		}
		void ISound::Play(const bool loop)
		{
			PlayImpl(loop);
		}

		bool ISound::IsPlaying() const
		{
			return IsPlayingImpl();
		}

		void ISound::Pause()
		{
			PauseImpl();
		}
		void ISound::Stop()
		{
			StopImpl();
		}

		int ISound::Id() const
		{
			return m_id;
		}

		int ISound::s_id = 0;

#endif
		//-------------------------------------------------------------------------------------------------

	}
}
