#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

//==============================================================================
//  Les presets d'un plugin ONIRIQUE bâti sur un AudioProcessorValueTreeState : ceux d'usine, écrits dans le code,
//  ceux de l'utilisateur, un fichier chacun, les favoris, la comparaison A/B et le presse-papier.
//  La barre « Nuit » (OniriqueBar.h) les montre.
//
//  Ce fichier se copie tel quel dans Source/ui/ de chaque plugin qui a la barre Nuit.
//
//      PresetBank presets { apvts, "Weight", stockPresets(), { "Vocal", "Drums", "Bass" } };
//      getStateInformation : auto state = apvts.copyState(); presets.writeState (state); ...
//      setStateInformation : auto state = ValueTree::fromXml (*xml); presets.readState (state); apvts.replaceState (state);
//==============================================================================
namespace onirique
{

class PresetBank
{
public:
    struct Stock
    {
        juce::String name;
        int tags;                                               // des bits, dans l'ordre des noms de tags
        std::vector<std::pair<juce::String, float>> values;     // ce qui diffère des réglages par défaut
    };

    enum Source { anySource, stockOnly, userOnly };

    /** `notStored` : les paramètres qu'un preset laisse tels quels, ceux qui disent comment on écoute le plugin
        plutôt que ce qu'il fait au son. */
    PresetBank (juce::AudioProcessorValueTreeState& s, const juce::String& pluginName, std::vector<Stock> stockPresets,
                juce::StringArray tags, juce::StringArray notStored = { "bypass" })
        : state (s), plugin (pluginName), stock (std::move (stockPresets)), tagNames (std::move (tags)), left (std::move (notStored))
    {
        reloadFavourites();
    }

    //==============================================================================
    const juce::String& getPluginName() const      { return plugin; }
    const juce::StringArray& getTagNames() const   { return tagNames; }

    juce::File getFolder() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Onirique").getChildFile (plugin).getChildFile ("Presets");
    }

    juce::String getExtension() const   { return "." + plugin.toLowerCase(); }

    juce::StringArray getStockNames() const
    {
        juce::StringArray names;

        for (const auto& preset : stock)
            names.add (preset.name);

        return names;
    }

    juce::StringArray getUserNames() const
    {
        juce::StringArray names;

        for (const auto& file : getFolder().findChildFiles (juce::File::findFiles, false, "*" + getExtension()))
            names.add (file.getFileNameWithoutExtension());

        names.sortNatural();
        return names;
    }

    bool isStock (const juce::String& name) const   { return findStock (name) != nullptr; }
    bool isUser (const juce::String& name) const    { return ! isStock (name) && fileOf (name).existsAsFile(); }

    /** Les tags d'un preset d'usine ; ceux de l'utilisateur n'en ont pas. */
    int getTags (const juce::String& name) const
    {
        const auto* preset = findStock (name);
        return preset != nullptr ? preset->tags : 0;
    }

    juce::String getCurrentName() const   { return state.state.getProperty (kPresetProperty, "Default").toString(); }

    //==============================================================================
    // Ce à quoi le navigateur restreint la liste. Les flèches de la barre passent d'un preset à l'autre parmi ceux-là.
    /** Facultatif : un paramètre pour lequel ceci rend vrai garde sa valeur quand un preset se charge (le MIX verrouillé
        de VERB, par exemple). Vide : tout ce que le preset range est appliqué. */
    std::function<bool (const juce::String&)> keep;

    Source source = anySource;
    int tagFilter = 0;              // des bits : un preset passe s'il a l'un d'eux
    bool favouritesOnly = false;

    juce::StringArray getFiltered() const
    {
        juce::StringArray names;

        auto add = [&] (const juce::StringArray& from)
        {
            for (const auto& name : from)
                if ((tagFilter == 0 || (getTags (name) & tagFilter) != 0) && (! favouritesOnly || isFavourite (name)))
                    names.add (name);
        };

        if (source != userOnly)
            add (getStockNames());

        if (source != stockOnly)
            add (getUserNames());

        return names;
    }

    void step (int delta)
    {
        const auto names = getFiltered();

        if (names.isEmpty())
            return;

        const auto at = names.indexOf (getCurrentName());
        const auto next = at < 0 ? (delta > 0 ? 0 : names.size() - 1) : (at + delta + names.size()) % names.size();
        load (names[next]);
    }

    //==============================================================================
    void load (const juce::String& name)
    {
        if (const auto* preset = findStock (name))
        {
            juce::ValueTree snapshot (kSnapshot);

            for (const auto& [id, value] : preset->values)
                snapshot.setProperty (id, value, nullptr);

            apply (snapshot);
            setCurrentName (preset->name);
            return;
        }

        if (const auto xml = juce::parseXML (fileOf (name)))
        {
            const auto snapshot = juce::ValueTree::fromXml (*xml);

            if (snapshot.hasType (kSnapshot))
            {
                apply (snapshot);
                setCurrentName (name);
            }
        }
    }

