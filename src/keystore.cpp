// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "keystore.h"

#include "key.h"
#include "pubkey.h"
#include "util.h"


bool CKeyStore::AddKey(const CKey &key) {
    return AddKeyPubKey(key, key.GetPubKey());
}

bool CBasicKeyStore::GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const
{
    CKey key;
    if (!GetKey(address, key)) {
        LOCK(cs_KeyStore);
        WatchKeyMap::const_iterator it = mapWatchKeys.find(address);
        if (it != mapWatchKeys.end()) {
            vchPubKeyOut = it->second;
            return true;
        }
        return false;
    }
    vchPubKeyOut = key.GetPubKey();
    return true;
}

bool CBasicKeyStore::SetMnemonicSeed(const MnemonicSeed& seed)
{
    LOCK(cs_KeyStore);
    if (mnemonicSeed.has_value()) {
        // Don't allow an existing seed to be changed. We can maybe relax this
        // restriction later once we have worked out the UX implications.
        return false;
    }
    mnemonicSeed = seed;
    return true;
}

bool CBasicKeyStore::HaveMnemonicSeed() const
{
    LOCK(cs_KeyStore);
    return mnemonicSeed.has_value();
}

std::optional<MnemonicSeed> CBasicKeyStore::GetMnemonicSeed() const
{
    LOCK(cs_KeyStore);
    return mnemonicSeed;
}

bool CBasicKeyStore::SetLegacyHDSeed(const HDSeed& seed)
{
    LOCK(cs_KeyStore);
    if (legacySeed.has_value()) {
        // Don't allow an existing seed to be changed.
        return false;
    }
    legacySeed = seed;
    return true;
}

std::optional<HDSeed> CBasicKeyStore::GetLegacyHDSeed() const
{
    LOCK(cs_KeyStore);
    return legacySeed;
}

bool CBasicKeyStore::AddKeyPubKey(const CKey& key, const CPubKey &pubkey)
{
    LOCK(cs_KeyStore);
    mapKeys[pubkey.GetID()] = key;
    return true;
}

bool CBasicKeyStore::AddCScript(const CScript& redeemScript)
{
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
        return error("CBasicKeyStore::AddCScript(): redeemScripts > %i bytes are invalid", MAX_SCRIPT_ELEMENT_SIZE);

    LOCK(cs_KeyStore);
    mapScripts[CScriptID(redeemScript)] = redeemScript;
    return true;
}

bool CBasicKeyStore::HaveCScript(const CScriptID& hash) const
{
    LOCK(cs_KeyStore);
    return mapScripts.count(hash) > 0;
}

bool CBasicKeyStore::GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const
{
    LOCK(cs_KeyStore);
    ScriptMap::const_iterator mi = mapScripts.find(hash);
    if (mi != mapScripts.end())
    {
        redeemScriptOut = (*mi).second;
        return true;
    }
    return false;
}

static bool ExtractPubKey(const CScript &dest, CPubKey& pubKeyOut)
{
    //TODO: Use Solver to extract this?
    CScript::const_iterator pc = dest.begin();
    opcodetype opcode;
    std::vector<unsigned char> vch;
    if (!dest.GetOp(pc, opcode, vch) || vch.size() < 33 || vch.size() > 65)
        return false;
    pubKeyOut = CPubKey(vch);
    if (!pubKeyOut.IsFullyValid())
        return false;
    if (!dest.GetOp(pc, opcode, vch) || opcode != OP_CHECKSIG || dest.GetOp(pc, opcode, vch))
        return false;
    return true;
}

bool CBasicKeyStore::AddWatchOnly(const CScript &dest)
{
    LOCK(cs_KeyStore);
    setWatchOnly.insert(dest);
    CPubKey pubKey;
    if (ExtractPubKey(dest, pubKey))
        mapWatchKeys[pubKey.GetID()] = pubKey;
    return true;
}

bool CBasicKeyStore::RemoveWatchOnly(const CScript &dest)
{
    LOCK(cs_KeyStore);
    setWatchOnly.erase(dest);
    CPubKey pubKey;
    if (ExtractPubKey(dest, pubKey))
        mapWatchKeys.erase(pubKey.GetID());
    return true;
}

bool CBasicKeyStore::HaveWatchOnly(const CScript &dest) const
{
    LOCK(cs_KeyStore);
    return setWatchOnly.count(dest) > 0;
}

bool CBasicKeyStore::HaveWatchOnly() const
{
    LOCK(cs_KeyStore);
    return (!setWatchOnly.empty());
}

bool CBasicKeyStore::AddSproutSpendingKey(const libzcash::SproutSpendingKey &sk)
{
    LOCK(cs_KeyStore);
    auto address = sk.address();
    mapSproutSpendingKeys[address] = sk;
    mapNoteDecryptors.insert(std::make_pair(address, ZCNoteDecryption(sk.receiving_key())));
    return true;
}

//! Sapling
bool CBasicKeyStore::AddSaplingSpendingKey(
    const libzcash::SaplingExtendedSpendingKey &sk)
{
    LOCK(cs_KeyStore);
    auto extfvk = sk.ToXFVK();

    // if extfvk is not in SaplingFullViewingKeyMap, add it
    if (!CBasicKeyStore::AddSaplingFullViewingKey(extfvk)) {
        return false;
    }

    mapSaplingSpendingKeys[extfvk] = sk;

    return true;
}

