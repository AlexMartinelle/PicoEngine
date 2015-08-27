#include "picoengine.h"

#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "picopng.h"
#include "tiny_obj_loader.h"

#include <cmath>
#include <fstream>
#include <unordered_map>
#include <deque>
#include <sstream>

#ifdef PICO_ANDROID
#include <time.h>
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
				,m_shader(shader)
				,m_shaderType(type)
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

				WNDCLASSEXA wndClass = {0};
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
				return static_cast<double>(::GetTickCount64()) - static_cast<double>(m_startTick) / 1000.0;
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
				,m_config(nullptr)
				,m_display(EGL_NO_DISPLAY)
				,m_surface(EGL_NO_SURFACE)
				,m_context(EGL_NO_CONTEXT)
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
				:m_app(sData.m_app)
				,m_name(windowName)
				,m_width(windowWidth)
				,m_height(windowHeight)
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
				return m_app->window;
			}

			EGLNativeDisplayType Display() const
			{
				return m_display;
			}

			void MsgLoop()
			{
				int ident;
				int events;
				struct android_poll_source* source;

				while ((ident = ALooper_pollAll(0, NULL, &events,
					(void**)&source)) >= 0) {

					if (source != NULL) {
						source->process(m_app, source);
					}

					if (m_app->destroyRequested != 0)
					{
						PushEvent(Event(Event::Type::WindowClosed));
						break;
					}
				}
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
			android_app* m_app;
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
					eglSwapInterval(m_display, m_swapInterval);
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
				m_startTick = t.tv_nsec;
			}

			double ElapsedTimeInSeconds() const
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);

				return static_cast<double>(t.tv_nsec) - static_cast<double>(m_startTick) / 1000000000.0;
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
				,m_width(windowWidth)
				,m_height(windowHeight)
			{
			    static bool oneTimeCraziness = true;
			    if (oneTimeCraziness)
			    {
			        oneTimeCraziness = false;
			        bcm_host_init();
			    }

               VC_RECT_T dst_rect;
               VC_RECT_T src_rect;

               uint32_t width = 640;
               uint32_t height = 480;
               
               auto success = graphics_get_display_size(0 /* LCD */,
                                    &width, &height);
               if ( success < 0 )
                    throw EngineException("Failed to get display");

               dst_rect.x = 0;
               dst_rect.y = 0;
               dst_rect.width = windowWidth;
               dst_rect.height = windowHeight;

               src_rect.x = 0;
               src_rect.y = 0;
               src_rect.width = windowWidth << 16;
               src_rect.height = windowHeight << 16;

               DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
               DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start( 0 );

               DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add ( dispman_update,
                  dispman_display, 0/*layer*/, &dst_rect, 0/*src*/,
                  &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/,
                  0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

               s_nativewindow.element = dispman_element;
               s_nativewindow.width = windowWidth;
               s_nativewindow.height = windowHeight;
               vc_dispmanx_update_submit_sync( dispman_update );
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
				m_startTick = t.tv_nsec;
			}

			double ElapsedTimeInSeconds() const
			{
				timespec t;
				clock_gettime(CLOCK_MONOTONIC, &t);

				return static_cast<double>(t.tv_nsec) - static_cast<double>(m_startTick) / 1000000000.0;
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
			float m_viewMatrix[4][4];
			float m_perspectiveMatrix[4][4];

			RenderDetails()
				:m_program(0)
				, m_positionLoc(0)
				, m_texCoordLoc(0)
				, m_samplerLoc(0)
				, m_mvpLoc(0)
				, m_modelLoc(0)
				, m_useLightingLocation(true)
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
				, const bool lighting) const
			{
				float matrix[4][4];
				float model[4][4];
				float temp[4][4];
				MatrixTools::MatrixMul(rotationMatrix, scaleMatrix, temp);
				MatrixTools::MatrixMul(translationMatrix, temp, model);
				glUniformMatrix4fv(renderDetails.m_modelLoc, 1, GL_FALSE, static_cast<const GLfloat*>(&model[0][0]));
				MatrixTools::MatrixMul(renderDetails.m_viewMatrix, model, temp);
				MatrixTools::MatrixMul(renderDetails.m_perspectiveMatrix, temp, matrix);
			
				glUniformMatrix4fv(renderDetails.m_mvpLoc, 1, GL_FALSE, static_cast<const GLfloat*>(&matrix[0][0]));
				glUniform1i(renderDetails.m_useLightingLocation, lighting);
				glEnableVertexAttribArray(renderDetails.m_positionLoc);
				glEnableVertexAttribArray(renderDetails.m_texCoordLoc);
				glUniform1i(renderDetails.m_samplerLoc, 0);

				const auto shapeCount = m_vertexData.size();
				for (size_t shapeNo = 0; shapeNo != shapeCount; ++shapeNo)
				{
					glVertexAttribPointer(renderDetails.m_positionLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), &m_vertexData[shapeNo][0]);
					glVertexAttribPointer(renderDetails.m_texCoordLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), &m_textureCoordData[shapeNo][0]);
					const MaterialId2TextureIdMap::const_iterator& endIter = std::end(m_materialId2TextureId);
					const auto& indexData = m_indexData[shapeNo];
					for (const auto& iData : indexData)
					{
						const auto materialId = iData.first;
						const auto& indices = iData.second;
						glActiveTexture(GL_TEXTURE0);
						const auto& textureIter = m_materialId2TextureId.find(materialId);
						if (textureIter != endIter)
							glBindTexture(GL_TEXTURE_2D, textureIter->second);

						glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0]);
					}
				}
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
				m_materialId2TextureId = LoadTextures(textureNames);
				return true;
			}

		private:
			typedef std::vector<std::vector<GLfloat>> FloatSeries;
			typedef std::vector<std::map<int, std::vector<GLushort>>> MaterialId2IndexDataMap;
			FloatSeries m_vertexData;
			FloatSeries m_textureCoordData;
			MaterialId2IndexDataMap m_indexData;
			typedef std::map<int, std::string> MateralId2TextureNameMap;
			typedef std::map<int, int> MaterialId2TextureIdMap;
			MaterialId2TextureIdMap m_materialId2TextureId;

			//Ripped out of picopng example with additional Android assetmanager support
			template<typename T>
			void LoadFile(std::vector<T>& buffer, const std::string& filename) //designed for loading files from hard disk in an std::vector
			{
#ifdef PICO_ANDROID
				AAssetManager* assetManager = m_systemData.m_app->activity->assetManager;
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
#else
			  std::ifstream file(filename.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		
			  //get filesize
			  std::streamsize size = 0;
			  if(file.seekg(0, std::ios::end).good()) size = file.tellg();
			  if(file.seekg(0, std::ios::beg).good()) size -= file.tellg();
		
			  //read contents of the file into the vector
			  if(size > 0)
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

				glGenTextures(1, &textureIds[0]);

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
					for (unsigned long y = 0; y != height; ++y)
					{
						for (unsigned long x = 0; x != width*4; ++x)
							image.push_back(imageUpsideDown[(height - 1 - y)*width*4+ x]);
					}

					glBindTexture(GL_TEXTURE_2D, textureIds[i]);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

				
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);
				
					materialId2TextureId[materialId] = textureIds[i++];
				}

				return std::move(materialId2TextureId);
			}

			SystemSpecificData m_systemData;
		};

		//-------------------------------------------------------------------------------------------------

		IGameObject::IGameObject(MeshPtr mesh)
			:m_lighting(true)
			,m_mesh(mesh)
			,m_gobId(s_gobId++)
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

		bool IGameObject::Update(const double elapsedTime)
		{
			return UpdateState(elapsedTime);
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
				m->Render(renderDetails, m_rotationMatrix, m_scaleMatrix, m_translationMatrix, m_lighting);
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
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		}
		void SimpleParticleObject::AdditionalRenderInstructionsAfter(const double elapsedTime)
		{
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

		struct Engine::Impl
		{
			IGameObject::RenderDetails m_renderDetails;
			float m_light[3];
			GLuint m_lightLocation;

			float m_viewTranslationMatrix[4][4];
			float m_viewRotationMatrix[4][4];

			const std::string m_windowName;
			const size_t m_windowWidth;
			const size_t m_windowHeight;
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

				if(regcomp(&kbd,"event-kbd",0)!=0)
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
						printf("readdir (%s)\n",dp->d_name);
						if(regexec (&kbd, dp->d_name, 0, NULL, 0) == 0)
						{
							printf("match for the kbd = %s\n",dp->d_name);
							sprintf(fullPath,"%s/%s",dirName,dp->d_name);
							m_keyboardFd = open(fullPath,O_RDONLY | O_NONBLOCK);
							printf("%s Fd = %d\n",fullPath,m_keyboardFd);
							printf("Getting exclusive access: ");
							result = ioctl(m_keyboardFd, EVIOCGRAB, 1);
							printf("%s\n", (result == 0) ? "SUCCESS" : "FAILURE");
							break;
						}
					}
					}while (dp != NULL);

				closedir(dirp);
				regfree(&kbd);
				if((m_keyboardFd == -1))
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
				m_keys.clear();
				auto rd = read(m_keyboardFd,ev,sizeof(ev));
				if(rd > 0)
				{
					const auto count = rd / sizeof(struct input_event);
					for (const auto& evp : ev)
					{
						const auto evp& = ev[n++];
						if(evp.type == 1 && (evp.value == 1 || evp.value == 2))
							m_keys.insert(evp.code);
					}

				}
			}