    /** Enregistre les réglages sous ce nom ; rend le nom gardé, ou rien si l'écriture a échoué. */
    juce::String save (const juce::String& requested)
    {
        auto name = juce::File::createLegalFileName (requested.trim());

        if (name.isEmpty())
            return {};

        // Les presets d'usine ne sont pas à écraser.
        if (isStock (name))
            name << " (user)";

        const auto file = fileOf (name);

        if (! file.getParentDirectory().createDirectory().wasOk())
            return {};

        if (const auto xml = capture().createXml(); xml != nullptr && xml->writeTo (file))
        {
            setCurrentName (name);
            return name;
        }

        return {};
    }

    juce::String rename (const juce::String& from, const juce::String& requested)
    {
        const auto name = juce::File::createLegalFileName (requested.trim());

        if (! isUser (from) || name.isEmpty() || isStock (name) || fileOf (name).exists())
            return {};

        if (! fileOf (from).moveFileTo (fileOf (name)))
            return {};

        if (isFavourite (from))
        {
            setFavourite (from, false);
            setFavourite (name, true);
        }

        if (getCurrentName() == from)
            setCurrentName (name);

        return name;
    }

    void remove (const juce::String& name)
    {
        if (! isUser (name))
            return;

        fileOf (name).deleteFile();
        setFavourite (name, false);

        // Les réglages restent ; ils ne sont simplement plus ceux de personne.
        if (getCurrentName() == name)
            setCurrentName ("Default");
    }

    /** Copie dans le dossier des presets ceux de ce plugin parmi ces fichiers ; rend combien. */
    int importFiles (const juce::Array<juce::File>& files)
    {
        int count = 0;
        getFolder().createDirectory();

        for (const auto& file : files)
            if (file.hasFileExtension (getExtension()) && juce::parseXML (file) != nullptr)
                count += file.copyFileTo (getFolder().getNonexistentChildFile (file.getFileNameWithoutExtension(), getExtension(), false)) ? 1 : 0;

        return count;
    }

    //==============================================================================
    bool isFavourite (const juce::String& name) const   { return favourites.contains (name); }

    void setFavourite (const juce::String& name, bool on)
    {
        if (on == isFavourite (name))
            return;

        if (on)
            favourites.add (name);
        else
            favourites.removeString (name);

        favouritesFile().getParentDirectory().createDirectory();
        favouritesFile().replaceWithText (favourites.joinIntoString ("\n"));
    }

    /** D'autres instances du plugin ont pu changer les favoris : relu à chaque ouverture du navigateur. */
    void reloadFavourites()
    {
        favourites.clear();
        favourites.addLines (favouritesFile().loadFileAsString());
        favourites.removeEmptyStrings();
    }

    //==============================================================================
    // La comparaison A/B : deux jeux de réglages, celui qu'on entend et l'autre.
    int getSlot() const   { return activeSlot; }

    void switchToSlot (int slot)
    {
        slot = juce::jlimit (0, 1, slot);

        if (slot == activeSlot)
            return;

        juce::ValueTree wanted;

        {
            const juce::ScopedLock lock (slotLock);
            slots[activeSlot] = capture();
            slots[activeSlot].setProperty (kPresetProperty, getCurrentName(), nullptr);
            wanted = slots[slot];
            activeSlot = slot;
        }

        // Un côté jamais visité commence en copie de celui qu'on quitte.
        if (wanted.isValid())
        {
            apply (wanted);
            setCurrentName (wanted.getProperty (kPresetProperty, "Default").toString());
        }
    }

    void copyToOtherSlot()
    {
        const juce::ScopedLock lock (slotLock);
        slots[1 - activeSlot] = capture();
        slots[1 - activeSlot].setProperty (kPresetProperty, getCurrentName(), nullptr);
    }

    //==============================================================================
    // Le presse-papier : les réglages d'une instance collés dans une autre du même plugin.
    void copyToClipboard() const
    {
        auto snapshot = capture();
        snapshot.setProperty (kPluginProperty, plugin, nullptr);
        snapshot.setProperty (kPresetProperty, getCurrentName(), nullptr);

        if (const auto xml = snapshot.createXml())
            juce::SystemClipboard::copyTextToClipboard (xml->toString());
    }

    bool canPaste() const
    {
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        return text.contains ("<" + kSnapshot.toString()) && text.contains ("\"" + plugin + "\"");
    }

