#pragma once
#include "..\PicoEngineSample.Shared\picoengine.h"

namespace SampleApp
{
	class WindowWrapper : public CogitareComputing::Pico::IWindowWrapper
	{
	public:
		enum class FakeKey
		{
			None = 0,
			Escape = 16,
		};

		WindowWrapper(Windows::UI::Core::CoreWindow^ wnd);

		EGLNativeWindowType NativeWindowType();

		void OnVisibilityChanged(Windows::UI::Core::CoreWindow ^ sender, Windows::UI::Core::VisibilityChangedEventArgs ^ args);

		Windows::System::VirtualKey GetVirtualKey(char rk);

		bool GetAsyncKeyState(char rk) override;

		void OnWindowClosed(Windows::UI::Core::CoreWindow ^ sender, bool isClose);

		bool Pump();

		void SetFakePersistentKeyCombo(unsigned int keys);

		virtual ~WindowWrapper();
	private:
		bool IsFakePersistentKeyPressed(const FakeKey key);

#pragma warning(push)
#pragma warning(disable: 4451)
		Windows::UI::Core::CoreWindow^ m_wnd;
#pragma warning(pop)

		bool m_windowClosed;
		bool m_windowVisible;
		unsigned int m_persistentKeys;
		bool m_showBackButton;
	};
}