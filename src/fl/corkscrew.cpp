#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/assert.h"
#include "fl/math.h"
#include "fl/splat.h"
#include "fl/warn.h"
#include "fl/tile2x2.h"
#include "fl/math_macros.h"
#include "fl/unused.h"

#define TWO_PI (PI * 2.0)

namespace fl {

namespace {

// New helper function to calculate individual LED position
vec2f calculateLedPositionExtended(uint16_t ledIndex, uint16_t numLeds, float totalTurns, float offsetCircumference, uint16_t width, uint16_t height) {
    // Calculate position along the corkscrew (0.0 to 1.0)
    FL_UNUSED(totalTurns);
    FL_UNUSED(offsetCircumference);
    FL_UNUSED(height);

    const float ledProgress = static_cast<float>(ledIndex) / static_cast<float>(numLeds - 1);
    
    // Calculate row (turn) using integer division
    const uint16_t row = ledIndex / width;
    
    // Calculate remainder as position within the row
    const uint16_t remainder = ledIndex % width;
    
    // Convert remainder to alpha value (0.0 to 1.0) against width
    const float alpha = static_cast<float>(remainder) / static_cast<float>(width);
    
    // Width position uses original calculation
    const float width_pos = ledProgress * numLeds;
    
    // Height is the row number plus the alpha interpolation
    const float height_pos = static_cast<float>(row) + alpha;
    
    return vec2f(width_pos, height_pos);
}

void generateState(const Corkscrew::Input &input, CorkscrewState *output) {
    output->width = input.calculateWidth();
    output->height = input.calculateHeight();
    // No longer pre-calculating mapping - positions computed on-the-fly
}
}  // namespace


Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    fl::generateState(mInput, &mState);
}

vec2f Corkscrew::at_exact(uint16_t i) const {
    if (i >= mInput.numLeds) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2f(0, 0);
    }
    
    // Compute position on-the-fly
    vec2f position = calculateLedPositionExtended(i, mInput.numLeds, mInput.totalTurns, 
                                         mInput.offsetCircumference, mState.width, mState.height);
    
    // Apply inversion if requested
    if (mInput.invert) {
        uint16_t invertedIndex = mInput.numLeds - 1 - i;
        position = calculateLedPositionExtended(invertedIndex, mInput.numLeds, mInput.totalTurns, 
                                       mInput.offsetCircumference, mState.width, mState.height);
    }

    // now wrap the x-position
    position.x = fmodf(position.x, static_cast<float>(mState.width));
    
    return position;
}


Tile2x2_u8 Corkscrew::at_splat_extrapolate(float i) const {
    if (i >= mInput.numLeds) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        FASTLED_ASSERT(false, "Out of bounds access in Corkscrew at_splat: "
                                  << i << " size: " << mInput.numLeds);
        return Tile2x2_u8();
    }
    
    // Use the splat function to convert the vec2f to a Tile2x2_u8
    float i_floor = floorf(i);
    float i_ceil = ceilf(i);
    if (ALMOST_EQUAL_FLOAT(i_floor, i_ceil)) {
        // If the index is the same, just return the splat of that index
        vec2f position = at_exact(static_cast<uint16_t>(i_floor));
        return splat(position);
    } else {
        // Interpolate between the two points and return the splat of the result
        vec2f pos1 = at_exact(static_cast<uint16_t>(i_floor));
        vec2f pos2 = at_exact(static_cast<uint16_t>(i_ceil));

        if (pos2.x < pos1.x) {
            // If the next point is on the other side of the cylinder, we need
            // to wrap it around and bring it back into the positive direction so we can construct a Tile2x2_u8 wrap with it.
            pos2.x += mState.width;
        }

        vec2f interpolated_pos =
            pos1 * (1.0f - (i - i_floor)) + pos2 * (i - i_floor);
        return splat(interpolated_pos);
    }
}

size_t Corkscrew::size() const { return mInput.numLeds; }

Corkscrew::State Corkscrew::generateState(const Corkscrew::Input &input) {
    CorkscrewState output;
    fl::generateState(input, &output);
    return output;
}



Tile2x2_u8_wrap Corkscrew::at_wrap(float i) const {
    // This is a splatted pixel, but wrapped around the cylinder.
    // This is useful for rendering the corkscrew in a cylindrical way.
    Tile2x2_u8 tile = at_splat_extrapolate(i);
    return Tile2x2_u8_wrap(tile, mState.width, mState.height);
}

// Iterator implementation
vec2f CorkscrewState::iterator::operator*() const {
    return corkscrew_->at_exact(static_cast<uint16_t>(position_));
}

} // namespace fl