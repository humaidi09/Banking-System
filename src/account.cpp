#include "account.h"

#include <sstream>

namespace bank {

std::string entryTypeName(EntryType type) {
    switch (type) {
        case EntryType::Open: return "OPEN";
        case EntryType::Deposit: return "DEPOSIT";
        case EntryType::Withdrawal: return "WITHDRAWAL";
        case EntryType::TransferIn: return "TRANSFER_IN";
        case EntryType::TransferOut: return "TRANSFER_OUT";
    }
    return "DEPOSIT";
}

namespace {

bool entryTypeFromName(const std::string& name, EntryType& out) {
    if (name == "OPEN") { out = EntryType::Open; return true; }
    if (name == "DEPOSIT") { out = EntryType::Deposit; return true; }
    if (name == "WITHDRAWAL") { out = EntryType::Withdrawal; return true; }
    if (name == "TRANSFER_IN") { out = EntryType::TransferIn; return true; }
    if (name == "TRANSFER_OUT") { out = EntryType::TransferOut; return true; }
    return false;
}

// A note may contain spaces but never a tab or newline, so tab-separation stays
// unambiguous. Any stray tab in a note is replaced with a space on the way out.
std::string sanitiseNote(const std::string& note) {
    std::string clean = note;
    for (char& c : clean) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return clean;
}

}  // namespace

std::string LedgerEntry::serialise() const {
    std::ostringstream out;
    out << entryTypeName(type) << '\t' << amount.minorUnits() << '\t'
        << balanceAfter.minorUnits() << '\t' << counterparty << '\t'
        << sanitiseNote(note);
    return out.str();
}

bool LedgerEntry::deserialise(const std::string& line, LedgerEntry& out) {
    std::istringstream in(line);
    std::string typeName, amountStr, balanceStr, counterpartyStr;

    if (!std::getline(in, typeName, '\t')) return false;
    if (!std::getline(in, amountStr, '\t')) return false;
    if (!std::getline(in, balanceStr, '\t')) return false;
    if (!std::getline(in, counterpartyStr, '\t')) return false;

    std::string note;
    std::getline(in, note);   // the rest of the line, may be empty

    EntryType type;
    if (!entryTypeFromName(typeName, type)) return false;

    try {
        out.type = type;
        out.amount = Money(std::stoll(amountStr));
        out.balanceAfter = Money(std::stoll(balanceStr));
        out.counterparty = std::stoll(counterpartyStr);
        out.note = note;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

void Account::open(Money openingBalance, const std::string& note) {
    balance_ = openingBalance;
    LedgerEntry entry;
    entry.type = EntryType::Open;
    entry.amount = openingBalance;
    entry.balanceAfter = openingBalance;
    entry.counterparty = 0;
    entry.note = note;
    ledger_.push_back(entry);
}

Money Account::apply(EntryType type, Money signedAmount, std::int64_t counterparty,
                     const std::string& note) {
    balance_ = balance_ + signedAmount;

    LedgerEntry entry;
    entry.type = type;
    // The ledger stores the magnitude of the movement; the type says direction.
    entry.amount = signedAmount.isNegative() ? -signedAmount : signedAmount;
    entry.balanceAfter = balance_;
    entry.counterparty = counterparty;
    entry.note = note;
    ledger_.push_back(entry);

    return balance_;
}

bool Account::verifyLedger() const {
    Money running;
    for (const LedgerEntry& entry : ledger_) {
        switch (entry.type) {
            case EntryType::Open:
                running = entry.amount;
                break;
            case EntryType::Deposit:
            case EntryType::TransferIn:
                running = running + entry.amount;
                break;
            case EntryType::Withdrawal:
            case EntryType::TransferOut:
                running = running - entry.amount;
                break;
        }
        if (running != entry.balanceAfter) return false;
    }
    return running == balance_;
}

std::string Account::serialiseHeader() const {
    std::ostringstream out;
    std::string owner = owner_;
    for (char& c : owner) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    out << id_ << '\t' << balance_.minorUnits() << '\t' << ledger_.size() << '\t' << owner;
    return out.str();
}

bool Account::deserialiseHeader(const std::string& line, Account& out) {
    std::istringstream in(line);
    std::string idStr, balanceStr, countStr;

    if (!std::getline(in, idStr, '\t')) return false;
    if (!std::getline(in, balanceStr, '\t')) return false;
    if (!std::getline(in, countStr, '\t')) return false;

    std::string owner;
    std::getline(in, owner);

    try {
        out.id_ = std::stoll(idStr);
        out.balance_ = Money(std::stoll(balanceStr));
        out.owner_ = owner;
        out.ledger_.clear();
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace bank
