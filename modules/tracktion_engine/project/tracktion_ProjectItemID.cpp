/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

ProjectItemID::ProjectItemID (const juce::String& asString) noexcept
{
    int mid = 0;
    int pid = 0;
    int* number = &pid;

    for (auto p = asString.getCharPointer();; ++p)
    {
        auto c = *p;
        auto hex = juce::CharacterFunctions::getHexDigitValue (c);

        if (c == 0)
        {
            break; // End of string
        }
        else if (hex >= 0)
        {
            *number = ((*number) << 4) | hex;
        }
        else if ((c == '/' || c == '_') && number == &pid)
        {
            number = &mid;
        }
        else
        {
            // Invalid character encountered
            mid = 0;
            pid = 0;
            break;
        }
    }

    *this = ProjectItemID (mid, ProjectID (pid));
}

ProjectItemID::ProjectItemID (int itemID, ProjectID projectID) noexcept
   : combinedID ((((int64_t) projectID.toInt()) << 32) | itemID)
{
}

ProjectID ProjectItemID::getProjectID() const   { return ProjectID ((int) (combinedID >> 32)); }
int ProjectItemID::getItemID() const            { return (int) combinedID; }

juce::String ProjectItemID::toString() const
{
    return juce::String::toHexString (getProjectID().toInt()) + '/' + juce::String::toHexString (getItemID());
}

juce::String ProjectItemID::toStringSuitableForFilename() const
{
    return juce::String::toHexString (getProjectID().toInt()) + '_' + juce::String::toHexString (getItemID());
}

ProjectItemID ProjectItemID::createNewID (ProjectID projectID) noexcept
{
    return ProjectItemID (juce::Random::getSystemRandom().nextInt (0x3ffffff), projectID);
}

ProjectItemID ProjectItemID::fromProperty (const juce::ValueTree& v, const juce::Identifier& prop)
{
    return ProjectItemID (v.getProperty (prop).toString());
}

ProjectItemID ProjectItemID::withNewProjectID (ProjectID newProjectID) const
{
    return { getItemID(), newProjectID };
}

}} // namespace tracktion { inline namespace engine
