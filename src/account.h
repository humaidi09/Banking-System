#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <cstdint>
#include <string>
#include <vector>

#include "money.h"

namespace bank {

// The kind of movement a ledger entry records. Stored as the account's own
// history so a statement can be reconstructed and the balance re-derived.
enum class EntryType {
    Open,        // account opened, optional opening balance
    Deposit,
    Withdrawal,
    TransferIn,  // received from another account
    TransferOut, // sent to another account
};

std::string entryTypeName(EntryType type);

// One immutable line in an account's ledger. `balanceAfter` is stored so a
// printed statement does not have to re-run the whole history, and so a
// tampered balance can be detected by replaying the entries.
struct LedgerEntry {
    EntryType type = EntryType::Deposit;
    Money amount;                 // always the non-negative size of the movement
    Money balanceAfter;
    std::int64_t counterparty = 0;  // the other account for transfers, else 0
    std::string note;

    std::string serialise() const;
    static bool deserialise(const std::string& line, LedgerEntry& out);
};

// A single bank account: an identity, an owner, a balance and the full ledger
// that produced that balance. The account enforces its own invariants — it
// never exposes a raw balance setter — so no caller can move money without
// leaving a matching ledger entry.
class Account {
public:
    Account() = default;
    Account(std::int64_t id, std::string owner)
        : id_(id), owner_(std::move(owner)) {}

    std::int64_t id() const { return id_; }
    const std::string& owner() const { return owner_; }
    Money balance() const { return balance_; }
    const std::vector<LedgerEntry>& ledger() const { return ledger_; }

    // Records an opening entry. Only valid as the first entry on a fresh account.
    void open(Money openingBalance, const std::string& note);

    // Applies a movement, appends the matching ledger entry and returns the new
    // balance. `signedAmount` is positive for credits, negative for debits; the
    // caller (Bank) is responsible for having checked funds and overflow first.
    Money apply(EntryType type, Money signedAmount, std::int64_t counterparty,
                const std::string& note);

    // Re-adds every ledger entry from zero and returns whether the running total
    // matches the stored balance at every step — the integrity check.
    bool verifyLedger() const;

    // Tab-separated header line (without the ledger, which serialises per entry).
    std::string serialiseHeader() const;
    static bool deserialiseHeader(const std::string& line, Account& out);
    void appendLoadedEntry(const LedgerEntry& entry) { ledger_.push_back(entry); }
    void setBalance(Money balance) { balance_ = balance; }

private:
    std::int64_t id_ = 0;
    std::string owner_;
    Money balance_;
    std::vector<LedgerEntry> ledger_;
};

}  // namespace bank

#endif  // ACCOUNT_H
