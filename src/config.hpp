// Displayed Screenshot width.
// Will be calculated based on height and screen ratio if set to zero
#include <cstdint>
#include <vector>

const int dexpo_width = 0;
// Displayed screenshot height.
// Will be calculated based on width and screen ratio if set to zero
const int dexpo_height = 300;
// Displayed interval between screenshots
const int dexpo_padding = 10;
// Highlight color
// Red (0xFFFF0000) recommended
const unsigned int dexpo_hlcolor = 0xFFFF0000;
// Higlight border width
// Should not be less than 3 or more than 8
// Otherwise it will be poorly visible or buggy
const int dexpo_hlwidth = 3;
// Background color
// White (0xFFFFFFFF) recommended
const unsigned int dexpo_bgcolor = 0xFFFFFFFF;
// Coordinates of GUI's top left corner
const int dexpo_x = 1080;
const int dexpo_y = 0;
// Border outside of GUI window
const unsigned int dexpo_outer_border = 0;
// Desktop viewport
const std::vector<uint32_t> dexpo_viewport = {};
