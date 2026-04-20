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
        setDescription("Smooths between frames by predicting positions. Patched for GD 2.2.");
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
        
        // Storage for rotation since m_lastRotation can be unreliable in 2.2
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
        // Run original game logic first
        GJBaseGameLayer::update(dt);

        if (!typeinfo_cast<PlayLayer*>(this)) return;
        if (!FrameExtrapolation::get()->getRealEnabled()) return;
        if (m_isPaused || dt <= 0) return;

        auto f = m_fields.self();

        // Handle Tick Detection
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

        // Camera / Object Layer Extrapolation
        float camDiffX = f->m_lastCamPos.x - f->m_lastCamPos2.x;
        float camDiffY = f->m_lastCamPos.y - f->m_lastCamPos2.y;
        float endCamX = f->m_lastCamPos.x + camDiffX;
        float endCamY = f->m_lastCamPos.y + camDiffY;

        m_objectLayer->setPositionX(lerpVal(f->m_lastCamPos.x, endCamX, percent));
        m_objectLayer->setPositionY(lerpVal(f->m_lastCamPos.y, endCamY, percent));

        // Ground Sync
        if (m_groundLayer) extrapolateGround(m_groundLayer, percent, camDiffX);
        if (m_groundLayer2) extrapolateGround(m_groundLayer2, percent, camDiffX);

        // Player Extrapolation
        extrapolatePlayer(m_player1, percent, f->m_p1LastRot);
        if (m_player2) extrapolatePlayer(m_player2, percent, f->m_p2LastRot);

        // Update stored rotation for the next tick
        if (m_player1) f->m_p1LastRot = m_player1->getRotation();
        if (m_player2) f->m_p2LastRot = m_player2->getRotation();
    }

    void extrapolatePlayer(PlayerObject* player, float percent, float lastRot) {
        if (!player || player->m_isDead) return;

        float diffX = player->m_position.x - player->m_lastPosition.x;
        float diffY = player->m_position.y - player->m_lastPosition.y;
        
        if (std::abs(diffX) > 100.0f) return;

        float endX = player->m_position.x + diffX;
        float endY = player->m_position.y + diffY;

        player->CCNode::setPosition({ 
            lerpVal(player->m_position.x, endX, percent), 
            lerpVal(player->m_position.y, endY, percent) 
        });

        // Rotation Fix using CCNode methods
        if (player->m_mainLayer) {
            float currentRot = player->getRotation();
            float rotDiff = currentRot - lastRot;
            player->m_mainLayer->setRotation(currentRot + (rotDiff * percent));
        }
    }

    void extrapolateGround(GJGroundLayer* ground, float percent, float moveDelta) {
        for (auto child : CCArrayExt<CCNode*>(ground->getChildren())) {
            if (typeinfo_cast<CCSpriteBatchNode*>(child)) {
                child->setPositionX(lerpVal(0.0f, moveDelta, percent));
            }
        }
    }
};
