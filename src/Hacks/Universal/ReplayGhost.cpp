#include "../../Client/Module.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class GhostIcon : public Module {
public:
    MODULE_SETUP(GhostIcon) {
        setID("ghost-icon"); // This is the internal ID
        setName("Ghost Icon"); // This is the display name
        setCategory("Universal");
        setDescription("Plays back your previous attempt as a transparent ghost.");
        
        setDisabled(false);
        setDisabledMessage("Ghost Icon is temporarily disabled for maintenance.");
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
        
        auto mod = GhostIcon::get();
        if (mod && mod->getRealEnabled()) {
            // Using the official ghost sprite frame
            m_fields->m_ghostSprite = CCSprite::createWithSpriteFrameName("gj_ghostIcon_001.png");
            
            if (m_fields->m_ghostSprite) {
                m_fields->m_ghostSprite->setOpacity(130);
                m_fields->m_ghostSprite->setZOrder(10);
                m_fields->m_ghostSprite->setVisible(false);
                
                // Add to object layer so it moves with the world
                m_objectLayer->addChild(m_fields->m_ghostSprite);
            }
        }
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto mod = GhostIcon::get();
        if (!mod || !mod->getRealEnabled() || m_isPaused) return;

        // 1. Record current position
        if (m_player1) {
            m_fields->m_currentRunData.push_back(m_player1->getPosition());
        }

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

        // If we finished a run, save it as the ghost for the next attempt
        if (!m_fields->m_currentRunData.empty()) {
            m_fields->m_lastRunData = m_fields->m_currentRunData;
        }
        
        m_fields->m_currentRunData.clear();
        m_fields->m_frameIndex = 0;
    }
};