bool CBasicKeyStore::AddSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    LOCK(cs_KeyStore);
    auto address = vk.address();
    mapSproutViewingKeys[address] = vk;
    mapNoteDecryptors.insert(std::make_pair(address, ZCNoteDecryption(vk.sk_enc)));
    return true;
}

bool CBasicKeyStore::AddSaplingFullViewingKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk)
{
    LOCK(cs_KeyStore);
    auto ivk = extfvk.ToIncomingViewingKey();
    mapSaplingFullViewingKeys[ivk] = extfvk;

    return true;
}

// This function updates the wallet's internal address->ivk map.
// If we add an address that is already in the map, the map will
// remain unchanged as each address only has one ivk.
bool CBasicKeyStore::AddSaplingPaymentAddress(
    const libzcash::SaplingIncomingViewingKey &ivk,
    const libzcash::SaplingPaymentAddress &addr)
{
    LOCK(cs_KeyStore);

    // Add addr -> SaplingIncomingViewing to SaplingIncomingViewingKeyMap
    mapSaplingIncomingViewingKeys[addr] = ivk;

    return true;
}

bool CBasicKeyStore::RemoveSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    LOCK(cs_KeyStore);
    mapSproutViewingKeys.erase(vk.address());
    return true;
}

bool CBasicKeyStore::HaveSproutViewingKey(const libzcash::SproutPaymentAddress &address) const
{
    LOCK(cs_KeyStore);
    return mapSproutViewingKeys.count(address) > 0;
}

bool CBasicKeyStore::HaveSaplingFullViewingKey(const libzcash::SaplingIncomingViewingKey &ivk) const
{
    LOCK(cs_KeyStore);
    return mapSaplingFullViewingKeys.count(ivk) > 0;
}

bool CBasicKeyStore::HaveSaplingIncomingViewingKey(const libzcash::SaplingPaymentAddress &addr) const
{
    LOCK(cs_KeyStore);
    return mapSaplingIncomingViewingKeys.count(addr) > 0;
}

bool CBasicKeyStore::GetSproutViewingKey(
    const libzcash::SproutPaymentAddress &address,
    libzcash::SproutViewingKey &vkOut) const
{
    LOCK(cs_KeyStore);
    SproutViewingKeyMap::const_iterator mi = mapSproutViewingKeys.find(address);
    if (mi != mapSproutViewingKeys.end()) {
        vkOut = mi->second;
        return true;
    }
    return false;
}

bool CBasicKeyStore::GetSaplingFullViewingKey(
    const libzcash::SaplingIncomingViewingKey &ivk,
    libzcash::SaplingExtendedFullViewingKey &extfvkOut) const
{
    LOCK(cs_KeyStore);
    SaplingFullViewingKeyMap::const_iterator mi = mapSaplingFullViewingKeys.find(ivk);
    if (mi != mapSaplingFullViewingKeys.end()) {
        extfvkOut = mi->second;
        return true;
    }
    return false;
}

bool CBasicKeyStore::GetSaplingIncomingViewingKey(const libzcash::SaplingPaymentAddress &addr,
                                   libzcash::SaplingIncomingViewingKey &ivkOut) const
{
    LOCK(cs_KeyStore);
    SaplingIncomingViewingKeyMap::const_iterator mi = mapSaplingIncomingViewingKeys.find(addr);
    if (mi != mapSaplingIncomingViewingKeys.end()) {
        ivkOut = mi->second;
        return true;
    }
    return false;
}

bool CBasicKeyStore::GetSaplingExtendedSpendingKey(
        const libzcash::SaplingPaymentAddress &addr,
        libzcash::SaplingExtendedSpendingKey &extskOut) const {
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingExtendedFullViewingKey extfvk;

    LOCK(cs_KeyStore);
    return GetSaplingIncomingViewingKey(addr, ivk) &&
            GetSaplingFullViewingKey(ivk, extfvk) &&
            GetSaplingSpendingKey(extfvk, extskOut);
}

bool CBasicKeyStore::HaveSaplingSpendingKeyForAddress(
        const libzcash::SaplingPaymentAddress &addr) const {
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingExtendedFullViewingKey extfvk;

    return GetSaplingIncomingViewingKey(addr, ivk) &&
        GetSaplingFullViewingKey(ivk, extfvk) &&
        HaveSaplingSpendingKey(extfvk);
}

//
// Unified Keys
//

