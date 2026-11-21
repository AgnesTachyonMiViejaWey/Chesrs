#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/AppDelegate.hpp>

using namespace geode::prelude;

// Mientras sea true, el juego NO escribe el archivo de guardado.
// Todas las compras se hacen en memoria y se guarda una sola vez al final.
static bool g_blockSave = false;

class $modify(DeferSaveDelegate, AppDelegate) {
    void trySaveGame(bool p0) {
        if (g_blockSave) {
            log::info("Buy All Shops: guardado pospuesto");
            return;
        }
        AppDelegate::trySaveGame(p0);
    }
};

class $modify(BuyAllMenu, MenuLayer) {
    struct Fields {
        int bought = 0;
        int skippedFunds = 0;
        int alreadyOwned = 0;
    };

    bool init() {
        if (!MenuLayer::init()) return false;

        auto spr = ButtonSprite::create("Comprar todo", "goldFont.fnt", "GJ_button_01.png", .7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BuyAllMenu::onBuyAll));
        btn->setID("buy-all-shops-btn");

        auto menu = CCMenu::create();
        menu->setID("buy-all-shops-menu");
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({75.f, win.height - 25.f});
        this->addChild(menu, 100);
        return true;
    }

    void onBuyAll(CCObject*) {
        Ref<BuyAllMenu> self = this;
        createQuickPopup(
            "Comprar todo",
            "Voy a comprar <cy>todo lo que te alcance</c> en todas las tiendas, "
            "gastando orbes y diamantes. ¿Seguir?",
            "No", "Sí",
            [self](auto, bool yes) {
                if (yes) self->buyEverything();
            }
        );
    }

    void buyEverything() {
        auto f = m_fields.self();
        f->bought = 0;
        f->skippedFunds = 0;
        f->alreadyOwned = 0;

        auto gsm = GameStatsManager::get();

        // Reunir todos los items de todas las tiendas.
        std::vector<GJStoreItem*> items;
        for (auto [id, item] : CCDictionaryExt<int, GJStoreItem*>(gsm->m_storeItems)) {
            if (item) items.push_back(item);
        }
        log::info("Buy All Shops: {} items en total", items.size());

        if (items.empty()) {
            FLAlertLayer::create("Buy All Shops", "No encontre items de tienda.", "OK")->show();
            return;
        }

        // Los mas baratos primero: asi compras la mayor cantidad posible.
        std::sort(items.begin(), items.end(), [](GJStoreItem* a, GJStoreItem* b) {
            return a->m_price < b->m_price;
        });

        // Todo en memoria, sin escribir el archivo hasta el final.
        g_blockSave = true;
        for (auto item : items) {
            if (gsm->isStoreItemUnlocked(item->m_index)) {
                f->alreadyOwned++;
                continue;
            }
            if (!gsm->canAfford(item)) {
                f->skippedFunds++;
                continue;
            }
            gsm->purchaseItem(item);
            f->bought++;
        }

        // Un solo guardado, despues de la espera configurada.
        this->scheduleOnce(
            schedule_selector(BuyAllMenu::finish),
            static_cast<float>(Mod::get()->getSettingValue<double>("save-delay"))
        );
    }

    void finish(float) {
        auto f = m_fields.self();
        g_blockSave = false;
        AppDelegate::get()->trySaveGame(false);
        log::info("Buy All Shops: guardado hecho");

        FLAlertLayer::create(
            "Buy All Shops",
            fmt::format(
                "Compradas: {}\nSin fondos: {}\nYa las tenias: {}",
                f->bought, f->skippedFunds, f->alreadyOwned
            ),
            "OK"
        )->show();
    }
};
