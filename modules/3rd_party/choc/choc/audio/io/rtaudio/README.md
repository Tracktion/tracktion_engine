## RtAudio

This folder contains the RtAudio and RtMidi libraries. I could have written my own CHOC code to handle all the many audio and MIDI OS-specific APIs, but life is short, and RtAudio is permissively licensed..

Don't include these directly or add them to your build: the `choc_RtAudioPlayer.h` file includes them for you, so just include that and you're done. I've added some checks to these files to warn you if that happens by accident.

