#include "core/LinAppleCore.h"
#include "core/Peripheral_Internal.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "core/Util_Path.h"
#include "apple2/Disk.h"
#include <cstdio>
#include <iostream>
#include <getopt.h>
#include <cstring>

void VideoCallback(const uint32_t* pixels, int width, int height, int pitch) {
    (void)pixels; (void)width; (void)height; (void)pitch;
}

void AudioCallback(const int16_t* samples, size_t num_samples) {
    (void)samples; (void)num_samples;
}

void TitleCallback(const char* title) {
    (void)title;
}

auto main(int argc, char* argv[]) -> int {
    std::string configPath = Path::FindDataFile("linapple.conf");
    if (!configPath.empty()) {
        Configuration::Instance().Load(configPath);
    } else {
        std::string fallbackPath = Path::GetUserConfigDir();
        Path::EnsureDirExists(fallbackPath);
        Configuration::Instance().Load(fallbackPath + "linapple.conf");
    }

    static struct option long_options[] = {
        {"test-cpu", required_argument, nullptr, 't'},
        {"test-6502", no_argument, nullptr, '6'},
        {"test-65c02", no_argument, nullptr, 'C'},
        {"boot", no_argument, nullptr, 'b'},
        {"d1", required_argument, nullptr, '1'},
        {"d2", required_argument, nullptr, '2'},
        {"list-hardware", no_argument, nullptr, 0x100},
        {"hardware-info", required_argument, nullptr, 0x101},
        {nullptr, 0, 0, 0}
    };

    const char* disk1 = nullptr;
    const char* disk2 = nullptr;
    const char* hardwareName = nullptr;
    bool listHardware = false;

    int c = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "t:b6C1:2:", long_options, &option_index)) != -1) {
        switch (c) {
            case 't':
                Linapple_CpuTest(optarg);
                return 0;
            case '6':
                g_Apple2Type = A2TYPE_APPLE2PLUS;
                break;
            case 'C':
                g_Apple2Type = A2TYPE_APPLE2EENHANCED;
                break;
            case 'b':
                break;
            case '1':
                disk1 = optarg;
                break;
            case '2':
                disk2 = optarg;
                break;
            case 0x100:
                listHardware = true;
                break;
            case 0x101:
                hardwareName = optarg;
                break;
        }
    }

    if (listHardware) {
        Linapple_ListHardware();
        return 0;
    }

    if (hardwareName) {
        Peripheral_t* p = Peripheral_Find_Internal(hardwareName);
        if (p) {
            printf("Hardware Info: %s\n", p->name);
            printf("ABI Version: %d\n", p->abi_version);
            printf("Compatible Slots: ");
            bool first = true;
            for (int i = 0; i < NUM_SLOTS; ++i) {
                if (p->compatible_slots & (1u << static_cast<uint32_t>(i))) {
                    if (!first) printf(", ");
                    printf("%d", i);
                    first = false;
                }
            }
            printf("\n");
        } else {
            fprintf(stderr, "Error: Unknown hardware '%s'\n", hardwareName);
            return 1;
        }
        return 0;
    }

    std::cout << "Starting LinApple Headless Frontend…" << std::endl;

    Linapple_Init();
    Linapple_SetVideoCallback(VideoCallback);
    Linapple_SetAudioCallback(AudioCallback);
    Linapple_SetTitleCallback(TitleCallback);

    if (disk1) {
        DiskInsert(0, disk1, false, false);
    }
    if (disk2) {
        DiskInsert(1, disk2, false, false);
    }

    Linapple_RegisterPeripherals();

    g_state.mode = MODE_RUNNING;

    // Simulate 60 frames (1 second of emulation)
    for (int i = 0; i < 60; ++i) {
        Linapple_RunFrame(17030);

        if (i == 10) {
            Linapple_SetKeyState('H', true);
            Linapple_RunFrame(100);
            Linapple_SetKeyState('H', false);
        }
    }

    Linapple_Shutdown();
    std::cout << "Headless execution complete." << std::endl;
    return 0;
}
