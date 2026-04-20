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
        setDescription("Smooths visuals by predicting movement. Fixed implementation.");
        setDisabled(false);
        setDisabledMessage("Frame extrapolation is temporarily disabled due to bugs");
    }
};

SUBMIT_HACK(FrameExtrapolation)

class $modify(ExtrapolatedGameLayer, GJBaseGameLayer) {
    struct Fields {
        float m_timeTilNextTick = 0.0f;
        float m_progressTilNextTick = 0.0f;
        CCPoint m_lastCamPos = {0, 0};
        CCPoint m_lastCamPos2 = {0, 0};
        float m_lastDelta = 0.0f;
    };

    // Manual lerp for compatibility with older C++ standards
    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float getModifiedDelta(float dt) {
        auto pRet = GJBaseGameLayer::getModifiedDelta(dt);
        m_fields->m_lastDelta = pRet;
        return pRet;
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (!typeinfo_cast<PlayLayer*>(this)) return;
        if (!FrameExtrapolation::get()->getRealEnabled()) return;
        if (m_isPaused || dt <= 0) return;

        auto f = m_fields.self();

        if (f->m_lastDelta != 0) {
            f->m_timeTilNextTick = f->m_lastDelta;
            f->m_progressTilNextTick = 0;
            f->m_lastCamPos2 = f->m_lastCamPos;
            f->m_lastCamPos = m_objectLayer->getPosition();
        } else {
            f->m_progressTilNextTick += dt;
        }

        if (f->m_timeTilNextTick <= 0) return;

        float percent = f->m_progressTilNextTick / f->m_timeTilNextTick;
        if (percent > 1.0f) percent = 1.0f;
        if (percent < 0.0f) percent = 0.0f;

        // Camera Extrapolation
        float endX = f->m_lastCamPos.x + (f->m_lastCamPos.x - f->m_lastCamPos2.x);
        float endY = f->m_lastCamPos.y + (f->m_lastCamPos.y - f->m_lastCamPos2.y);
        
        m_objectLayer->setPositionX(lerp(f->m_lastCamPos.x, endX, percent));
        m_objectLayer->setPositionY(lerp(f->m_lastCamPos.y, endY, percent));

        // Player Extrapolation
        if (m_player1 && !m_player1->m_isDead) {
            float pEndX = m_player1->m_position.x + (m_player1->m_position.x - m_player1->m_lastPosition.x);
            float pEndY = m_player1->m_position.y + (m_player1->m_position.y - m_player1->m_lastPosition.y);
            m_player1->CCNode::setPosition({ lerp(m_player1->m_position.x, pEndX, percent), lerp(m_player1->m_position.y, pEndY, percent) });
        }

        if (m_player2 && !m_player2->m_isDead) {
            float pEndX2 = m_player2->m_position.x + (m_player2->m_position.x - m_player2->m_lastPosition.x);
            float pEndY2 = m_player2->m_position.y + (m_player2->m_position.y - m_player2->m_lastPosition.y);
            m_player2->CCNode::setPosition({ lerp(m_player2->m_position.x, pEndX2, percent), lerp(m_player2->m_position.y, pEndY2, percent) });
        }
    }
};
