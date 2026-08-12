#include <JuceHeader.h>
#include "MainComponent.h"

class LayerHostApplication  : public juce::JUCEApplication
{
public:
    LayerHostApplication() {}

    const juce::String getApplicationName() override       { return "Layerhost"; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::Image splashImg (juce::Image::ARGB, 600, 350, true);
        {
            juce::Graphics g (splashImg);
            g.fillAll (juce::Colour (0xff121214)); // DAW dark background
            
            g.setColour (juce::Colour (0xff00b4d8)); // Main accent colour
            g.setFont (50.0f);
            g.drawText ("LAYERHOST", splashImg.getBounds().withY(-30), juce::Justification::centred, true);
            
            g.setColour (juce::Colours::white.withAlpha(0.7f));
            g.setFont (18.0f);
            g.drawText ("Loading Plugins and Engine...", splashImg.getBounds().withY(50), juce::Justification::centred, true);
            
            g.setColour (juce::Colour (0xff00b4d8).withAlpha(0.3f));
            g.drawRect (splashImg.getBounds(), 4.0f);
        }
        
        splashScreen = new juce::SplashScreen ("Splash", splashImg, true);
        splashScreen->deleteAfterDelay (juce::RelativeTime::seconds (3.0), false);
        
        // Delay main window creation to allow splash screen to render
        juce::MessageManager::callAsync ([this] {
            mainWindow.reset (new MainWindow (getApplicationName()));
        });
    }

    void shutdown() override
    {
        mainWindow = nullptr; 
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
    }

    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colour (0xff121214),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
    juce::SplashScreen* splashScreen = nullptr;
};

START_JUCE_APPLICATION (LayerHostApplication)
