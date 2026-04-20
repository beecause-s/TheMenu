#include "../../Client/Module.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class FrameExtrapolation : public Module {
public:
    MODULE_SETUP(FrameExtrapolation) {
        setName("Frame Extrapolation");
        setID("frame-extrapolation");
        setCategory("Universal");
        setDescription("Mobile-optimized smoothing. Patched for Android lag.");
        setDisabled(false);
    }
};

SUBMIT_HACK(FrameExtrapolation)

class $modify(ExtrapolatedGameLayer, GJBaseGameLayer) {
    struct Fields {
        float m_timeTilNextTick = 0.0f;
        float m_progressTilNextTick = 0.0f;
        CCPoint m_lastCamPos = {0, 0};
        CCPoint m_lastCamPos2 = {0, 0};
        CCPoint m_p1LastPos = {0, 0};
        float m_p1LastRot = 0.0f;
    };

    inline float fastLerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    virtual void update(float dt) {
        GJBaseGameLayer::update(dt);

        // FIX: Cast to PlayLayer to check for m_isPaused
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || pl->m_isPaused || dt <= 0 || dt > 0.1f) return;
        
        if (!FrameExtrapolation::get()->getRealEnabled()) return;

        auto f = m_fields.self();
        CCPoint currentCam = m_objectLayer->getPosition();
        
        if (currentCam != f->m_lastCamPos) {
            f->m_timeTilNextTick = dt; 
            f->m_progressTilNextTick = 0;
            f->m_lastCamPos2 = f->m_lastCamPos;
            f->m_lastCamPos = currentCam;
        } else {
            f->m_progressTilNextTick += dt;
        }

        if (f->m_timeTilNextTick < 0.002f || f->m_timeTilNextTick > 0.025f) return;

        float percent = f->m_progressTilNextTick / f->m_timeTilNextTick;
        if (percent > 1.0f) percent = 1.0f;

        float diffX = f->m_lastCamPos.x - f->m_lastCamPos2.x;
        float diffY = f->m_lastCamPos.y - f->m_lastCamPos2.y;
        
        if (std::abs(diffX) < 100.0f) {
            m_objectLayer->setPositionX(fastLerp(f->m_lastCamPos.x, f->m_lastCamPos.x + diffX, percent));
            m_objectLayer->setPositionY(fastLerp(f->m_lastCamPos.y, f->m_lastCamPos.y + diffY, percent));
        }

        if (m_player1 && !m_player1->m_isDead) {
            CCPoint pPos = m_player1->getPosition();
            float pDiffX = pPos.x - f->m_p1LastPos.x;
            float pDiffY = pPos.y - f->m_p1LastPos.y;

            if (std::abs(pDiffX) < 50.0f) {
                m_player1->CCNode::setPosition({ 
                    fastLerp(pPos.x, pPos.x + pDiffX, percent), 
                    fastLerp(pPos.y, pPos.y + pDiffY, percent) 
                });

                float curRot = m_player1->getRotation();
                float rDiff = curRot - f->m_p1LastRot;
                if (std::abs(rDiff) < 180.0f) {
                    m_player1->setRotation(curRot + (rDiff * percent));
                }
            }
            f->m_p1LastPos = pPos;
            f->m_p1LastRot = m_player1->getRotation();
        }
    }
};