    void paste()
    {
        const auto xml = juce::parseXML (juce::SystemClipboard::getTextFromClipboard());

        if (xml == nullptr)
            return;

        const auto snapshot = juce::ValueTree::fromXml (*xml);

        if (snapshot.hasType (kSnapshot) && snapshot[kPluginProperty].toString() == plugin)
        {
            apply (snapshot);
            setCurrentName (snapshot.getProperty (kPresetProperty, "Default").toString());
        }
    }

    //==============================================================================
    /** Les réglages présents, et le preset qu'ils étaient, à remettre tels quels (Échap dans le navigateur). */
    juce::ValueTree capture() const
    {
        juce::ValueTree snapshot (kSnapshot);

        for (auto* parameter : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (isStored (ranged->paramID))
                    snapshot.setProperty (ranged->paramID, ranged->convertFrom0to1 (ranged->getValue()), nullptr);

        return snapshot;
    }

    void restore (const juce::ValueTree& snapshot, const juce::String& name)
    {
        apply (snapshot);
        setCurrentName (name);
    }

    //==============================================================================
    /** Dans l'arbre que le plugin enregistre : le côté A/B écouté, et l'autre. */
    void writeState (juce::ValueTree& tree) const
    {
        const juce::ScopedLock lock (slotLock);

        tree.setProperty (kSlotProperty, activeSlot, nullptr);
        tree.removeChild (tree.getChildWithName (kOtherSlot), nullptr);

        if (slots[1 - activeSlot].isValid())
        {
            juce::ValueTree other (kOtherSlot);
            other.copyPropertiesFrom (slots[1 - activeSlot], nullptr);
            tree.appendChild (other, nullptr);
        }
    }

    /** Avant apvts.replaceState : reprend l'autre côté A/B, et le retire de l'arbre. */
    void readState (juce::ValueTree& tree)
    {
        const auto other = tree.getChildWithName (kOtherSlot);

        {
            const juce::ScopedLock lock (slotLock);

            activeSlot = juce::jlimit (0, 1, (int) tree.getProperty (kSlotProperty, 0));
            slots[0] = slots[1] = {};

            if (other.isValid())
            {
                juce::ValueTree snapshot (kSnapshot);
                snapshot.copyPropertiesFrom (other, nullptr);
                slots[1 - activeSlot] = snapshot;
            }
        }

        tree.removeChild (other, nullptr);
    }

    //==============================================================================
    /** Pour les tests : ce qui cloche dans les presets d'usine (un paramètre qui n'existe pas, une valeur hors plage). */
    juce::StringArray checkStock() const
    {
        juce::StringArray problems;

        for (const auto& preset : stock)
            for (const auto& [id, value] : preset.values)
            {
                auto* parameter = state.getParameter (id);

                if (parameter == nullptr)
                    problems.add (preset.name + " : " + id + " n'existe pas");
                else if (const auto range = parameter->getNormalisableRange(); value < range.start || value > range.end)
                    problems.add (preset.name + " : " + id + " hors plage (" + juce::String (value) + ")");
            }

        return problems;
    }

private:
    bool isStored (const juce::String& id) const   { return ! left.contains (id); }

    const Stock* findStock (const juce::String& name) const
    {
        for (const auto& preset : stock)
            if (name.equalsIgnoreCase (preset.name))
                return &preset;

        return nullptr;
    }

    juce::File fileOf (const juce::String& name) const
    {
        return getFolder().getChildFile (juce::File::createLegalFileName (name) + getExtension());
    }

    juce::File favouritesFile() const   { return getFolder().getSiblingFile ("favourites.txt"); }

    void setCurrentName (const juce::String& name)   { state.state.setProperty (kPresetProperty, name, nullptr); }

    void apply (const juce::ValueTree& snapshot)
    {
        for (auto* parameter : state.processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

            if (ranged == nullptr || ! isStored (ranged->paramID) || (keep != nullptr && keep (ranged->paramID)))
                continue;

            // Ce qu'un preset ne nomme pas revient à sa valeur par défaut : un vieux preset garde son sens
            // quand un paramètre s'ajoute.
            const auto wanted = snapshot.hasProperty (ranged->paramID) ? ranged->convertTo0to1 ((float) snapshot[ranged->paramID])
                                                                       : ranged->getDefaultValue();

            if (std::abs (wanted - ranged->getValue()) > 1.0e-6f)
            {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost (wanted);
                ranged->endChangeGesture();
            }
        }
    }

    inline static const juce::Identifier kPresetProperty { "preset" }, kSlotProperty { "slot" }, kOtherSlot { "OtherSlot" },
                                         kSnapshot { "Snapshot" }, kPluginProperty { "plugin" };

    juce::AudioProcessorValueTreeState& state;
    juce::String plugin;
    std::vector<Stock> stock;
    juce::StringArray tagNames, left, favourites;

    juce::CriticalSection slotLock;
    juce::ValueTree slots[2];
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE (PresetBank)
};

} // namespace onirique
