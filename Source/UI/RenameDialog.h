//
//  RenameDialog.h
//  Tonbox (Kitbox's rename window, made reusable)
//
//  A small text dialog inside the plugin window - not a window of its own,
//  which can open behind the host's floating plugin window. The owner keeps
//  it; the callback gets the new name only if Rename was pressed.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class RenameDialog
{
public:
    void open (juce::Component& owner, const juce::String& title, const juce::String& current,
               std::function<void (const juce::String&)> onRenamed)
    {
        auto* parent = owner.getTopLevelComponent();

        window = std::make_unique<juce::AlertWindow> (title, juce::String(), juce::MessageBoxIconType::NoIcon, parent);
        window->addTextEditor ("name", current);
        window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        if (auto* editor = window->getTextEditor ("name"))
            editor->selectAll();

        parent->addAndMakeVisible (*window);
        window->setCentrePosition (parent->getLocalBounds().getCentre());

        window->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, alive = std::weak_ptr<bool> (token), onRename = std::move (onRenamed)] (int result)
        {
            if (alive.expired() || window == nullptr)
                return;

            const auto name = window->getTextEditorContents ("name");
            window.reset();

            if (result == 1 && onRename)
                onRename (name);
        }), false);
    }

private:
    std::unique_ptr<juce::AlertWindow> window;
    std::shared_ptr<bool> token = std::make_shared<bool> (true);   // gone with the owner
};
