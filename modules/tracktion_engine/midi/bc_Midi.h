/////////////////////////////////////////////////////////////////////////////////////////////
//     ____             _    ____                            _                             //
//    | __ )  ___  __ _| |_ / ___|___  _ __  _ __   ___  ___| |_                           //
//    |  _ \ / _ \/ _` | __| |   / _ \| '_ \| '_ \ / _ \/ __| __|     Copyright 2023       //
//    | |_) |  __/ (_| | |_| |__| (_) | | | | | | |  __/ (__| |_    www.beatconnect.com    //
//    |____/ \___|\__,_|\__|\____\___/|_| |_|_| |_|\___|\___|\__|                          //
//                                                                                         //
/////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace tracktion {
	inline namespace engine {
		namespace bc
		{
			static const int midiPitchWheelBase = 0x2000;
			static const int semitonesPerOctave = 12;

			struct midiNoteValue
			{
				const unsigned int midiNote;
				const double freqHz;
				const juce::String noteName;
			};

			static const midiNoteValue midiCalibriation = { 69, 440.0, "A4" };

			const struct MidiNote
			{
				// freq = 440 * 2^((x-69)/12)
				// calibration A4/midiNote 69/440hz
				// One 12 toned equal temperament semitone has a frequency ratio of 2^(1/12)
				// x-69 is the offset from the known calibration values
				static inline const std::vector<midiNoteValue> midiNoteValues =
				{
					{0, 8.1757989156, "C-1"},
					{1, 8.6619572180, "Db-1"},
					{2, 9.1770239974, "D-1"},
					{3, 9.7227182413, "Eb-1"},
					{4, 10.3008611535, "E-1"},
					{5, 10.9133822323, "F-1"},
					{6, 11.5623257097, "Gb-1"},
					{7, 12.2498573744, "G-1"},
					{8, 12.9782717994, "Ab-1"},
					{9, 13.750, "A-1"},
					{10, 14.5676175474, "Bb-1"},
					{11, 15.4338531643, "B-1"},
					{12, 16.3515978313 , "C0"},
					{13, 17.3239144361, "Db0"},
					{14, 18.3540479948, "D0"},
					{15, 19.4454364826, "Eb0"},
					{16, 20.6017223071, "E0"},
					{17, 21.8267644646, "F0"},
					{18, 23.1246514195, "Gb0"},
					{19, 24.4997147489, "G0"},
					{20, 25.9565435987, "Ab0"},
					{21, 27.50, "A0"},
					{22, 29.1352350949, "Bb0"},
					{23, 30.8677063285, "B0"},
					{24, 32.7031956626, "C1"},
					{25, 34.6478288721, "Db1"},
					{26, 36.7080959897, "D1"},
					{27, 38.8908729653, "Eb1"},
					{28, 41.2034446141, "E1"},
					{29, 43.6535289291, "F1"},
					{30, 46.2493028390, "Gb1"},
					{31, 48.9994294977, "G1"},
					{32, 51.9130871975, "Ab1"},
					{33, 55.0, "A1"},
					{34, 58.2704701898, "Bb1"},
					{35, 61.7354126570, "B1"},
					{36, 65.4063913251, "C2"},
					{37, 69.2956577442, "Db2"},
					{38, 73.4161919794, "D2"},
					{39, 77.7817459305, "Eb2"},
					{40, 82.4068892282, "E2"},
					{41, 87.3070578583, "F2"},
					{42, 92.4986056779, "Gb2"},
					{43, 97.9988589954, "G2"},
					{44, 103.8261743950, "Ab2"},
					{45, 110.0, "A2"},
					{46, 116.5409403795, "Bb2"},
					{47, 123.4708253140, "B2"},
					{48, 130.8127826503, "C3"},
					{49, 138.5913154884, "Db3"},
					{50, 146.8323839587, "D3"},
					{51, 155.5634918610, "Eb3"},
					{52, 164.8137784564, "E3"},
					{53, 174.6141157165, "F3"},
					{54, 184.9972113558, "Gb3"},
					{55, 195.9977179909, "G3"},
					{56, 207.652348790, "Ab3"},
					{57, 220.0, "A3"},
					{58, 233.0818807590, "Bb3"},
					{59, 246.9416506281, "B3"},
					{60, 261.6255653006, "C4"},
					{61, 277.1826309769, "Db4"},
					{62, 293.6647679174, "D4"},
					{63, 311.1269837221, "Eb4"},
					{64, 329.6275569129, "E4"},
					{65, 349.2282314330, "F4"},
					{66, 369.9944227116, "Gb4"},
					{67, 391.9954359817, "G4"},
					{68, 415.3046975799, "Ab4"},
					{69, 440.0, "A4"},
					{70, 466.1637615181, "Bb4"},
					{71, 493.8833012561, "B4"},
					{72, 523.2511306012, "C5"},
					{73, 554.3652619537, "Db5"},
					{74, 587.3295358348, "D5"},
					{75, 622.2539674442, "Eb5"},
					{76, 659.2551138257, "E5"},
					{77, 698.4564628660, "F5"},
					{78, 739.9888454233, "Gb5"},
					{79, 783.9908719635, "G5"},
					{80, 830.6093951599, "Ab5"},
					{81, 880.0, "A5"},
					{82, 932.3275230362, "Bb5"},
					{83, 987.7666025122, "B5"},
					{84, 1046.5022612024, "C6"},
					{85, 1108.7305239075, "Db6"},
					{86, 1174.6590716696, "D6"},
					{87, 1244.5079348883, "Eb6"},
					{88, 1318.5102276515, "E6"},
					{89, 1396.9129257320, "F6"},
					{90, 1479.9776908465, "Gb6"},
					{91, 1567.9817439270, "G6"},
					{92, 1661.2187903198, "Ab6"},
					{93, 1760.0, "A6"},
					{94, 1864.6550460724, "Bb6"},
					{95, 1975.5332050245, "B6"},
					{96, 2093.0045224048, "C7"},
					{97, 2217.4610478150, "Db7"},
					{98, 2349.3181433393, "D7"},
					{99, 2489.0158697766, "Eb7"},
					{100, 2637.0204553030, "E7"},
					{101, 2793.8258514640, "F7"},
					{102, 2959.9553816931, "Gb7"},
					{103, 3135.9634878540, "G7"},
					{104, 3322.4375806396, "Ab7"},
					{105, 3520.0, "A7"},
					{106, 3729.3100921447, "Bb7"},
					{107, 3951.0664100490, "B7"},
					{108, 4186.0090448096, "C8"},
					{109, 4434.922095630, "Db8"},
					{110, 4698.6362866785, "D8"},
					{111, 4978.0317395533, "Eb8"},
					{112, 5274.0409106059, "E8"},
					{113, 5587.6517029281, "F8"},
					{114, 5919.9107633862, "Gb8"},
					{115, 6271.9269757080, "G8"},
					{116, 6644.8751612791, "Ab8"},
					{117, 7040.0, "A8"},
					{118, 7458.6201842894, "Bb8"},
					{119, 7902.1328200980, "B8"},
					{120, 8372.0180896192, "C9"},
					{121, 8869.8441912599, "Db9"},
					{122, 9397.2725733570, "D9"},
					{123, 9956.0634791066, "Eb9"},
					{124, 10548.0818212118, "E9"},
					{125, 11175.3034058561, "F9"},
					{126, 11839.8215267723, "Gb9"},
					{127, 12543.8539514160, "G9"}
				};

				static double getFrequencyByPitchWheel(double p_pitchWheelPosition, int p_pitchWheelSemitoneRange, double p_noteHz)
				{
					return p_noteHz * pow(2.0, ((p_pitchWheelPosition - (double)midiPitchWheelBase) * p_pitchWheelSemitoneRange) /
						(semitonesPerOctave * (double)midiPitchWheelBase));
				}

				static int getMidiNote(double p_hz)
				{
					for (auto it = midiNoteValues.begin(); it != midiNoteValues.end(); it++)
					{
						if (it->freqHz == p_hz)
							return it->midiNote;
						if (it > midiNoteValues.begin() && it < midiNoteValues.end()) {
							if (p_hz > (it - 1)->freqHz && p_hz < (it + 1)->freqHz)
							{
								return it->midiNote;
							}
							else if (p_hz < midiNoteValues.begin()->freqHz)
							{
<<<<<<< HEAD
								return midiNoteValues.begin()->midiNote;
							}
							else if (p_hz > midiNoteValues.back().freqHz)
							{
								return midiNoteValues.back().midiNote;
=======
								return (int)midiNoteValues.begin()->freqHz;
							}
							else if (p_hz > midiNoteValues.back().freqHz)
							{
								return (int)midiNoteValues.back().freqHz;
>>>>>>> master
							}
						}
					}
					return -1;
				}

				static int getMidiNote(juce::String p_name)
				{
					for (auto element : midiNoteValues)
						if (element.noteName.toLowerCase() == p_name.toLowerCase())
							return element.midiNote;
					return -1;
				}

				static double getNoteFrequency(unsigned int p_midiNote)
				{
					for (auto element : midiNoteValues)
						if (element.midiNote == (int)p_midiNote)
							return element.freqHz;
					return -1;
				}

				static double getNoteFrequency(juce::String p_noteName)
				{
					for (auto element : midiNoteValues)
						if (element.noteName.toLowerCase() == p_noteName.toLowerCase())
							return element.freqHz;
					return -1;
				}

				static juce::String getNoteName(double p_hz)
				{

					for (auto it = midiNoteValues.begin(); it != midiNoteValues.end(); it++)
					{
						if (it->freqHz == p_hz)
							return it->noteName;
						if (it > midiNoteValues.begin() && it < midiNoteValues.end()) {
							if (p_hz > (it - 1)->freqHz && p_hz < (it + 1)->freqHz)
							{
								return it->noteName;
							}
							else if (p_hz < midiNoteValues.begin()->freqHz)
							{
								return midiNoteValues.begin()->noteName;
							}
							else if (p_hz > midiNoteValues.back().freqHz)
							{
								return midiNoteValues.back().noteName;
							}
						}
					}
					return "";
				}

				static juce::String getNoteName(unsigned int p_midiNote)
				{
					for (auto element : midiNoteValues)
						if (element.midiNote == (int)p_midiNote)
							return element.noteName;
					return "";
				}
			};
		}
	}
}
