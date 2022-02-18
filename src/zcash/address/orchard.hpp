// Copyright (c) 2021 The Zcash Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ADDRESS_ORCHARD_H
#define ZCASH_ADDRESS_ORCHARD_H

#include "streams.h"
#include "zcash/address/zip32.h"
#include <rust/orchard/keys.h>

namespace libzcash {

class OrchardIncomingViewingKey;

class OrchardRawAddress
{
private:
    std::unique_ptr<OrchardRawAddressPtr, decltype(&orchard_address_free)> inner;

    OrchardRawAddress() : inner(nullptr, orchard_address_free) {}

    OrchardRawAddress(OrchardRawAddressPtr* ptr) : inner(ptr, orchard_address_free) {}

    friend class OrchardIncomingViewingKey;
public:
    OrchardRawAddress(OrchardRawAddress&& key) : inner(std::move(key.inner)) {}

    OrchardRawAddress(const OrchardRawAddress& key) :
        inner(orchard_address_clone(key.inner.get()), orchard_address_free) {}

    OrchardRawAddress& operator=(OrchardRawAddress&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardRawAddress& operator=(const OrchardRawAddress& key)
    {
        if (this != &key) {
            inner.reset(orchard_address_clone(key.inner.get()));
        }
        return *this;
    }
};

class OrchardFullViewingKey;

class OrchardIncomingViewingKey
{
private:
    std::unique_ptr<OrchardIncomingViewingKeyPtr, decltype(&orchard_incoming_viewing_key_free)> inner;

    OrchardIncomingViewingKey() : inner(nullptr, orchard_incoming_viewing_key_free) {}

    OrchardIncomingViewingKey(OrchardIncomingViewingKeyPtr* key) :
        inner(key, orchard_incoming_viewing_key_free) {}

    friend class OrchardFullViewingKey;
public:

    OrchardIncomingViewingKey(OrchardIncomingViewingKey&& key) : inner(std::move(key.inner)) {}

    OrchardIncomingViewingKey(const OrchardIncomingViewingKey& key) :
        inner(orchard_incoming_viewing_key_clone(key.inner.get()), orchard_incoming_viewing_key_free) {}

    OrchardRawAddress Address(const diversifier_index_t& j) const;

    OrchardIncomingViewingKey& operator=(OrchardIncomingViewingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardIncomingViewingKey& operator=(const OrchardIncomingViewingKey& key)
    {
        if (this != &key) {
            inner.reset(orchard_incoming_viewing_key_clone(key.inner.get()));
        }
        return *this;
    }

    friend bool operator==(const OrchardIncomingViewingKey& a, const OrchardIncomingViewingKey& b)
    {
        return orchard_incoming_viewing_key_eq(a.inner.get(), b.inner.get());
    }

    friend bool operator<(const OrchardIncomingViewingKey& c1, const OrchardIncomingViewingKey& c2) {
        return orchard_incoming_viewing_key_lt(c1.inner.get(), c2.inner.get());
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_incoming_viewing_key_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard incoming viewing key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardIncomingViewingKeyPtr* key = orchard_incoming_viewing_key_parse(&rs, RustStream<Stream>::read_callback);
        if (key == nullptr) {
            throw std::ios_base::failure("Failed to parse Orchard incoming viewing key");
        }
        inner.reset(key);
    }

    template<typename Stream>
    static OrchardIncomingViewingKey Read(Stream& stream) {
        OrchardIncomingViewingKey key;
        stream >> key;
        return key;
    }
};

class OrchardSpendingKey;

class OrchardFullViewingKey
{
private:
    std::unique_ptr<OrchardFullViewingKeyPtr, decltype(&orchard_full_viewing_key_free)> inner;

    OrchardFullViewingKey() : inner(nullptr, orchard_full_viewing_key_free) {}

    OrchardFullViewingKey(OrchardFullViewingKeyPtr* ptr) :
        inner(ptr, orchard_full_viewing_key_free) {}

    friend class OrchardSpendingKey;
public:
    OrchardFullViewingKey(OrchardFullViewingKey&& key) : inner(std::move(key.inner)) {}

    OrchardFullViewingKey(const OrchardFullViewingKey& key) :
        inner(orchard_full_viewing_key_clone(key.inner.get()), orchard_full_viewing_key_free) {}

    OrchardFullViewingKey& operator=(OrchardFullViewingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardFullViewingKey& operator=(const OrchardFullViewingKey& key)
    {
        if (this != &key) {
            inner.reset(orchard_full_viewing_key_clone(key.inner.get()));
        }
        return *this;
    }

    friend bool operator==(const OrchardFullViewingKey& a, const OrchardFullViewingKey& b)
    {
        return orchard_full_viewing_key_eq(a.inner.get(), b.inner.get());
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_full_viewing_key_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard full viewing key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardFullViewingKeyPtr* key = orchard_full_viewing_key_parse(&rs, RustStream<Stream>::read_callback);
        if (key == nullptr) {
            throw std::ios_base::failure("Failed to parse Orchard full viewing key");
        }
        inner.reset(key);
    }

    template<typename Stream>
    static OrchardFullViewingKey Read(Stream& stream) {
        OrchardFullViewingKey key;
        stream >> key;
        return key;
    }

    OrchardIncomingViewingKey ToIncomingViewingKey() const;
};

class OrchardSpendingKey
{
private:
    std::unique_ptr<OrchardSpendingKeyPtr, decltype(&orchard_spending_key_free)> inner;

    OrchardSpendingKey() : inner(nullptr, orchard_spending_key_free) {}

    OrchardSpendingKey(OrchardSpendingKeyPtr* ptr) :
        inner(ptr, orchard_spending_key_free) {}
public:
    OrchardSpendingKey(OrchardSpendingKey&& key) : inner(std::move(key.inner)) {}

    OrchardSpendingKey(const OrchardSpendingKey& key) :
        inner(orchard_spending_key_clone(key.inner.get()), orchard_spending_key_free) {}

    static OrchardSpendingKey ForAccount(
            const HDSeed& seed,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    OrchardSpendingKey& operator=(OrchardSpendingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardSpendingKey& operator=(const OrchardSpendingKey& key)
    {
        if (this != &key) {
            inner.reset(orchard_spending_key_clone(key.inner.get()));
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_spending_key_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard spending key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardSpendingKeyPtr* key = orchard_spending_key_parse(&rs, RustStream<Stream>::read_callback);
        if (key == nullptr) {
            throw std::ios_base::failure("Failed to parse Orchard spending key");
        }
        inner.reset(key);
    }

    template<typename Stream>
    static OrchardSpendingKey Read(Stream& stream) {
        OrchardSpendingKey key;
        stream >> key;
        return key;
    }

    OrchardFullViewingKey ToFullViewingKey() const;
};

} // namespace libzcash

#endif // ZCASH_ADDRESS_ORCHARD_H

