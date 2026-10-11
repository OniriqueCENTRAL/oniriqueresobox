#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  Licence ONIRIQUE : lecture et vérification du fichier écrit par l'application
//  ONIRIQUE CENTRAL quand le plugin est activé sur ce PC
//  (C:\ProgramData\Onirique\Licenses\<plugin>.lic sous Windows,
//  /Users/Shared/Onirique/Licenses/<plugin>.lic sur Mac).
//
//  Le fichier est signé par le serveur (Ed25519). Le plugin n'embarque que la
//  clé publique : il peut vérifier une licence, pas en fabriquer une. Tout se
//  fait hors ligne.
//
//  Ce dossier est le même pour tous les plugins ONIRIQUE (voir README.md) : seul
//  l'identifiant du plugin change.
//==============================================================================
namespace onirique::license
{

enum class State
{
    unlicensed, // pas de licence valable sur ce PC : bruit blanc à l'export, et par salves à l'écoute
    trial,      // essai gratuit en cours : écoute intacte, bruit blanc à l'export
    licensed    // plugin acheté ou abonnement actif
};

struct Status
{
    State state = State::unlicensed;
    juce::String email;        // compte auquel la licence appartient
    juce::int64 expiresAt = 0; // millisecondes depuis 1970 ; 0 = sans limite

    /** Seul un plugin acheté exporte proprement : en essai ou en démo, l'export produit un bruit blanc. */
    bool exportsClean() const noexcept { return state == State::licensed; }

    /** Jours entiers restants avant l'expiration (0 si sans limite ou expirée). */
    int daysLeft() const noexcept;
};

/** Empreinte de ce PC, calculée comme dans ONIRIQUE CENTRAL :
    40 premiers caractères hexadécimaux de SHA-256 ("onirique:" + MachineGuid en minuscules). */
juce::String machineId();

/** Le fichier de licence de ce plugin sur ce PC. */
juce::File licenseFile (const juce::String& productSlug);

/** Lit la licence de ce plugin et la vérifie avec la clé publique ONIRIQUE embarquée. */
Status read (const juce::String& productSlug);

/** La même vérification, avec tout en paramètre (sert aux tests). `publicKey` : 32 octets. */
Status verify (const juce::File& file, const juce::String& productSlug, const juce::String& machine,
               const unsigned char* publicKey, juce::int64 nowMs);

} // namespace onirique::license
