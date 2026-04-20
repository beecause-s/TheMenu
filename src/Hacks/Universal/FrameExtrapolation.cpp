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
        setDescription("Smooths between frames by predicting movement.");
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
        float m_lastModifiedDelta = 0.0f;
        
        CCPoint m_p1LastPos = {0, 0};
        CCPoint m_p2LastPos = {0, 0};
        float m_p1LastRot = 0.0f;
        float m_p2LastRot = 0.0f;
    };

    float lerpVal(float a, float b, float t) {
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
        return a + t * (b - a);
    }

    float getModifiedDelta(float dt) {
        auto pRet = GJBaseGameLayer::getModifiedDelta(dt);
        m_fields->m_lastModifiedDelta = pRet;
        return pRet;
    }

    virtual void update(float dt) {
        GJBaseGameLayer::update(dt);

        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl) return;
        
        auto mod = FrameExtrapolation::get();
        if (!mod || !mod->getRealEnabled()) return;

        // FIX: Use the casted PlayLayer to check for m_isPaused
        if (pl->m_isPaused || dt <= 0) return;

        auto f = m_fields.self();

        if (f->m_lastModifiedDelta != 0) {
            f->m_timeTilNextTick = f->m_lastModifiedDelta;
            f->m_progressTilNextTick = 0;
            f->m_lastCamPos2 = f->m_lastCamPos;
            f->m_lastCamPos = m_objectLayer->getPosition();
        } else {
            f->m_progressTilNextTick += dt;
        }

        if (f->m_timeTilNextTick <= 0) return;
        float percent = f->m_progressTilNextTick / f->m_timeTilNextTick;

        // Camera
        float camDiffX = f->m_lastCamPos.x - f->m_lastCamPos2.x;
        float camDiffY = f->m_lastCamPos.y - f->m_lastCamPos2.y;
        
        m_objectLayer->setPositionX(lerpVal(f->m_lastCamPos.x, f->m_lastCamPos.x + camDiffX, percent));
        m_objectLayer->setPositionY(lerpVal(f->m_lastCamPos.y, f->m_lastCamPos.y + camDiffY, percent));

        // Ground
        if (m_groundLayer) extrapolateGround(m_groundLayer, percent, camDiffX);
        if (m_groundLayer2) extrapolateGround(m_groundLayer2, percent, camDiffX);

        // Players
        if (m_player1) extrapolatePlayer(m_player1, percent, f->m_p1LastPos, f->m_p1LastRot);
        if (m_player2) extrapolatePlayer(m_player2, percent, f->m_p2LastPos, f->m_p2LastRot);

        // Save states
        if (m_player1) {
            f->m_p1LastPos = m_player1->getPosition();
            f->m_p1LastRot = m_player1->getRotation();
        }
        if (m_player2) {
            f->m_p2LastPos = m_player2->getPosition();
            f->m_p2LastRot = m_player2->getRotation();
        }
    }

    void extrapolatePlayer(PlayerObject* player, float percent, CCPoint lastPos, float lastRot) {
        if (!player || player->m_isDead) return;

        CCPoint currentPos = player->getPosition();
        float diffX = currentPos.x - lastPos.x;
        float diffY = currentPos.y - lastPos.y;
        
        if (std::abs(diffX) > 100.0f) return;

        player->CCNode::setPosition({ 
            lerpVal(currentPos.x, currentPos.x + diffX, percent), 
            lerpVal(currentPos.y, currentPos.y + diffY, percent) 
        });

        float currentRot = player->getRotation();
        float rotDiff = currentRot - lastRot;
        
        if (auto visual = player->getChildByID("main-layer")) {
            visual->setRotation(currentRot + (rotDiff * percent));
        } else {
            player->setRotation(currentRot + (rotDiff * percent));
        }
    }

    void extrapolateGround(GJGroundLayer* ground, float percent, float moveDelta) {
        if (!ground || !ground->getChildren()) return;
        auto children = ground->getChildren();
        for (int i = 0; i < children->count(); ++i) {
            auto child = static_cast<CCNode*>(children->objectAtIndex(i));
            if (typeinfo_cast<CCSpriteBatchNode*>(child)) {
                child->setPositionX(lerpVal(0.0f, moveDelta, percent));
            }
        }
    }
};
