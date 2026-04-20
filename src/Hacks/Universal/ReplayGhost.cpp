#include "../../Client/Module.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class GhostIcon : public Module {
public:
    MODULE_SETUP(GhostIcon) {
        setName("Ghost Icon");
        setID("ghost-icon");
        setCategory("Universal");
        setDescription("Plays back your previous attempt as a transparent ghost.");
        setDisabled(false);
        // Added the message in case you need to lock it later
        setDisabledMessage("Ghost Icon is currently disabled.");
    }
};

SUBMIT_HACK(GhostIcon)

class $modify(GhostLayer, PlayLayer) {
    struct Fields {
        std::vector<CCPoint> m_currentRunData;
        std::vector<CCPoint> m_lastRunData;
        CCSprite* m_ghostSprite = nullptr;
        size_t m_frameIndex = 0;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;
        
        if (GhostIcon::get()->getRealEnabled()) {
            // Android Tip: Using a simple circle or dot is better for performance than a full icon
            m_fields->m_ghostSprite = CCSprite::createWithSpriteFrameName("player_ball_001.png"); 
            if (m_fields->m_ghostSprite) {
                m_fields->m_ghostSprite->setOpacity(100);
                m_fields->m_ghostSprite->setScale(0.8f);
                m_fields->m_ghostSprite->setVisible(false);
                // We add it to the object layer so it moves with the camera
                m_objectLayer->addChild(m_fields->m_ghostSprite, 10);
            }
        }
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!GhostIcon::get()->getRealEnabled() || m_isPaused) return;

        // 1. Record current position
        // On Android, recording every frame is heavy. 
        // This is a basic version; we can optimize with 'ticks' later.
        m_fields->m_currentRunData.push_back(m_player1->getPosition());

        // 2. Playback previous run
        if (!m_fields->m_lastRunData.empty() && m_fields->m_frameIndex < m_fields->m_lastRunData.size()) {
            if (m_fields->m_ghostSprite) {
                m_fields->m_ghostSprite->setVisible(true);
                m_fields->m_ghostSprite->setPosition(m_fields->m_lastRunData[m_fields->m_frameIndex]);
                m_fields->m_frameIndex++;
            }
        } else if (m_fields->m_ghostSprite) {
            m_fields->m_ghostSprite->setVisible(false);
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        // Swap data: Current run becomes the Ghost for the next run
        if (!m_fields->m_currentRunData.empty()) {
            m_fields->m_lastRunData = m_fields->m_currentRunData;
        }
        
        m_fields->m_currentRunData.clear();
        m_fields->m_frameIndex = 0;
    }
};
