// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
  ==============================================================================

    This file was auto-generated by the Introjucer!

    It contains the basic startup code for a Juce application.

  ==============================================================================
*/
/*
  ==============================================================================

  Main.cpp

This file is part of MIDI2LR. Copyright 2015-2017 by Rory Jaffe.

MIDI2LR is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

MIDI2LR is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
MIDI2LR.  If not, see <http://www.gnu.org/licenses/>.
  ==============================================================================
*/

#include <exception>
#include <fstream>
#include <memory>
#include "../JuceLibraryCode/JuceHeader.h"
#include <cereal/archives/binary.hpp>
#include "CCoptions.h"
#include "CommandMap.h"
#include "ControlsModel.h"
#include "LR_IPC_IN.h"
#include "LR_IPC_OUT.h"
#include "MainComponent.h"
#include "MainWindow.h"
#include "MIDIProcessor.h"
#include "MIDISender.h"
#include "PWoptions.h"
#include "ProfileManager.h"
#include "SettingsManager.h"
#include "VersionChecker.h"

const juce::String ShutDownString{"--LRSHUTDOWN"};

class MIDI2LRApplication final: public juce::JUCEApplication {
public:
    MIDI2LRApplication():
        lr_ipc_in_{std::make_shared<LR_IPC_IN>(&controls_model_, &profile_manager_, &command_map_)},
        lr_ipc_out_{std::make_shared<LR_IPC_OUT>(&controls_model_, &command_map_)},
        midi_processor_{std::make_shared<MIDIProcessor>()},
        midi_sender_{std::make_shared<MIDISender>()}
    {
        CCoptions::LinkToControlsModel(&controls_model_);
        PWoptions::LinkToControlsModel(&controls_model_);
    }

    const juce::String getApplicationName() override
    {
        return ProjectInfo::projectName;
    }
    const juce::String getApplicationVersion() override
    {
        return ProjectInfo::versionString;
    }
    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    //==============================================================================

    void initialise(const juce::String& command_line) override
    {
        //Called when the application starts.

        // This will be called once to let the application do whatever initialization
        // it needs, create its windows, etc.

        // After the method returns, the normal event - dispatch loop will be run,
        // until the quit() method is called, at which point the shutdown() method
        // will be called to let the application clear up anything it needs to
        // delete.

        // If during the initialise() method, the application decides not to start -
        // up after all, it can just call the quit() method and the event loop won't
        // be run.

        if (command_line != ShutDownString) {
            {//scoped so archive gets flushed
                auto controllerfile =
                    juce::File::getSpecialLocation(juce::File::currentExecutableFile).
                    getSiblingFile("settings.bin").getFullPathName().toStdString();
                std::ifstream infile(controllerfile, std::ios::in | std::ios::binary);
                if (infile.is_open() && !infile.eof()) {
                    cereal::BinaryInputArchive iarchive(infile);
                    iarchive(controls_model_);
                }
            }
            midi_processor_->Init();
            midi_sender_->Init();
            lr_ipc_out_->Init(midi_processor_);
            //set the reference to the command map
            profile_manager_.Init(lr_ipc_out_, midi_processor_);
            //initialize the IPC_In
            lr_ipc_in_->Init(midi_sender_);
            // initialize the settings manager
            settings_manager_.Init(lr_ipc_out_);
            main_window_ = std::make_unique<MainWindow>(getApplicationName());
            main_window_->Init(&command_map_, lr_ipc_out_, midi_processor_,
                &profile_manager_, &settings_manager_, midi_sender_);
            // Check for latest version
            version_checker_.startThread();
        }
        else {
            // apparently the application is already terminated
            quit();
        }
    }

    void shutdown() override
    {
        //Called to allow the application to clear up before exiting.

        // After JUCEApplication::quit() has been called, the event - dispatch loop
        // will terminate, and this method will get called to allow the application
        // to sort itself out.

        // Be careful that nothing happens in this method that might rely on messages
        // being sent, or any kind of window activity, because the message loop is no
        // longer running at this point.
        lr_ipc_out_.reset();
        lr_ipc_in_.reset();
        midi_processor_.reset();
        midi_sender_.reset();
        main_window_ = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the application is being asked to quit: you can
        // ignore this request and let the application carry on running, or call
        // quit() to allow the application to close.
        if (lr_ipc_in_)
            lr_ipc_in_->PleaseStopThread();
        auto default_profile =
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).
            getSiblingFile("default.xml");
        command_map_.toXMLDocument(default_profile);
        {//scoped so archive gets flushed
            auto controllerfile =
                juce::File::getSpecialLocation(juce::File::currentExecutableFile).
                getSiblingFile("settings.bin").getFullPathName().toStdString();
            std::ofstream outfile(controllerfile, std::ios::out | std::ios::binary | std::ios::trunc);
            if (outfile.is_open()) {
                cereal::BinaryOutputArchive oarchive(outfile);
                oarchive(controls_model_);
            }
            else {
                juce::AlertWindow::showNativeDialogBox("Error",
                    "Unable to save control settings. Unable to open file settings.bin.", false);
            }
        }
        quit();
    }

    void anotherInstanceStarted(const juce::String& command_line) override
    {
        // When another instance of the application is launched while this one is
        // running, this method is invoked, and the commandLine parameter tells you
        // what the other instance's command-line arguments were.
        if (command_line == ShutDownString) {
            //shutting down
            systemRequestedQuit();
        }
    }

    void unhandledException(const std::exception * e,
        const juce::String& sourceFilename, int lineNumber
    )	override
    {
        // If any unhandled exceptions make it through to the message dispatch
        // loop, this callback will be triggered, in case you want to log them or
        // do some other type of error-handling.
        //
        // If the type of exception is derived from the std::exception class, the
        // pointer passed-in will be valid. If the exception is of unknown type,
        // this pointer will be null.
        if (juce::Logger::getCurrentLogger()) {
            if (e)
                juce::Logger::writeToLog(juce::String(e->what()) + " " + sourceFilename +
                    " line " + juce::String(lineNumber));
            else
                juce::Logger::writeToLog(sourceFilename + " line " + juce::String(lineNumber));
        }
        juce::AlertWindow::showNativeDialogBox("Error",
            "Unhandled exception. " + juce::String(e->what()) + " " + sourceFilename +
            " line " + juce::String(lineNumber), false);
        std::terminate(); // can't go on with the program
    }

private:
    CommandMap command_map_{};
    ControlsModel controls_model_{};
    ProfileManager profile_manager_{&controls_model_, &command_map_};
    SettingsManager settings_manager_{&profile_manager_};
    std::shared_ptr<LR_IPC_IN> lr_ipc_in_{nullptr};
    std::shared_ptr<LR_IPC_OUT> lr_ipc_out_{nullptr};
    std::shared_ptr<MIDIProcessor> midi_processor_{nullptr};
    std::shared_ptr<MIDISender> midi_sender_{nullptr};
    std::unique_ptr<MainWindow> main_window_{nullptr};
    VersionChecker version_checker_{&settings_manager_};
};

//==============================================================================
// This macro generates the main() routine that launches the application.
START_JUCE_APPLICATION(MIDI2LRApplication)