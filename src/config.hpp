#include <chrono>
#include <cstdint>
#include <vector>

using color = const uint32_t;

///
/// Dimensions of the screenshots that will be displayed.
///
/// Specifying width will stack desktops vertically.
/// Specifying height will stack desktops horizontally.
///
/// One dimension *must* be set to zero and will be calculated at runtime.
///
/// Actual window width will be equal to dexpo_width + (dexpo_padding * 2)
///
const uint dexpo_width = 0;
const uint dexpo_height = 200;

const int16_t dexpo_x = 0; ///< X coordinate of window's top left corner
const int16_t dexpo_y = 0; ///< Y coordinate of window's top left corner

const uint16_t dexpo_padding = 4; ///< Padding around the screenshots

color dexpo_background = 0x444444;    ///< Window background
color dexpo_border_pres = 0xFFFFFF;   ///< Desktop preselection
color dexpo_border_nopres = 0x808080; ///< Desktop without preselection

const uint16_t dexpo_border_pres_width = 2; ///< Width of the preselection

const uint dexpo_border_width = 0; ///< dexpo window border

///
/// How often to take screenshots.
///
/// Recommended value is 10 seconds
//
/// NOTE: Setting timeout to values below 5 ms may severely increase CPU load.
///
const auto dexpo_screenshot_period = std::chrono::seconds (10);

///
/// Desktop viewport:
/// Top left coordinates of each of your desktops in the format
/// { x1, y1, x2, y2, x3, y3 } and so on.
///
/// Should be set only if you have a DE and
/// `xprop -root _NET_DESKOP_VIEWPORT`
/// returns `0, 0`
///
/// Otherwise leave empty.
///
const std::vector<uint32_t> dexpo_viewport = {};
