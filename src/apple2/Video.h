#include <cstdint>
#include <atomic>
#include <mutex>
#pragma once

struct SS_IO_Video;

// Types
enum
{
  APPLE2_VISIBLE_WIDTH  = 280,
  APPLE2_VISIBLE_HEIGHT = 192,

  VIDEO_SCALE_FACTOR    = 2,

  VIDEO_WIDTH           = APPLE2_VISIBLE_WIDTH * VIDEO_SCALE_FACTOR, // 560
  VIDEO_HEIGHT          = APPLE2_VISIBLE_HEIGHT * VIDEO_SCALE_FACTOR, // 384

  TEXT_COLUMNS          = 40,
  TEXT_ROWS             = 24,
  DIRTY_CELL_ROWS       = 32,

  MAX_PALETTE_SIZE      = 256,
  DEFAULT_GRAY_COMPONENT = 0xC0,

  HGR_MATRIX_YOFFSET    = 2
};

enum VIDEOTYPE {
  VT_MONO_CUSTOM,
  VT_COLOR_STANDARD,
  VT_COLOR_TEXT_OPTIMIZED,
  VT_COLOR_TVEMU,
  VT_COLOR_HALF_SHIFT_DIM,
  VT_MONO_AMBER,
  VT_MONO_GREEN,
  VT_MONO_WHITE,
  VT_NUM_MODES
};

enum VideoFlag_e {
  VF_80COL = 0x00000001,
  VF_DHIRES = 0x00000002,
  VF_HIRES = 0x00000004,
  VF_MASK2 = 0x00000008,
  VF_MIXED = 0x00000010,
  VF_PAGE2 = 0x00000020,
  VF_TEXT = 0x00000040
};

/*long*/
enum AppleFont_e {
  // 40-Column mode is 1x Zoom (default)
  // 80-Column mode is ~0.75x Zoom (7 x 16)
  // Tiny mode is 0.5 zoom (7x8) for debugger
  APPLE_FONT_WIDTH = 14, // in pixels
  APPLE_FONT_HEIGHT = 16, // in pixels

  // Each cell has a reserved aligned pixel area (grid spacing)
  APPLE_FONT_CELL_WIDTH = 16, APPLE_FONT_CELL_HEIGHT = 16,

  // The bitmap contains 3 regions
  // Each region is 256x256 pixels = 16x16 chars
  APPLE_FONT_X_REGIONSIZE = 256, // in pixelx
  APPLE_FONT_Y_REGIONSIZE = 256, // in pixels

  // Starting Y offsets (pixels) for the regions
  APPLE_FONT_Y_APPLE_2PLUS = 0, // ][+
  APPLE_FONT_Y_APPLE_80COL = 256, // //e (inc. Mouse Text)
  APPLE_FONT_Y_APPLE_40COL = 512, // ][
};

// STANDARD LINUX COLORS
constexpr uint8_t CREAM       = 0xF6;
constexpr uint8_t MEDIUM_GRAY = 0xF7;
constexpr uint8_t DARK_GRAY   = 0xF8;
constexpr uint8_t RED         = 0xF9;
constexpr uint8_t GREEN       = 0xFA;
constexpr uint8_t YELLOW      = 0xFB;
constexpr uint8_t BLUE        = 0xFC;
constexpr uint8_t MAGENTA     = 0xFD;
constexpr uint8_t CYAN        = 0xFE;
constexpr uint8_t WHITE       = 0xFF;

inline uint32_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

enum Color_Palette_Index_e {
  // Really need to have Quarter Green and Quarter Blue for Hi-Res
  BLACK,
  DARK_RED,
  DARK_GREEN,       // Half Green
  DARK_YELLOW,
  DARK_BLUE,        // Half Blue
  DARK_MAGENTA,
  DARK_CYAN,
  LIGHT_GRAY,
  MONEY_GREEN,
  SKY_BLUE,

  // OUR CUSTOM COLORS
  DEEP_RED,
  LIGHT_BLUE,
  BROWN,
  ORANGE,
  PINK,
  AQUA,