bool CBasicKeyStore::AddUnifiedFullViewingKey(
        const libzcash::ZcashdUnifiedFullViewingKey &ufvk)
{
    LOCK(cs_KeyStore);

    // Add the Sapling component of the UFVK to the wallet.
    auto saplingKey = ufvk.GetSaplingKey();
    if (saplingKey.has_value()) {
        auto ivk = saplingKey.value().ToIncomingViewingKey();
        mapSaplingKeyUnified.insert(std::make_pair(ivk, ufvk.GetKeyID()));

        auto changeIvk = saplingKey.value().GetChangeIVK();
        mapSaplingKeyUnified.insert(std::make_pair(changeIvk, ufvk.GetKeyID()));
    }

    // We can't reasonably add the transparent component here, because
    // of the way that transparent addresses are generated from the
    // P2PKH part of the unified address. Instead, whenever a new
    // unified address is generated, the keys associated with the
    // transparent part of the address must be added to the keystore.

    // Add the UFVK by key identifier.
    mapUnifiedFullViewingKeys.insert({ufvk.GetKeyID(), ufvk});

    return true;
}

bool CBasicKeyStore::AddTransparentReceiverForUnifiedAddress(
        const libzcash::UFVKId& keyId,
        const libzcash::diversifier_index_t& diversifierIndex,
        const libzcash::UnifiedAddress& ua)
{
    LOCK(cs_KeyStore);

    // It's only necessary to add p2pkh and p2sh components of
    // the UA; all other lookups of the associated UFVK will be
    // made via the protocol-specific viewing key that is used
    // to trial-decrypt a transaction.
    auto addrEntry = std::make_pair(keyId, diversifierIndex);

    auto p2pkhReceiver = ua.GetP2PKHReceiver();
    if (p2pkhReceiver.has_value()) {
        mapP2PKHUnified.insert(std::make_pair(p2pkhReceiver.value(), addrEntry));
    }

    auto p2shReceiver = ua.GetP2SHReceiver();
    if (p2shReceiver.has_value()) {
        mapP2SHUnified.insert(std::make_pair(p2shReceiver.value(), addrEntry));
    }

    return true;
}

std::optional<libzcash::ZcashdUnifiedFullViewingKey> CBasicKeyStore::GetUnifiedFullViewingKey(
        const libzcash::UFVKId& keyId) const
{
    auto mi = mapUnifiedFullViewingKeys.find(keyId);
    if (mi != mapUnifiedFullViewingKeys.end()) {
        return mi->second;
    } else {
        return std::nullopt;
    }
}

std::optional<std::pair<libzcash::UFVKId, std::optional<libzcash::diversifier_index_t>>>
CBasicKeyStore::GetUFVKMetadataForReceiver(const libzcash::Receiver& receiver) const
{
    return std::visit(FindUFVKId(*this), receiver);
}

std::optional<libzcash::UFVKId> CBasicKeyStore::GetUFVKIdForViewingKey(const libzcash::ViewingKey& vk) const
{
    std::optional<libzcash::UFVKId> result;
    std::visit(match {
        [&](const libzcash::SproutViewingKey& vk) {},
        [&](const libzcash::SaplingExtendedFullViewingKey& extfvk) {
            const auto saplingIvk = extfvk.ToIncomingViewingKey();
            const auto ufvkId = mapSaplingKeyUnified.find(saplingIvk);
            if (ufvkId != mapSaplingKeyUnified.end()) {
                result = ufvkId->second;
            }
        },
        [&](const libzcash::UnifiedFullViewingKey& ufvk) {
            const auto saplingDfvk = ufvk.GetSaplingKey();
            if (saplingDfvk.has_value()) {
                const auto saplingIvk = saplingDfvk.value().ToIncomingViewingKey();
                const auto ufvkId = mapSaplingKeyUnified.find(saplingIvk);
                if (ufvkId != mapSaplingKeyUnified.end()) {
                    result = ufvkId->second;
                }
            }
        }
    }, vk);
    return result;
}

std::optional<std::pair<libzcash::UFVKId, std::optional<libzcash::diversifier_index_t>>>
FindUFVKId::operator()(const libzcash::SaplingPaymentAddress& saplingAddr) const {
    const auto saplingIvk = keystore.mapSaplingIncomingViewingKeys.find(saplingAddr);
    if (saplingIvk != keystore.mapSaplingIncomingViewingKeys.end()) {
        const auto ufvkId = keystore.mapSaplingKeyUnified.find(saplingIvk->second);
        if (ufvkId != keystore.mapSaplingKeyUnified.end()) {
            return std::make_pair(ufvkId->second, std::nullopt);
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}
std::optional<std::pair<libzcash::UFVKId, std::optional<libzcash::diversifier_index_t>>>
FindUFVKId::operator()(const CScriptID& scriptId) const {
    const auto metadata = keystore.mapP2SHUnified.find(scriptId);
    if (metadata != keystore.mapP2SHUnified.end()) {
        return metadata->second;
    } else {
        return std::nullopt;
    }
}
std::optional<std::pair<libzcash::UFVKId, std::optional<libzcash::diversifier_index_t>>>
FindUFVKId::operator()(const CKeyID& keyId) const {
    const auto metadata = keystore.mapP2PKHUnified.find(keyId);
    if (metadata != keystore.mapP2PKHUnified.end()) {
        return metadata->second;
    } else {
        return std::nullopt;
    }
}
std::optional<std::pair<libzcash::UFVKId, std::optional<libzcash::diversifier_index_t>>>
FindUFVKId::operator()(const libzcash::UnknownReceiver& receiver) const {
    return std::nullopt;
}
