#include <cstdint>
#include <vector>

/*
  Width of the screenshots that will be displayed
  Will be calculated based on height and screen ratio if set to zero

  Actual window width will be equal to dexpo_width + (dexpo_padding * 2)
*/
const int dexpo_width = 0;

/*
  Height of the screenshots that will be displayed
  Will be calculated based on width and screen ratio if set to zero

  Actual window height will be equal to dexpo_height + (dexpo_padding * 2)
*/
const int dexpo_height = 150;

/*
  Padding between the screenshots and window border
*/
const int dexpo_padding = 5;

/*
  Color of the border around preselected desktop
*/
const unsigned int dexpo_hlcolor = 0XFF662C28;

/*
  Width of the border around preselected desktop
*/
const int dexpo_hlwidth = 3;

/*
  Background color. TODO Alpha doesn't work
*/
const unsigned int dexpo_bgcolor = 0xAA525252;

/*
  Coordinates of dexpo's top left corner
*/
const int dexpo_x = 0;
const int dexpo_y = 0;

/*
  Border outside of GUI window
*/
const unsigned int dexpo_outer_border = 0;

/*
  Desktop viewport

  Top left coordinates of each of your desktops in the format
  { x1, y1, x2, y2, x3, y3 } and so on

  Should be set only if you have a DE or
   _NET_DESKOP_VIEWPORT doesn't output coordinates of all desktops
*/
const std::vector<uint32_t> dexpo_viewport = {};

/*
  Timeout at which screenshots will be made
*/
const float dexpo_screenshot_timeout = 10;
