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
        setDescription("Smooths movement. Patched for Low TPS flickering and Rotation bugs.");
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

    virtual void update(float dt) {
        GJBaseGameLayer::update(dt);

        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || pl->m_isPaused || dt <= 0) return;
        
        auto mod = FrameExtrapolation::get();
        if (!mod || !mod->getRealEnabled()) return;

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

        // STABILITY PATCH: If TPS is lower than 30 (dt > 0.033), 
        // extrapolation is too inaccurate. Disable to prevent level disappearing.
        if (f->m_timeTilNextTick <= 0 || f->m_timeTilNextTick > 0.034f) return;

        float percent = f->m_progressTilNextTick / f->m_timeTilNextTick;
        if (percent > 1.0f) percent = 1.0f;

        // Camera Extrapolation
        float camDiffX = f->m_lastCamPos.x - f->m_lastCamPos2.x;
        float camDiffY = f->m_lastCamPos.y - f->m_lastCamPos2.y;
        
        // Safety: If camera jump is massive, don't extrapolate (prevents disappearing level)
        if (std::abs(camDiffX) < 150.0f) {
            m_objectLayer->setPositionX(lerpVal(f->m_lastCamPos.x, f->m_lastCamPos.x + camDiffX, percent));
            m_objectLayer->setPositionY(lerpVal(f->m_lastCamPos.y, f->m_lastCamPos.y + camDiffY, percent));
        }

        if (m_groundLayer) extrapolateGround(m_groundLayer, percent, camDiffX);
        if (m_groundLayer2) extrapolateGround(m_groundLayer2, percent, camDiffX);

        if (m_player1) extrapolatePlayer(m_player1, percent, f->m_p1LastPos, f->m_p1LastRot);
        if (m_player2) extrapolatePlayer(m_player2, percent, f->m_p2LastPos, f->m_p2LastRot);

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
        
        // Safety: Prevent teleporting icons
        if (std::abs(diffX) > 100.0f || std::abs(diffX) < 0.001f) return;

        player->CCNode::setPosition({ 
            lerpVal(currentPos.x, currentPos.x + diffX, percent), 
            lerpVal(currentPos.y, currentPos.y + diffY, percent) 
        });

        // HELICOPTER FIX: Handle rotation wrapping (360 -> 0)
        float currentRot = player->getRotation();
        float rotDiff = currentRot - lastRot;

        if (std::abs(rotDiff) > 180.0f) {
            // If the rotation jumped more than 180 degrees, it's a wrap-around.
            // Don't extrapolate rotation this frame to prevent the 360-spin glitch.
            rotDiff = 0;
        }

        if (auto visual = player->getChildByID("main-layer")) {
            visual->setRotation(currentRot + (rotDiff * percent));
        } else {
            player->setRotation(currentRot + (rotDiff * percent));
        }
    }

    void extrapolateGround(GJGroundLayer* ground, float percent, float moveDelta) {
        if (!ground || !ground->getChildren() || std::abs(moveDelta) > 150.0f) return;
        auto children = ground->getChildren();
        for (int i = 0; i < children->count(); ++i) {
            auto child = static_cast<CCNode*>(children->objectAtIndex(i));
            if (typeinfo_cast<CCSpriteBatchNode*>(child)) {
                child->setPositionX(lerpVal(0.0f, moveDelta, percent));
            }
        }
    }
};
