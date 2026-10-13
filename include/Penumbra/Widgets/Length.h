#pragma once

#include "Penumbra/Geometry.h"

namespace Penumbra::Widgets {

enum class LengthUnit { Logical, Percent, ViewportWidth, ViewportHeight };

struct Length {
    float      Value{-1.0f};
    LengthUnit Unit{LengthUnit::Logical};

    static Length Logical(float Value) { return {Value, LengthUnit::Logical}; }
    static Length Percent(float Value) { return {Value, LengthUnit::Percent}; }
    static Length ViewportWidth(float Value) { return {Value, LengthUnit::ViewportWidth}; }
    static Length ViewportHeight(float Value) { return {Value, LengthUnit::ViewportHeight}; }

    bool  IsSet() const { return Value >= 0.0f; }
    float Resolve(float PercentBasisLogical) const;

    friend bool operator==(const Length&, const Length&) = default;
};

void  SetLayoutViewportLogical(Point SizeLogical);
Point GetLayoutViewportLogical();

} // namespace Penumbra::Widgets
