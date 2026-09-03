#include "bank.h"

namespace bank {

std::string statusMessage(Status status) {
    switch (status) {
        case Status::Ok: return "OK";
        case Status::AccountNotFound: return "No account exists with that number.";
        case Status::InvalidAmount: return "Amount must be greater than zero.";
        case Status::InsufficientFunds: return "Insufficient funds for this operation.";
        case Status::Overflow: return "Amount is too large to process.";
        case Status::SameAccount: return "Source and destination accounts must differ.";
        case Status::EmptyOwnerName: return "Account holder name must not be empty.";
    }
    return "Unknown error.";
}

const Account* Bank::find(std::int64_t id) const {
    auto it = accounts_.find(id);
    return it == accounts_.end() ? nullptr : &it->second;
}

Account* Bank::findMutable(std::int64_t id) {
    auto it = accounts_.find(id);
    return it == accounts_.end() ? nullptr : &it->second;
}

std::vector<std::int64_t> Bank::accountIds() const {
    std::vector<std::int64_t> ids;
    ids.reserve(accounts_.size());
    for (const auto& pair : accounts_) ids.push_back(pair.first);
    return ids;   // std::map keeps them sorted ascending
}

Money Bank::totalDeposits() const {
    Money total;
    for (const auto& pair : accounts_) total = total + pair.second.balance();
    return total;
}

bool Bank::verifyIntegrity() const {
    for (const auto& pair : accounts_) {
        if (!pair.second.verifyLedger()) return false;
    }
    return true;
}

void Bank::adopt(Account account) {
    const std::int64_t id = account.id();
    accounts_.emplace(id, std::move(account));
    if (id >= nextId_) nextId_ = id + 1;
}

Result Bank::openAccount(const std::string& owner, Money openingBalance) {
    Result result;

    // Trim leading/trailing spaces to reject a name that is only whitespace.
    std::size_t begin = owner.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        result.status = Status::EmptyOwnerName;
        return result;
    }
    if (openingBalance.isNegative()) {
        result.status = Status::InvalidAmount;
        return result;
    }

    const std::int64_t id = nextId_++;
    Account account(id, owner);
    account.open(openingBalance, "account opened");
    accounts_.emplace(id, std::move(account));

    result.accountId = id;
    result.balance = openingBalance;
    return result;
}

Result Bank::deposit(std::int64_t id, Money amount, const std::string& note) {
    Result result;
    result.accountId = id;

    Account* account = findMutable(id);
    if (account == nullptr) {
        result.status = Status::AccountNotFound;
        return result;
    }
    if (amount.isZero() || amount.isNegative()) {
        result.status = Status::InvalidAmount;
        return result;
    }

    Money newBalance;
    if (!account->balance().tryAdd(amount, newBalance)) {
        result.status = Status::Overflow;
        return result;
    }

    result.balance = account->apply(EntryType::Deposit, amount, 0, note);
    return result;
}

Result Bank::withdraw(std::int64_t id, Money amount, const std::string& note) {
    Result result;
    result.accountId = id;

    Account* account = findMutable(id);
    if (account == nullptr) {
        result.status = Status::AccountNotFound;
        return result;
    }
    if (amount.isZero() || amount.isNegative()) {
        result.status = Status::InvalidAmount;
        return result;
    }
    if (account->balance() < amount) {
        result.status = Status::InsufficientFunds;
        return result;
    }

    result.balance = account->apply(EntryType::Withdrawal, -amount, 0, note);
    return result;
}

Result Bank::transfer(std::int64_t fromId, std::int64_t toId, Money amount,
                      const std::string& note) {
    Result result;
    result.accountId = fromId;

    if (fromId == toId) {
        result.status = Status::SameAccount;
        return result;
    }
    if (amount.isZero() || amount.isNegative()) {
        result.status = Status::InvalidAmount;
        return result;
    }

    Account* from = findMutable(fromId);
    Account* to = findMutable(toId);
    if (from == nullptr || to == nullptr) {
        result.status = Status::AccountNotFound;
        return result;
    }
    if (from->balance() < amount) {
        result.status = Status::InsufficientFunds;
        return result;
    }

    // Check the credit side for overflow BEFORE touching the source, so a failed
    // transfer cannot leave money debited but not credited.
    Money creditedBalance;
    if (!to->balance().tryAdd(amount, creditedBalance)) {
        result.status = Status::Overflow;
        return result;
    }

    from->apply(EntryType::TransferOut, -amount, toId, note);
    to->apply(EntryType::TransferIn, amount, fromId, note);

    result.balance = from->balance();
    return result;
}

}  // namespace bank
