//
// This file demonstrates how to initialize Pico::Engine for a universal app.
//

#include "pch.h"
#include "app.h"
#include "WindowWrapper.h"
#include "..\PicoEngineSample.Shared\picoengine.h"
#include "..\PicoEngineSample.Shared\PicoEngineSample.h"

using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Microsoft::WRL;
using namespace Platform;

using namespace PicoEngineSample;

struct AppImpl
{
	AppImpl()
	{
	}

	CogitareComputing::Pico::IWindowWrapperPtr PicoWindowWrapper;

	void CreateWindowWrapper(CoreWindow^ window)
	{
		PicoWindowWrapper = std::make_shared<SampleApp::WindowWrapper>(window);
	}

	~AppImpl()
	{
	}
};

// Helper to convert a length in device-independent pixels (DIPs) to a length in physical pixels.
inline float ConvertDipsToPixels(float dips, float dpi)
{
    static const float dipsPerInch = 96.0f;
    return floor(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
}

// Implementation of the IFrameworkViewSource interface, necessary to run our app.
ref class SimpleApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
    {
		//Windows::UI::ViewManagement::ApplicationView::PreferredLaunchViewSize = Size(640, 480);
		//Windows::UI::ViewManagement::ApplicationView::PreferredLaunchWindowingMode = Windows::UI::ViewManagement::ApplicationViewWindowingMode::PreferredLaunchViewSize;
		return ref new App();
    }
};

// The main function creates an IFrameworkViewSource for our app, and runs the app.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    auto simpleApplicationSource = ref new SimpleApplicationSource();
    CoreApplication::Run(simpleApplicationSource);
    return 0;
}

App::App()
	:m_impl(new AppImpl())
{
}

// The first method called when the IFrameworkView is being created.
void App::Initialize(CoreApplicationView^ applicationView)
{
    // Register event handlers for app lifecycle. This example includes Activated, so that we
    // can make the CoreWindow active and start rendering on the window.
    applicationView->Activated += 
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

    // Logic for other event handlers could go here.
    // Information about the Suspending and Resuming event handlers can be found here:
    // http://msdn.microsoft.com/en-us/library/windows/apps/xaml/hh994930.aspx
}

// Called when the CoreWindow object is created (or re-created).
void App::SetWindow(CoreWindow^ window)
{
    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

    window->Closed += 
        ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerPressed);
	window->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerReleased);
	window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerMoved);

	SystemNavigationManager::GetForCurrentView()->AppViewBackButtonVisibility = AppViewBackButtonVisibility::Visible;

	SystemNavigationManager::GetForCurrentView()->BackRequested +=
		ref new EventHandler<BackRequestedEventArgs^>(this, &App::OnBackRequested);

	((AppImpl*)m_impl)->CreateWindowWrapper(window);
}

// Initializes scene resources
void App::Load(Platform::String^ entryPoint)
{
    
}

// This method is called after the window becomes active.
void App::Run()
{
	auto state = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->Orientation;
	auto bounds = CoreWindow::GetForCurrentThread()->Bounds;

	//bounds.Width, bounds.Height
	CogitareComputing::Pico::SystemSpecificData sData(((AppImpl*)m_impl)->PicoWindowWrapper);

	SampleApp::PicoEngineSample sample;
	sample.Run(sData, bounds.Width, bounds.Height);
}

// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void App::Uninitialize()
{
}

// Application lifecycle event handler.
void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
    // Run() won't start until the CoreWindow is activated.
    CoreWindow::GetForCurrentThread()->Activate();
}

// Window event handlers.
void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
    mWindowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
    mWindowClosed = true;
}


void App::OnBackRequested(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e)
{
	AppImpl* pimpl = (AppImpl*)m_impl;
	if (pimpl != nullptr)
	{
		if (auto w = pimpl->PicoWindowWrapper)
		{
			if (auto ww = (SampleApp::WindowWrapper*)(w.get()))
			{
				ww->SetFakePersistentKeyCombo(static_cast<int>(SampleApp::WindowWrapper::FakeKey::Escape));
				e->Handled = true;
			}
		}
	}

}

void App::OnPointerPressed(CoreWindow^ sender, PointerEventArgs^ args)
{
	args->Handled = true;
}

void App::OnPointerReleased(CoreWindow^ sender, PointerEventArgs^ args)
{
	args->Handled = true;
}

void App::OnPointerMoved(CoreWindow^ sender, PointerEventArgs^ args)
{
	args->Handled = true;
}
