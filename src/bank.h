#ifndef BANK_H
#define BANK_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "account.h"
#include "money.h"

namespace bank {

// The outcome of an operation. A distinct code per failure keeps the tests
// honest — they assert on the status rather than matching message wording — and
// lets a caller react differently to, say, insufficient funds versus a frozen
// account.
enum class Status {
    Ok,
    AccountNotFound,
    InvalidAmount,        // zero or negative where a positive amount is required
    InsufficientFunds,
    Overflow,             // the operation would exceed the representable range
    SameAccount,          // transfer source and destination are equal
    EmptyOwnerName,
};

std::string statusMessage(Status status);

struct Result {
    Status status = Status::Ok;
    Money balance;                 // resulting balance of the primary account
    std::int64_t accountId = 0;    // the account opened / acted upon
    bool ok() const { return status == Status::Ok; }
};

// The bank: a collection of accounts and the operations that move money between
// them. It performs NO console or file I/O — it takes values and returns a
// Result — which is what makes every rule here directly unit-testable and lets
// the same core back a CLI, a GUI or a web handler unchanged. Persistence lives
// in BankStore, which reads and writes a Bank.
class Bank {
public:
    // Opens a new account, assigning the next sequential id. An opening balance
    // is optional but may not be negative.
    Result openAccount(const std::string& owner, Money openingBalance = Money());

    Result deposit(std::int64_t id, Money amount, const std::string& note = "");
    Result withdraw(std::int64_t id, Money amount, const std::string& note = "");

    // Moves `amount` from `fromId` to `toId`. Atomic: if the debit cannot be
    // applied (missing account, insufficient funds, overflow on the credit) the
    // source is left untouched. Both ledgers get a matching entry on success.
    Result transfer(std::int64_t fromId, std::int64_t toId, Money amount,
                    const std::string& note = "");

    // Read-only lookups. Returns nullptr when the id is unknown.
    const Account* find(std::int64_t id) const;

    std::vector<std::int64_t> accountIds() const;
    std::size_t accountCount() const { return accounts_.size(); }
    Money totalDeposits() const;   // sum of all balances — the bank's liabilities

    // True when every account's ledger replays to its stored balance.
    bool verifyIntegrity() const;

    // Used by BankStore when loading; keeps nextId_ ahead of any loaded id.
    void adopt(Account account);

private:
    Account* findMutable(std::int64_t id);

    std::map<std::int64_t, Account> accounts_;
    std::int64_t nextId_ = 1001;   // human-friendly starting account number
};

}  // namespace bank

#endif  // BANK_H
