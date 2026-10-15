#pragma once

#include "mc/client/renderer/Tessellator.h"
#include <variant>

#if !defined(LAMIUM_SDK_26_20_7)
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#endif

namespace lamium::render {

#if defined(LAMIUM_SDK_26_20_7)
using SupplementaryFieldAutoGenerationMode = Tessellator::SupplementaryFieldAutoGenerationMode;
using OffscreenCaptureDescription = std::monostate;
#else
using SupplementaryFieldAutoGenerationMode = ::SupplementaryFieldAutoGenerationMode;
using OffscreenCaptureDescription = ::OffscreenCaptureDescription;
#endif

}
