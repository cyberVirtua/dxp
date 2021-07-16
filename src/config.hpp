#ifndef DXP_CONFIG_HPP
#define DXP_CONFIG_HPP

#include "keys.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

constexpr std::size_t keys_size = 10;
using color = const uint32_t;
using keys = std::array<std::string_view, keys_size>;

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
const uint dxp_height = 175;

///
/// Name of the monitor to put window on. Will use primary if unset.
///
const std::string dxp_monitor_name = "";

///
/// Prepending minus to the coordinate value will switch the corner side:
///
/// +x, +y: top left corner
/// +x, -y: bottom left corner
/// -x, +y: top right corner
/// -x, -y: bottom right corner
///
/// Coordinates are relative to the specified monitor.
///
/// NOTE: To specify -0, write = -0.0
///
const float dxp_x = 0; ///< X coordinate of window's corner
const float dxp_y = 0; ///< Y coordinate of window's corner

///
/// If centering is enabled, corresponding coordinate will be ignored
///
const bool dxp_center_x = true;  ///< Center dxp on the monitor horizontally
const bool dxp_center_y = true; ///< Center dxp on the monitor vertically

const uint16_t dxp_padding = 3; ///< Padding around the screenshots

color dxp_background = 0x444444;    ///< Window background
color dxp_border_pres = 0xFFFFFF;   ///< Desktop preselection
color dxp_border_nopres = 0x808080; ///< Desktop without preselection

const uint16_t dxp_border_pres_width = 2; ///< Width of the preselection

const uint dxp_border_width = 0; ///< dxp window border

///
/// How often to take screenshots.
///
/// Recommended value is 10 seconds.
///
/// NOTE: Setting timeout to values below 5 ms may severely increase CPU load.
///
const auto dxp_screenshot_period = std::chrono::seconds (2);

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
const std::vector<uint32_t> dxp_viewport = {0,0,0,0,0,0,0,0};

///
/// Key bindings:
///
/// next: preselect next desktop
/// prev: preselect previous desktop
/// exit: kill dxp
/// slct: switch to preselected desktop
///
/// View keynames.hpp for a list of supported keys
/// and their names
///
struct dxp_keys
{
  static constexpr keys next = { "Down", "Right", "l", "j" };
  static constexpr keys prev = { "Left", "Up", "h", "k" };
  static constexpr keys slct = { "Return" };
  static constexpr keys exit = { "Escape", "Cyrillic_hardsign" };
};

#endif /* ifndef DXP_CONFIG_HPP */
