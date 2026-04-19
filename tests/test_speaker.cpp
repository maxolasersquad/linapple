#include "doctest.h"
#include <vector>
#include <array>
#include <cmath>
#include "apple2/Speaker.h"
#include "apple2/CPU.h"
#include "core/LinAppleCore.h"
#include "core/Common_Globals.h"
#include "apple2/SoundCore.h"

// Forward declaration of the internal frontend update function used by the test
extern void SpkrFrontend_Update(uint32_t dwExecutedCycles);
extern void SpkrFrontend_Reset();

// Define a test callback to capture audio output
static std::vector<int16_t> g_testAudioBuffer;
static void TestAudioCallback(const int16_t* samples, size_t num_samples) {
    g_testAudioBuffer.insert(g_testAudioBuffer.end(), samples, samples + num_samples);
}

TEST_CASE("Speaker subsystem accurately generates and filters PCM audio") {
    Linapple_Init();
    Linapple_SetAudioCallback(TestAudioCallback);
    g_testAudioBuffer.clear();
    
    // Ensure cycles are initialized so CpuCalcCycles works predictably
    CpuInitialize();
    g_nCumulativeCycles = 1000;
    
    SUBCASE("Speaker records toggles with cycle accuracy") {
        uint64_t initialCycles = g_nCumulativeCycles;
        
        // We mock the executed cycles directly since we're bypassing CpuExecute
        uint32_t executedCycles = 10;
        CpuCalcCycles(executedCycles);
        SpkrToggle(nullptr, 0, 0, 0, 0, 0); // Toggle at the current cycle
        
        std::array<SpkrEvent, MAX_SPKR_EVENTS> events{};
        int count = SpkrGetEvents(events.data(), MAX_SPKR_EVENTS);
        
        CHECK(count >= 1); // Initialization might add events
        // The last event should be the one we just triggered
        CHECK(events[count-1].cycle == g_nCumulativeCycles);
        // And we expect it to have advanced
        CHECK(g_nCumulativeCycles >= initialCycles);
    }

    SUBCASE("SpkrFrontend_Update integrates fast pulses (PWM/Boxcar Filter)") {
        double clksPerSample = g_fCurrentCLK6502 / SPKR_SAMPLE_RATE;
        
        // Clear any previous events
        std::array<SpkrEvent, MAX_SPKR_EVENTS> dump{};
        SpkrGetEvents(dump.data(), MAX_SPKR_EVENTS);
        g_testAudioBuffer.clear();
        
        // Reset the frontend
        SpkrFrontend_Reset(); 
        
        // Advance cycle so we're not at 0, using a clean cycle baseline
        g_nCumulativeCycles = 1000;
        SpkrFrontend_Reset();

        // Simulate a PWM pulse: High for exactly 25% of a sample period, then low for 75%
        uint32_t pulseWidth = static_cast<uint32_t>(clksPerSample * 0.25);
        uint32_t remainingWidth = static_cast<uint32_t>(clksPerSample) - pulseWidth;
        
        // Toggle ON (move cycles forward by 1, then toggle)
        CpuCalcCycles(1);
        SpkrToggle(nullptr, 0, 0, 0, 0, 0); 
        
        // Move forward by pulseWidth and toggle OFF
        CpuCalcCycles(pulseWidth);
        SpkrToggle(nullptr, 0, 0, 0, 0, 0); 
        
        // Move forward to complete the sample period
        CpuCalcCycles(remainingWidth);

        // Run the frontend update
        SpkrFrontend_Update(1 + pulseWidth + remainingWidth);
        
        // A boxcar integrated system will emit an average:
        // (25% * +Volume) + (75% * -Volume) = -50% Volume.
        
        // Check index 0 (first stereo sample) since we explicitly aligned the cycles
        CHECK(g_testAudioBuffer.size() >= 2);
        CHECK(std::abs(g_testAudioBuffer[0]) < 0x3F00); 
    }

    Linapple_Shutdown();
}
