#pragma once

#include "Animation/AnimationDiagnostics.h"

#ifndef ANIMATION_DEBUG
#define ANIMATION_DEBUG 0
#endif

#if ANIMATION_DEBUG
#define ANIMATION_DEBUG_LOG(x) ANIM_LOG(x)
#define ANIMATION_DEBUG_FLUSH() ANIM_LOG_FLUSH()
#else
#define ANIMATION_DEBUG_LOG(x) do {} while (0)
#define ANIMATION_DEBUG_FLUSH() do {} while (0)
#endif

