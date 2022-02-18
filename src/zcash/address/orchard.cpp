// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash/address/orchard.hpp"

namespace libzcash {

OrchardRawAddress OrchardIncomingViewingKey::Address(const diversifier_index_t& j) const {
    return OrchardRawAddress(orchard_incoming_viewing_key_to_address(inner.get(), j.begin()));
}

OrchardIncomingViewingKey OrchardFullViewingKey::ToIncomingViewingKey() const {
    return OrchardIncomingViewingKey(orchard_full_viewing_key_to_incoming_viewing_key(inner.get()));
}

OrchardSpendingKey OrchardSpendingKey::ForAccount(
        const HDSeed& seed,
        uint32_t bip44CoinType,
        libzcash::AccountId accountId) {

    auto ptr = orchard_spending_key_for_account(
            seed.RawSeed().data(),
            seed.RawSeed().size(),
            bip44CoinType,
            accountId);

    if (ptr == nullptr) {
        throw std::ios_base::failure("Unable to generate Orchard extended spending key.");
    }

    return OrchardSpendingKey(ptr);
}

OrchardFullViewingKey OrchardSpendingKey::ToFullViewingKey() const {
    return OrchardFullViewingKey(orchard_spending_key_to_full_viewing_key(inner.get()));
}

} //namespace libzcash

