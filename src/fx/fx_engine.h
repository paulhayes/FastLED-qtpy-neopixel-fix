#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/fx_compositing_engine.h"
#include "fx/fx_layer.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

class FxEngine {
  public:
    FxEngine(uint16_t numLeds);
    ~FxEngine();
    bool addFx(RefPtr<Fx> effect);
    void draw(uint32_t now, CRGB *outputBuffer);
    bool nextFx(uint32_t now, uint32_t duration);
    bool setNextFx(uint16_t index, uint32_t now, uint32_t duration);

  private:
    uint16_t mNumLeds;
    FixedVector<RefPtr<Fx>, FASTLED_FX_ENGINE_MAX_FX> mEffects;
    FxCompositingEngine mCompositor;
    bool mIsTransitioning;
    bool mWasTransitioning;
    uint16_t mCurrentIndex;
    uint16_t mNextIndex;
    Transition mTransition;
    void startTransition(uint32_t now, uint32_t duration);
};

inline FxEngine::FxEngine(uint16_t numLeds)
    : mNumLeds(numLeds), mCompositor(numLeds), mIsTransitioning(false), mCurrentIndex(0),
      mNextIndex(0) {
}

inline FxEngine::~FxEngine() {}

inline bool FxEngine::addFx(RefPtr<Fx> effect) {
    if (mEffects.size() >= FASTLED_FX_ENGINE_MAX_FX) {
        return false;
    }
    mEffects.push_back(effect);
    return true;
}

inline bool FxEngine::nextFx(uint32_t now, uint32_t duration) {
    uint16_t next_index = (mCurrentIndex + 1) % mEffects.size();
    return setNextFx(next_index, now, duration);
}

inline bool FxEngine::setNextFx(uint16_t index, uint32_t now, uint32_t duration) {
    if (index >= mEffects.size() && index != mCurrentIndex) {
        return false;
    }
    if (mIsTransitioning) {
        // If already transitioning, complete the current transition
        // immediately
        mEffects[mCurrentIndex]->pause();
        mCurrentIndex = mNextIndex;
        mIsTransitioning = false;
    }
    mNextIndex = index;
    mEffects[mNextIndex]->resume();
    mIsTransitioning = true;
    mTransition.start(now, duration);
    return true;
}

inline void FxEngine::draw(uint32_t now, CRGB *finalBuffer) {
    if (!mEffects.empty()) {
        Fx::DrawContext context = {now, mCompositor.mLayers[0]->surface.get()};
        mEffects[mCurrentIndex]->draw(context);

        if (!mIsTransitioning || mEffects.size() < 2) {
            memcpy(finalBuffer, mCompositor.mLayers[0]->surface.get(),
                   sizeof(CRGB) * mNumLeds);
            return;
        }

        context = {now, mCompositor.mLayers[1]->surface.get()};
        mEffects[mNextIndex]->draw(context);

        uint8_t progress = mTransition.getProgress(now);
        uint8_t inverse_progress = 255 - progress;

        const CRGB* surface0 = mCompositor.mLayers[0]->surface.get();
        const CRGB* surface1 = mCompositor.mLayers[1]->surface.get();

        for (uint16_t i = 0; i < mNumLeds; i++) {
            CRGB p0 = surface0[i];
            CRGB p1 = surface1[i];
            p0.nscale8(inverse_progress);
            p1.nscale8(progress);
            finalBuffer[i] = p0 + p1;
        }

        if (progress == 255) {
            // Transition complete, update current index
            mEffects[mCurrentIndex]->pause();
            mCurrentIndex = mNextIndex;
            mIsTransitioning = false;
        }
    }
}


FASTLED_NAMESPACE_END
