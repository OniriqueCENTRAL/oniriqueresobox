#include "License.h"

#include <juce_cryptography/juce_cryptography.h>

#include <cstring>
#include <vector>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
 #include <IOKit/IOKitLib.h>
#endif

extern "C"
{
#include "tweetnacl.h"

// TweetNaCl réclame cette fonction pour créer des clés et signer. Le plugin ne fait que vérifier :
// elle n'est jamais appelée.
void randombytes (unsigned char* bytes, unsigned long long count)
{
    std::memset (bytes, 0, (size_t) count);
}
}

namespace onirique::license
{

// Clé publique du serveur ONIRIQUE CENTRAL (https://central.onirique.store/api/public-key, champ "raw").
static const unsigned char serverPublicKey[32] = {
    0xd0, 0xc1, 0x4c, 0xa4, 0x0e, 0x26, 0xe2, 0xce, 0x06, 0xaf, 0xa9, 0x8f, 0xd0, 0xcb, 0xe6, 0x48,
    0x7d, 0x67, 0x9f, 0x57, 0x55, 0x41, 0xed, 0x75, 0x75, 0x9f, 0x58, 0x9b, 0xb9, 0x0f, 0x0b, 0xac
};

int Status::daysLeft() const noexcept
{
    if (expiresAt <= 0)
        return 0;

    const auto remaining = expiresAt - juce::Time::currentTimeMillis();
    return remaining <= 0 ? 0 : (int) ((remaining + 86399999) / 86400000);
}

juce::String machineId()
{
    juce::String guid;

   #if JUCE_WINDOWS
    guid = juce::WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid",
                                            {}, juce::WindowsRegistry::WoW64_64bit);
   #elif JUCE_MAC
    // L'identifiant matériel du Mac (IOPlatformUUID) : le même que lit l'application ONIRIQUE CENTRAL.
    if (const auto entry = IOServiceGetMatchingService (0, IOServiceMatching ("IOPlatformExpertDevice")))
    {
        if (const auto uuid = (CFStringRef) IORegistryEntryCreateCFProperty (entry, CFSTR ("IOPlatformUUID"), kCFAllocatorDefault, 0))
        {
            guid = juce::String::fromCFString (uuid);
            CFRelease (uuid);
        }

        IOObjectRelease (entry);
    }
   #endif

    if (guid.isEmpty())
        return {};

    const auto text = "onirique:" + guid.trim().toLowerCase();
    return juce::SHA256 (text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString().substring (0, 40);
}

juce::File licenseFile (const juce::String& productSlug)
{
   #if JUCE_MAC
    // Dossier commun à tous les comptes du Mac, où l'application ONIRIQUE CENTRAL écrit sans mot de passe.
    return juce::File ("/Users/Shared/Onirique/Licenses").getChildFile (productSlug + ".lic");
   #endif

    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
        .getChildFile ("Onirique").getChildFile ("Licenses").getChildFile (productSlug + ".lic");
}

static bool decodeBase64 (const juce::String& text, juce::MemoryBlock& out)
{
    juce::MemoryOutputStream stream (out, false);
    return juce::Base64::convertFromBase64 (stream, text);
}

Status verify (const juce::File& file, const juce::String& productSlug, const juce::String& machine,
               const unsigned char* publicKey, juce::int64 nowMs)
{
    const Status none;

    // Un fichier de licence fait quelques centaines d'octets : au-delà, ce n'en est pas un.
    if (machine.isEmpty() || ! file.existsAsFile() || file.getSize() > 16384)
        return none;

    const auto envelope = juce::JSON::parse (file.loadFileAsString());
    if (envelope["format"].toString() != "onirique-license-v1")
        return none;

    juce::MemoryBlock payload, signature;
    if (! decodeBase64 (envelope["payload"].toString(), payload) || ! decodeBase64 (envelope["signature"].toString(), signature))
        return none;
    if (signature.getSize() != crypto_sign_BYTES || payload.getSize() == 0)
        return none;

    // Vérification Ed25519 : TweetNaCl attend la signature suivie du message, et rend le message s'il est authentique.
    std::vector<unsigned char> signedMessage (signature.getSize() + payload.getSize());
    std::memcpy (signedMessage.data(), signature.getData(), signature.getSize());
    std::memcpy (signedMessage.data() + signature.getSize(), payload.getData(), payload.getSize());

    std::vector<unsigned char> opened (signedMessage.size());
    unsigned long long openedSize = 0;
    if (crypto_sign_open (opened.data(), &openedSize, signedMessage.data(), signedMessage.size(), publicKey) != 0)
        return none;

    // À partir d'ici le contenu vient bien du serveur ONIRIQUE. Reste à savoir s'il vaut pour ce plugin, ce PC, aujourd'hui.
    const auto data = juce::JSON::parse (juce::String::fromUTF8 ((const char*) opened.data(), (int) openedSize));
    if (data["product"].toString() != productSlug || data["machine"].toString() != machine)
        return none;

    Status status;
    status.email = data["email"].toString();
    status.expiresAt = data["expiresAt"].isVoid() ? 0 : (juce::int64) (double) data["expiresAt"];
    if (status.expiresAt > 0 && status.expiresAt <= nowMs)
        return none;

    status.state = data["type"].toString() == "trial" ? State::trial : State::licensed;
    return status;
}

Status read (const juce::String& productSlug)
{
    // Pour tester le mode démo sur un PC qui a une licence. Ne peut que retirer la licence, jamais en donner une.
    if (juce::SystemStats::getEnvironmentVariable ("ONIRIQUE_DEMO", {}).isNotEmpty())
        return {};

    return verify (licenseFile (productSlug), productSlug, machineId(), serverPublicKey, juce::Time::currentTimeMillis());
}

} // namespace onirique::license
