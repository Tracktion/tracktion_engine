/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ProjectItemID::ProjectItemID() noexcept {}
ProjectItemID::~ProjectItemID() noexcept {}

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

    *this = ProjectItemID (mid, pid);
}

ProjectItemID::ProjectItemID (int itemID, int projectID) noexcept
   : combinedID ((((juce::int64) projectID) << 32) | itemID)
{
}

ProjectItemID::ProjectItemID (const ProjectItemID& other) noexcept
   : combinedID (other.combinedID)
{
}

ProjectItemID ProjectItemID::operator= (const ProjectItemID& other) noexcept
{
    combinedID = other.combinedID;
    return *this;
}

int ProjectItemID::getProjectID() const         { return (int) (combinedID >> 32); }
int ProjectItemID::getItemID() const            { return (int) combinedID; }

juce::String ProjectItemID::toString() const
{
    return juce::String::toHexString (getProjectID()) + '/' + juce::String::toHexString (getItemID());
}

juce::String ProjectItemID::toStringSuitableForFilename() const
{
    return juce::String::toHexString (getProjectID()) + '_' + juce::String::toHexString (getItemID());
}

ProjectItemID ProjectItemID::createNewID (int projectID) noexcept
{
    return ProjectItemID (juce::Random::getSystemRandom().nextInt (0x3ffffff), projectID);
}

ProjectItemID ProjectItemID::fromProperty (const juce::ValueTree& v, const juce::Identifier& prop)
{
    return ProjectItemID (v.getProperty (prop).toString());
}

ProjectItemID ProjectItemID::withNewProjectID (int newProjectID) const
{
    return { getItemID(), newProjectID };
}
