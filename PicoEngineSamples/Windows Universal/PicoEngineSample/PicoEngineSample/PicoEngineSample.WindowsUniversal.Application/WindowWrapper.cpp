#include "pch.h"
#include "WindowWrapper.h"
using namespace Windows::UI::Core;
using namespace Windows::Foundation::Collections;
using namespace Platform;

namespace SampleApp
{
	WindowWrapper::WindowWrapper(Windows::UI::Core::CoreWindow^ wnd)
		:m_wnd(wnd)
		, m_windowClosed(false)
		, m_windowVisible(true)
		, m_showBackButton(false)
	{
		if (m_wnd == nullptr)
			m_wnd = Windows::UI::Core::CoreWindow::GetForCurrentThread();
	}


	EGLNativeWindowType WindowWrapper::NativeWindowType()
	{
		// Create a PropertySet and initialize with the EGLNativeWindowType.
		//PropertySet^ surfaceCreationProperties = ref new PropertySet();
		//surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), window.Window());

		// You can configure the surface to render at a lower resolution and be scaled up to 
		// the full window size. The scaling is often free on mobile hardware.
		//
		// One way to configure the SwapChainPanel is to specify precisely which resolution it should render at.
		// Size customRenderSurfaceSize = Size(800, 600);
		// surfaceCreationProperties->Insert(ref new String(EGLRenderSurfaceSizeProperty), PropertyValue::CreateSize(customRenderSurfaceSize));
		//
		// Another way is to tell the SwapChainPanel to render at a certain scale factor compared to its size.
		// e.g. if the SwapChainPanel is 1920x1280 then setting a factor of 0.5f will make the app render at 960x640
		// float customResolutionScale = 0.5f;
		// surfaceCreationProperties->Insert(ref new String(EGLRenderResolutionScaleProperty), PropertyValue::CreateSingle(customResolutionScale));


		// Create a PropertySet and initialize with the EGLNativeWindowType.
		PropertySet^ surfaceCreationProperties = ref new PropertySet();
		surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), m_wnd);
		return reinterpret_cast<IInspectable*>(m_wnd);
	}

	// Window event handlers.
	void WindowWrapper::OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
	{
		m_windowVisible = args->Visible;
	}

	Windows::System::VirtualKey WindowWrapper::GetVirtualKey(char rk)
	{
		switch (rk)
		{
		case 'a':
			return Windows::System::VirtualKey::A;
		case 'b':
			return Windows::System::VirtualKey::B;
		case 'c':
			return Windows::System::VirtualKey::C;
		case 'd':
			return Windows::System::VirtualKey::D;
		case 'e':
			return Windows::System::VirtualKey::E;
		case 'f':
			return Windows::System::VirtualKey::F;
		case 'g':
			return Windows::System::VirtualKey::G;
		case 'h':
			return Windows::System::VirtualKey::H;
		case 'i':
			return Windows::System::VirtualKey::I;
		case 'j':
			return Windows::System::VirtualKey::J;
		case 'k':
			return Windows::System::VirtualKey::K;
		case 'l':
			return Windows::System::VirtualKey::L;
		case ',':
			return Windows::System::VirtualKey::M;
		case 'n':
			return Windows::System::VirtualKey::N;
		case 'o':
			return Windows::System::VirtualKey::O;
		case 'p':
			return Windows::System::VirtualKey::P;
		case 'q':
			return Windows::System::VirtualKey::Q;
		case 'r':
			return Windows::System::VirtualKey::R;
		case 's':
			return Windows::System::VirtualKey::S;
		case 't':
			return Windows::System::VirtualKey::T;
		case 'u':
			return Windows::System::VirtualKey::U;
		case 'v':
			return Windows::System::VirtualKey::V;
		case 'w':
			return Windows::System::VirtualKey::W;
		case 'x':
			return Windows::System::VirtualKey::X;
		case 'y':
			return Windows::System::VirtualKey::Y;
		case 'z':
			return Windows::System::VirtualKey::Z;
		case ' ':
			return Windows::System::VirtualKey::Space;
		default:
			return Windows::System::VirtualKey::None;
		}
		return Windows::System::VirtualKey::None;
	}


	bool WindowWrapper::GetAsyncKeyState(char rk)
	{
		if (rk == 27)
			return IsFakePersistentKeyPressed(FakeKey::Escape);

		auto vk = GetVirtualKey(rk);
		return (m_wnd->GetAsyncKeyState(vk) & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
	}

	void WindowWrapper::OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, bool isClose)
	{
		m_windowClosed = isClose;
	}

	bool WindowWrapper::Pump()
	{
		if (m_windowVisible)
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
		else
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);

		return !m_windowClosed;
	}

	void WindowWrapper::SetFakePersistentKeyCombo(unsigned int keys)
	{
		m_persistentKeys |= keys;
	}

	WindowWrapper::~WindowWrapper()
	{

	}

	bool WindowWrapper::IsFakePersistentKeyPressed(const FakeKey key)
	{
		auto res = (m_persistentKeys & static_cast<unsigned int>(key)) == static_cast<unsigned int>(key);
		m_persistentKeys &= ~static_cast<int>(key);//If it was detected, remove it...
		return res;
	}
}