#endif
			template<int N>
			void InitPerspective(float(&perspectiveMatrix)[N][N])
			{
				float zNear = 0.1f;
				float zFar = 100.0f;
				const float ar = 640.0f / 480.0f;
				const float zRange = zNear - zFar;
				const float tanHalfFOV = tanf(1.04f / 2.0f); //pi/3/2
			
				perspectiveMatrix[0][0] = 1.0f / (tanHalfFOV * ar); perspectiveMatrix[0][1] = 0.0f;              perspectiveMatrix[0][2] = 0.0f;            perspectiveMatrix[0][3] = 0.0;
				perspectiveMatrix[1][0] = 0.0f;                     perspectiveMatrix[1][1] = 1.0f / tanHalfFOV; perspectiveMatrix[1][2] = 0.0f;            perspectiveMatrix[1][3] = 0.0;
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
				,m_windowWidth(width)
				,m_windowHeight(height)
				,m_swapInterval(swapInterval)
				,m_systemData(sData)
				,m_window(m_windowName, m_windowWidth, m_windowHeight, sData)
				,m_egl(m_window, m_swapInterval)
			{
#ifdef PICO_PI
                SetupKeyboard();
#endif
				InitPerspective(m_renderDetails.m_perspectiveMatrix);
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

				glDisable(GL_CULL_FACE);
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

			bool Run(std::function<bool(double)> callBack)
			{
				TimeRetriever timer;
				std::vector<size_t> objectsToRender;

				HandleObjectMapUpdate();

				auto isRunning = true;
				while (isRunning)
				{
					auto elapsedTime = timer.ElapsedTimeInSeconds();
					glUseProgram(m_renderDetails.m_program);
					glViewport(0, 0, m_window.Width(), m_window.Height());

#ifdef PICO_PI
                    GetKeys();
#endif
					isRunning = callBack(elapsedTime);
				
					if (isRunning)
					{
						objectsToRender.clear();
						for (auto& gob : m_gameObjects)
						{
							if (gob.second->Update(elapsedTime))
								objectsToRender.push_back(gob.first);
						}

						glUniform3fv(m_lightLocation, 1, m_light);

						for (auto gobId : objectsToRender)
							m_gameObjects[gobId]->Render(elapsedTime, m_renderDetails);

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
				}
				return false;
			}


			bool AddGameObject(GameObjectPtr gob)
			{
				m_objectsToAdd[gob->Id()] = gob;
				return true;
			}

			bool RemoveGameObject(const size_t id)
			{
				if (m_gameObjects.find(id) == std::end(m_gameObjects))
					return false;

				m_objectsToRemove.push_back(id);
				m_objectsToAdd.erase(id);
				return true;
			}

		};

		//-------------------------------------------------------------------------------------------------
	
		Engine::Engine(const std::string & windowName, const size_t width, const size_t height, const size_t swapInterval,
					   const std::string& vertexShader, const std::string& pixelShader, SystemSpecificData sData)
			:m_impl(/*std::make_unique<Impl>*/new Impl(windowName, width, height, swapInterval, vertexShader, pixelShader, sData))//Android C++11 support is a bit lacking in Visual studio		
		{
			SetLight(Vec3(0.0, 1.0f, 0.0));
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

		MeshPtr Engine::LoadMesh(const std::string & meshName)
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
#ifdef PICO_ANDROID
			return false;//Skipping keyboard support for android
#endif
#ifdef PICO_PI
            return m_impl->HasKey(key);
#endif
		}

		void Engine::SetLight(const Vec3& pos)
		{
			if (!m_impl)
				return;

			m_impl->m_light[0] = pos.X;
			m_impl->m_light[1] = pos.Y;
			m_impl->m_light[2] = pos.Z;
		}

		Engine::~Engine()
		{
		}

	}
}