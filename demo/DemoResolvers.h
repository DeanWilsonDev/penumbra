#pragma once

#include "DemoTheme.h"

#include "Penumbra/Widgets/Styles.h"

namespace Demo {

// Resolvers map semantic intent → concrete Penumbra style. This is what the
// UmbraComponentLibrary will own later. Penumbra contains none of this.
Penumbra::Widgets::BoxStyle      ResolvePanelStyle(const Theme&);
Penumbra::Widgets::ButtonStyle   ResolvePrimaryButtonStyle(const Theme&);
Penumbra::Widgets::CheckboxStyle ResolveCheckboxStyle(const Theme&);
Penumbra::Widgets::BoxStyle      ResolveInputFieldStyle(const Theme&);

} // namespace Demo
