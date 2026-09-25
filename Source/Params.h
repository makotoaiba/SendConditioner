#pragma once
#include <juce_core/juce_core.h>

/** Parameter IDs and tempo-note tables shared by processor and editor. */
namespace scp
{
    // Pre-delay
    inline constexpr const char* pdSync  = "pdSync";
    inline constexpr const char* pdNote  = "pdNote";
    inline constexpr const char* pdMs    = "pdMs";
    // Filter / width
    inline constexpr const char* hpf     = "hpf";
    inline constexpr const char* lpf     = "lpf";
    inline constexpr const char* slope   = "slope";
    inline constexpr const char* monoLow = "monoLow";
    inline constexpr const char* width   = "width";
    // Sidechain ducker
    inline constexpr const char* duckOn  = "duckOn";
    inline constexpr const char* duckThr = "duckThr";
    inline constexpr const char* duckAmt = "duckAmt";
    inline constexpr const char* duckAtk = "duckAtk";
    inline constexpr const char* duckRel = "duckRel";
    // Tail gate
    inline constexpr const char* gateOn  = "gateOn";
    inline constexpr const char* gateThr = "gateThr";
    inline constexpr const char* gateLen = "gateLen";
    inline constexpr const char* gateRel = "gateRel";
    // Tempo LFO
    inline constexpr const char* lfoOn    = "lfoOn";
    inline constexpr const char* lfoRate  = "lfoRate";
    inline constexpr const char* lfoShape = "lfoShape";
    inline constexpr const char* lfoAmt   = "lfoAmt";
    // Output
    inline constexpr const char* autoGain  = "autoGain";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* outGain   = "outGain";
    inline constexpr const char* monoCheck = "monoCheck";
    inline constexpr const char* compare   = "compare";

    struct NoteTable
    {
        juce::StringArray names;
        std::vector<double> beats;   // length in quarter notes
    };

    inline const NoteTable& predelayNotes()
    {
        static const NoteTable t { { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T",
                                     "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" },
                                   { 1.0/16, 1.0/8, 1.0/6, 0.25, 0.375, 1.0/3,
                                     0.5, 0.75, 2.0/3, 1.0, 1.5, 2.0 } };
        return t;
    }

    inline const NoteTable& gateLengths()
    {
        static const NoteTable t { { "1/8", "1/4", "1/4D", "1/2", "1/2D", "1/1", "2/1" },
                                   { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0 } };
        return t;
    }

    inline const NoteTable& lfoRates()
    {
        static const NoteTable t { { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4",
                                     "1/4D", "1/2", "1/1", "2/1" },
                                   { 0.25, 1.0/3, 0.5, 0.75, 2.0/3, 1.0,
                                     1.5, 2.0, 4.0, 8.0 } };
        return t;
    }

    inline double beatsAt (const NoteTable& t, int index)
    {
        return t.beats[(size_t) juce::jlimit (0, (int) t.beats.size() - 1, index)];
    }
}
