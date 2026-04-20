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
        setDescription("Smooths visuals by predicting movement between physics ticks.");
        setDisabled(false);
        // Added the missing message back in
        setDisabledMessage("Frame extrapolation is temporarily disabled due to bugs");
    }
};

SUBMIT_HACK(FrameExtrapolation)

class $modify(ExtrapolatedGameLayer, GJBaseGameLayer) {
    struct Fields {
        float m_timeTilNextTick = 0.f;
        float m_progressTilNextTick = 0.f;
        CCPoint m_lastCamPos = {0, 0};
        CCPoint m_lastCamPos2 = {0, 0};
        float m_lastDelta = 0.f;
        bool m_shouldReset = false;
    };

    float getModifiedDelta(float dt) {
        auto pRet = GJBaseGameLayer::getModifiedDelta(dt);
        m_fields->m_lastDelta = pRet;
        return pRet;
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);

        auto mod = FrameExtrapolation::get();
        if (!typeinfo_cast<PlayLayer*>(this) || !mod->getRealEnabled() || m_isPaused) {
            return;
        }

        auto f = m_fields.self();

        // BUG PATCH: Detect state resets (like resets or portals) to prevent "snapping"
        if (dt > 0.5f || dt <= 0.0f) {
            f->m_shouldReset = true;
            return;
        }

        if (f->m_lastDelta != 0) {
            f->m_timeTilNextTick = f->m_lastDelta;
            f->m_progressTilNextTick = 0;
            f->m_lastCamPos2 = f->m_lastCamPos;
            f->m_lastCamPos = m_objectLayer->getPosition();
            f->m_shouldReset = false;
        } else {
            f->m_progressTilNextTick += dt;
        }

        // Safety exit for invalid timing
        if (f->m_timeTilNextTick <= 0 || f->m_shouldReset) return;

        // BUG PATCH: Clamp percent between 0 and 1. 
        // This prevents the "Forward Launch" bug during lag spikes.
        float percent = std::clamp(f->m_progressTilNextTick / f->m_timeTilNextTick, 0.0f, 1.0f);

        // Extrapolate Object Layer
        CCPoint endCamPos = f->m_lastCamPos + (f->m_lastCamPos - f->m_lastCamPos2);
        m_objectLayer->setPosition({
            std::lerp(f->m_lastCamPos.x, endCamPos.x, percent),
            std::lerp(f->m_lastCamPos.y, endCamPos.y, percent)
        });

        // Extrapolate Ground & Players
        extrapolateGround(m_groundLayer, percent, f);
        if (m_groundLayer2) extrapolateGround(m_groundLayer2, percent, f);

        extrapolatePlayer(m_player1, percent);
        if (m_player2) extrapolatePlayer(m_player2, percent);
    }

    void extrapolatePlayer(PlayerObject* player, float percent) {
        if (!player || player->m_isDead) return;

        // Prediction: (CurrentPos - LastPos) gives us the velocity vector
        float endXPos = player->m_position.x + (player->m_position.x - player->m_lastPosition.x);
        float endYPos = player->m_position.y + (player->m_position.y - player->m_lastPosition.y);

        player->CCNode::setPosition({
            std::lerp(player->m_position.x, endXPos, percent),
            std::lerp(player->m_position.y, endYPos, percent)
        });

        // Smooth Rotation - prevents the "stiff" icon look
        if (player->m_mainLayer) {
            float rotVel = player->m_rotation - player->m_lastRotation;
            player->m_mainLayer->setRotation(player->m_rotation + (rotVel * percent));
        }
    }

    void extrapolateGround(GJGroundLayer* ground, float percent, Fields* f) {
        if (!ground) return;
        float moveBy = (f->m_lastCamPos.x - f->m_lastCamPos2.x);
        
        for (auto child : CCArrayExt<CCNode*>(ground->getChildren())) {
            if (typeinfo_cast<CCSpriteBatchNode*>(child)) {
                child->setPositionX(std::lerp(0.0f, moveBy, percent));
            }
        }
    }
};
