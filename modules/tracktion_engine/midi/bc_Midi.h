/////////////////////////////////////////////////////////////////////////////////////////////
//     ____             _    ____                            _                             //
//    | __ )  ___  __ _| |_ / ___|___  _ __  _ __   ___  ___| |_                           //
//    |  _ \ / _ \/ _` | __| |   / _ \| '_ \| '_ \ / _ \/ __| __|     Copyright 2023       //
//    | |_) |  __/ (_| | |_| |__| (_) | | | | | | |  __/ (__| |_    www.beatconnect.com    //
//    |____/ \___|\__,_|\__|\____\___/|_| |_|_| |_|\___|\___|\__|                          //
//                                                                                         //
/////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace tracktion { inline namespace engine { namespace BeatConnect
		{
			static const int midiPitchWheelBase = 0x2000;
			static const int semitonesPerOctave = 12;

			struct midiNoteValue
			{
				const int midiNote;
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
					{0, 8.1757989156, "C1"},
					{1, 8.6619572180, "Db1"},
					{2, 9.1770239974, "D1"},
					{3, 9.7227182413, "Eb1"},
					{4, 10.3008611535, "E1"},
					{5, 10.9133822323, "F1"},
					{6, 11.5623257097, "Gb1"},
					{7, 12.2498573744, "G1"},
					{8, 12.9782717994, "Ab1"},
					{9, 13.750, "A1"},
					{10, 14.5676175474, "Bb1"},
					{11, 15.4338531643, "B1"},
					{12, 16.3515978313 , "C2"},
					{13, 17.3239144361, "Db2"},
					{14, 18.3540479948, "D2"},
					{15, 19.4454364826, "Eb2"},
					{16, 20.6017223071, "E2"},
					{17, 21.8267644646, "F2"},
					{18, 23.1246514195, "Gb2"},
					{19, 24.4997147489, "G2"},
					{20, 25.9565435987, "Ab2"},
					{21, 27.50, "A2"},
					{22, 29.1352350949, "Bb2"},
					{23, 30.8677063285, "B2"},
					{24, 32.7031956626, "C3"},
					{25, 34.6478288721, "Db3"},
					{26, 36.7080959897, "D3"},
					{27, 38.8908729653, "Eb3"},
					{28, 41.2034446141, "E3"},
					{29, 43.6535289291, "F3"},
					{30, 46.2493028390, "Gb3"},
					{31, 48.9994294977, "G3"},
					{32, 51.9130871975, "Ab3"},
					{33, 55.0, "A3"},
					{34, 58.2704701898, "Bb3"},
					{35, 61.7354126570, "B3"},
					{36, 65.4063913251, "C4"},
					{37, 69.2956577442, "Db4"},
					{38, 73.4161919794, "D4"},
					{39, 77.7817459305, "Eb4"},
					{40, 82.4068892282, "E4"},
					{41, 87.3070578583, "F4"},
					{42, 92.4986056779, "Gb4"},
					{43, 97.9988589954, "G4"},
					{44, 103.8261743950, "Ab4"},
					{45, 110.0, "A4"},
					{46, 116.5409403795, "Bb4"},
					{47, 123.4708253140, "B4"},
					{48, 130.8127826503, "C5"},
					{49, 138.5913154884, "Db5"},
					{50, 146.8323839587, "D5"},
					{51, 155.5634918610, "Eb5"},
					{52, 164.8137784564, "E5"},
					{53, 174.6141157165, "F5"},
					{54, 184.9972113558, "Gb5"},
					{55, 195.9977179909, "G5"},
					{56, 207.652348790, "Ab5"},
					{57, 220.0, "A5"},
					{58, 233.0818807590, "Bb5"},
					{59, 246.9416506281, "B5"},
					{60, 261.6255653006, "C6"},
					{61, 277.1826309769, "Db6"},
					{62, 293.6647679174, "D6"},
					{63, 311.1269837221, "Eb6"},
					{64, 329.6275569129, "E6"},
					{65, 349.2282314330, "F6"},
					{66, 369.9944227116, "Gb6"},
					{67, 391.9954359817, "G6"},
					{68, 415.3046975799, "Ab6"},
					{69, 440.0, "A6"},
					{70, 466.1637615181, "Bb6"},
					{71, 493.8833012561, "B6"},
					{72, 523.2511306012, "C7"},
					{73, 554.3652619537, "Db7"},
					{74, 587.3295358348, "D7"},
					{75, 622.2539674442, "Eb7"},
					{76, 659.2551138257, "E7"},
					{77, 698.4564628660, "F7"},
					{78, 739.9888454233, "Gb7"},
					{79, 783.9908719635, "G7"},
					{80, 830.6093951599, "Ab7"},
					{81, 880.0, "A7"},
					{82, 932.3275230362, "Bb7"},
					{83, 987.7666025122, "B7"},
					{84, 1046.5022612024, "C8"},
					{85, 1108.7305239075, "Db8"},
					{86, 1174.6590716696, "D8"},
					{87, 1244.5079348883, "Eb8"},
					{88, 1318.5102276515, "E8"},
					{89, 1396.9129257320, "F8"},
					{90, 1479.9776908465, "Gb8"},
					{91, 1567.9817439270, "G8"},
					{92, 1661.2187903198, "Ab8"},
					{93, 1760.0, "A8"},
					{94, 1864.6550460724, "Bb8"},
					{95, 1975.5332050245, "B8"},
					{96, 2093.0045224048, "C9"},
					{97, 2217.4610478150, "Db9"},
					{98, 2349.3181433393, "D9"},
					{99, 2489.0158697766, "Eb9"},
					{100, 2637.0204553030, "E9"},
					{101, 2793.8258514640, "F9"},
					{102, 2959.9553816931, "Gb9"},
					{103, 3135.9634878540, "G9"},
					{104, 3322.4375806396, "Ab9"},
					{105, 3520.0, "A9"},
					{106, 3729.3100921447, "Bb9"},
					{107, 3951.0664100490, "B9"},
					{108, 4186.0090448096, "C10"},
					{109, 4434.922095630, "Db10"},
					{110, 4698.6362866785, "D10"},
					{111, 4978.0317395533, "Eb10"},
					{112, 5274.0409106059, "E10"},
					{113, 5587.6517029281, "F10"},
					{114, 5919.9107633862, "Gb10"},
					{115, 6271.9269757080, "G10"},
					{116, 6644.8751612791, "Ab10"},
					{117, 7040.0, "A10"},
					{118, 7458.6201842894, "Bb10"},
					{119, 7902.1328200980, "B10"},
					{120, 8372.0180896192, "C11"},
					{121, 8869.8441912599, "Db11"},
					{122, 9397.2725733570, "D11"},
					{123, 9956.0634791066, "Eb11"},
					{124, 10548.0818212118, "E11"},
					{125, 11175.3034058561, "F11"},
					{126, 11839.8215267723, "Gb11"},
					{127, 12543.8539514160, "G11"}
				};

				static double getFrequencyByPitchWheel(double p_pitchWheelPosition, int p_pitchWheelSemitoneRange, double p_noteHz)
				{
					return p_noteHz * pow(2.0, ((p_pitchWheelPosition - (double)midiPitchWheelBase) * p_pitchWheelSemitoneRange) /
						(semitonesPerOctave * (double)midiPitchWheelBase));
				}

				static int getMidiNote(double p_hz)
				{
					for (auto element : midiNoteValues)
					{
						if (element.freqHz == p_hz)
							return element.midiNote;
						if ((&element - 1) != nullptr && (&element + 1) != nullptr)
							if (p_hz > (&element - 1)->freqHz && p_hz < (&element + 1)->freqHz)
								return element.midiNote;
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
						if (element.midiNote == p_midiNote)
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
					for (auto element : midiNoteValues)
					{
						if (element.freqHz == p_hz)
							return element.noteName;
						if ((&element - 1) != nullptr && (&element + 1) != nullptr)
							if (p_hz > (&element - 1)->freqHz && p_hz < (&element + 1)->freqHz)
								return element.noteName;
					}
					return "";
				}

				static juce::String getNoteName(unsigned int p_midiNote)
				{
					for (auto element : midiNoteValues)
						if (element.midiNote == p_midiNote)
							return element.noteName;
					return "";
				}
			};
}}}