#include <Geode/Geode.hpp>
#include <Geode/modify/SecretRewardsLayer.hpp>
#include <Geode/modify/AppDelegate.hpp>

using namespace geode::prelude;

// Mientras sea true, el juego NO escribe el archivo de guardado.
// Los cofres se abren y gastan llaves normal (en memoria), y el guardado
// se hace despues una sola vez, con calma.
static bool g_blockSave = false;

class $modify(DeferSaveDelegate, AppDelegate) {
    void trySaveGame(bool p0) {
        if (g_blockSave) {
            log::info("Open All Chests: guardado pospuesto");
            return;
        }
        AppDelegate::trySaveGame(p0);
    }
};

static void flushSave() {
    g_blockSave = false;
    AppDelegate::get()->trySaveGame(false);
    log::info("Open All Chests: guardado hecho");
}

// Recoge todos los botones de cofre del Treasure Room (sin el propio ni el de volver).
static void collectButtons(CCNode* node, std::vector<CCMenuItemSpriteExtra*>& out) {
    if (!node) return;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(child)) {
            if (btn->getID() == "open-all-btn") continue;
            if (btn->getID() == "back-button") continue;
            out.push_back(btn);
        }
        collectButtons(child, out);
    }
}

class $modify(OpenAllChestsLayer, SecretRewardsLayer) {
    struct Fields {
        float waited = 0.f;
        bool waitingSave = false;
    };

    bool init(bool p0) {
        if (!SecretRewardsLayer::init(p0)) return false;

        auto spr = ButtonSprite::create("Abrir todos", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(OpenAllChestsLayer::onOpenAll));
        btn->setID("open-all-btn");

        auto menu = CCMenu::create();
        menu->setID("open-all-menu");
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width - 70.f, 25.f});
        this->addChild(menu, 100);
        return true;
    }

    void onOpenAll(CCObject*) {
        std::vector<CCMenuItemSpriteExtra*> found;
        collectButtons(this, found);
        log::info("Open All Chests: {} botones candidatos", found.size());

        if (found.empty()) {
            FLAlertLayer::create("Open All Chests", "No encontre cofres.", "OK")->show();
            return;
        }

        // Abrir todo de golpe, sin escribir el archivo todavia.
        g_blockSave = true;
        for (auto b : found) {
            if (b && b->isVisible() && b->isEnabled()) b->activate();
        }

        auto f = m_fields.self();
        f->waited = 0.f;
        f->waitingSave = false;
        this->schedule(schedule_selector(OpenAllChestsLayer::checkDone), 0.5f);
    }

    void checkDone(float dt) {
        auto f = m_fields.self();
        auto scene = CCDirector::get()->getRunningScene();

        // Esperar a que se cierren todos los popups de recompensa.
        if (scene && scene->getChildByType<RewardUnlockLayer>(0)) {
            f->waited = 0.f;
            return;
        }

        // Pausa extra antes del unico guardado.
        f->waited += dt;
        if (f->waited < Mod::get()->getSettingValue<double>("save-delay")) return;

        this->unschedule(schedule_selector(OpenAllChestsLayer::checkDone));
        flushSave();
    }

    void onBack(CCObject* sender) {
        // Si sales antes de tiempo, guarda ya para no perder nada.
        if (g_blockSave) flushSave();
        SecretRewardsLayer::onBack(sender);
    }
};