  // CUSTOM HGR COLORS (don't change order) - For tv emulation mode
  HGR_BLACK,
  HGR_WHITE,
  HGR_BLUE,
  HGR_RED,
  HGR_GREEN,
  HGR_MAGENTA,
  HGR_GREY1,
  HGR_GREY2,
  HGR_YELLOW,
  HGR_AQUA,
  HGR_PURPLE,
  HGR_PINK,

  // USER CUSTOMIZABLE COLOR
  MONOCHROME_CUSTOM,

  // Pre-set "Monochromes"
  MONOCHROME_AMBER,
  MONOCHROME_GREEN,
  MONOCHROME_WHITE,

  DARKER_YELLOW,
  DARKEST_YELLOW,
  LIGHT_SKY_BLUE,
  DARKER_SKY_BLUE,
  DEEP_SKY_BLUE,
  DARKER_CYAN,
  DARKEST_CYAN,
  HALF_ORANGE,
  DARKER_BLUE,
  DARKER_GREEN,
  DARKEST_GREEN,
  LIGHTEST_GRAY,
  NUM_COLOR_PALETTE
};

// Globals
extern int g_iStatusCycle; // cycler for status panel showing

extern bool g_ShowLeds; // if we should show drive leds

extern bool graphicsmode;
extern uint32_t monochrome;
extern uint32_t g_videotype;
extern uint32_t g_uVideoMode;
extern uint32_t g_singlethreaded;
extern std::recursive_mutex g_video_draw_mutex; // drawing mutex for writing to SDL surface
extern std::atomic<bool> g_bFrameReady;

// Surfaces for drawing
struct VideoColor {
  uint8_t r, g, b, a;
};

struct VideoRect {
  int x, y, w, h;
};

struct VideoSurface {
  uint8_t* pixels;
  int w, h, pitch;
  int bpp; // bytes per pixel: 1 for INDEX8, 4 for RGB32
  VideoColor palette[256];
};

extern VideoSurface *g_hLogoBitmap;
extern VideoSurface *g_hStatusSurface;  // status panel

extern VideoSurface *g_hSourceBitmap;
extern VideoSurface *g_hDeviceBitmap;
extern VideoSurface *g_origscreen; // reserved for stretching

VideoSurface* VideoCreateSurface(int w, int h, int bpp);
void VideoDestroySurface(VideoSurface* s);
VideoSurface* VideoLoadXPM(const char * const *xpm);


uint32_t* VideoGetOutputBuffer();
VideoColor* VideoGetOutputPalette();

void SetBudgetVideo(bool);
bool GetBudgetVideo();
void SetCurrentCLK6502();

// Prototypes

void CreateColorMixMap();

bool VideoApparentlyDirty();

void VideoBenchmark();

void VideoCheckPage(bool);

void VideoChooseColor();

void VideoDestroy();

void VideoDrawLogoBitmap(/* HDC hDstDC */);

void VideoDisplayLogo();

bool VideoHasRefreshed();

void VideoInitialize();

void VideoRealizePalette(/*HDC*/);

void VideoSetNextScheduledUpdate();

void VideoRedrawScreen();

void VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode =0, bool bRedrawWholeScreen=false);

void VideoPerformRefresh();

void VideoReinitialize();

void VideoResetState();

uint16_t VideoGetScannerAddress(bool *pbVblBar_OUT, const uint32_t uExecutedCycles);

bool VideoGetVbl(uint32_t uExecutedCycles);

void VideoUpdateVbl(uint32_t dwCyclesThisFrame);

void VideoUpdateFlash();

bool VideoGetSW80COL(void);
bool VideoGetSWDHIRES(void);
bool VideoGetSWHIRES(void);
bool VideoGetSW80STORE(void);
bool VideoGetSWMIXED(void);
bool VideoGetSWPAGE2(void);
bool VideoGetSWTEXT(void);
bool VideoGetSWAltCharSet(void);

uint32_t VideoGetSnapshot(SS_IO_Video *pSS);

uint32_t VideoSetSnapshot(SS_IO_Video *pSS);

uint8_t VideoCheckMode(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);

uint8_t VideoCheckVbl(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);

uint8_t VideoSetMode(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);
