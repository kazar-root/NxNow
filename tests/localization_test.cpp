#include "localization.hpp"

#include <cassert>
#include <set>

int main()
{
    const auto& languages = nxnow::InterfaceLanguageOptions();
    assert(languages.size() == 8);

    std::set<std::string> codes;
    for (const auto& language : languages)
    {
        assert(!language.code.empty());
        assert(!language.label.empty());
        assert(codes.insert(language.code).second);

        nxnow::SetInterfaceLanguage(language.code);
        assert(nxnow::GetInterfaceLanguage() == language.code);
        assert(!nxnow::Tr("Settings").empty());
        assert(!nxnow::Tr("Store").empty());
        assert(!nxnow::Tr("Library").empty());
        assert(!nxnow::Tr("Language").empty());
        if (language.code != "en")
        {
            assert(nxnow::Tr("Loading...") != "Loading...");
            assert(nxnow::Tr("Next") != "Next");
            assert(nxnow::Tr("Previous") != "Previous");
        }
    }

    nxnow::SetInterfaceLanguage("unsupported");
    assert(nxnow::GetInterfaceLanguage() == "en");
    assert(nxnow::Tr("Untranslated technical message") ==
           "Untranslated technical message");
    nxnow::SetInterfaceLanguage("fr");
    assert(nxnow::Tr("Next stream") == "Prochain flux");
    assert(nxnow::Tr("Same as NxNow") == "Comme NxNow");
    for (const char* key : {"Guest", "No account", "Play", "Last played", "Last Played", "Last Added",
        "All", "Auto", "Change", "Create Switch shortcut", "New QR code", "Choose a login provider",
        "Display FPS", "Packet loss", "Cancel launch", "Loading game description...", "Enter"})
        assert(nxnow::Tr(key) != key);
    assert(nxnow::TrFormat("Account connected: {0}", {"Player"}) == "Compte connecté : Player");
    assert(nxnow::Tr("A/B and X/Y are automatically swapped only in World of Warcraft to play with Blizzard's official controller module. Other games keep the mapping selected in Settings.") ==
           "A/B et X/Y sont automatiquement inversés uniquement dans World of Warcraft pour jouer avec le module manette officiel de Blizzard. Les autres jeux conservent le mapping choisi dans les paramètres.");
    return 0;
}
