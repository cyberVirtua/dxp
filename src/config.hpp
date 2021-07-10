#include "keynames.hpp"
#include <chrono>
#include <cstdint>
#include <vector>

using color = const uint32_t;

using keys_for = const std::vector<int>;

///
/// Dimensions of the screenshots that will be displayed.
///
/// Specifying width will stack desktops vertically.
/// Specifying height will stack desktops horizontally.
///
/// One dimension *must* be set to zero and will be calculated at runtime.
///
/// Actual window width will be equal to dxp_width + (dxp_padding * 2)
///
const uint dxp_width = 0;
const uint dxp_height = 150;

const int16_t dxp_x = 0; ///< X coordinate of window's top left corner
const int16_t dxp_y = 0; ///< Y coordinate of window's top left corner

const uint16_t dxp_padding = 3; ///< Padding around the screenshots

color dxp_background = 0x444444;    ///< Window background
color dxp_border_pres = 0xFFFFFF;   ///< Desktop preselection
color dxp_border_nopres = 0x808080; ///< Desktop without preselection

const uint16_t dxp_border_pres_width = 2; ///< Width of the preselection

const uint dxp_border_width = 0; ///< dxp window border

///
/// How often to take screenshots.
///
/// Recommended value is 10 seconds
//
/// NOTE: Setting timeout to values below 5 ms may severely increase CPU load.
///
const auto dxp_screenshot_period = std::chrono::seconds (1);

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
const std::vector<uint32_t> dxp_viewport = {};

///
/// Key settings:
///
/// Next - preselect next desktop
/// (Right & down arrows by default)
///
/// Prev - preselect previous desktop
/// (Left & up arrows by default)
///
/// Exit - kill dxp
/// (Escape by default)
///
/// Choose - select desktop
/// (Enter by deafault)
///
/// View keynames.hpp for a list of supported keys
/// and their names

keys_for action_next = { Down, Right, latin_l, latin_j };

keys_for action_prev = { Left, Up, latin_h, latin_k };

keys_for action_exit = { Escape };

keys_for action_select = { Return };
