#include "bank_store.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace bank {

bool BankStore::load(Bank& bank, std::string& error) const {
    std::ifstream in(path_);
    if (!in) {
        // A missing file is a fresh, empty bank — not an error.
        return true;
    }

    std::string line;
    Account current;
    bool haveAccount = false;
    int lineNo = 0;

    // The header carries the authoritative balance; the entries that follow are
    // appended to the account before it is adopted into the bank.
    auto commit = [&]() {
        if (haveAccount) bank.adopt(current);
    };

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty()) continue;

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            error = "Malformed line " + std::to_string(lineNo) + ": no tag";
            return false;
        }
        const std::string tag = line.substr(0, tab);
        const std::string rest = line.substr(tab + 1);

        if (tag == "ACCOUNT") {
            commit();   // finish the previous account, if any
            if (!Account::deserialiseHeader(rest, current)) {
                error = "Malformed account header on line " + std::to_string(lineNo);
                return false;
            }
            haveAccount = true;
        } else if (tag == "ENTRY") {
            if (!haveAccount) {
                error = "Entry before any account on line " + std::to_string(lineNo);
                return false;
            }
            LedgerEntry entry;
            if (!LedgerEntry::deserialise(rest, entry)) {
                error = "Malformed ledger entry on line " + std::to_string(lineNo);
                return false;
            }
            current.appendLoadedEntry(entry);
        } else {
            error = "Unknown tag \"" + tag + "\" on line " + std::to_string(lineNo);
            return false;
        }
    }
    commit();

    // The stored balances must agree with the replayed ledgers, or the file has
    // been tampered with or corrupted.
    if (!bank.verifyIntegrity()) {
        error = "Ledger integrity check failed: a stored balance does not match its history";
        return false;
    }
    return true;
}

bool BankStore::save(const Bank& bank, std::string& error) const {
    const std::string tmpPath = path_ + ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out) {
            error = "Cannot open temporary file for writing: " + tmpPath;
            return false;
        }

        for (std::int64_t id : bank.accountIds()) {
            const Account* account = bank.find(id);
            if (account == nullptr) continue;   // cannot happen, but stay defensive

            out << "ACCOUNT\t" << account->serialiseHeader() << '\n';
            for (const LedgerEntry& entry : account->ledger()) {
                out << "ENTRY\t" << entry.serialise() << '\n';
            }
        }

        if (!out) {
            error = "Write failed for " + tmpPath;
            return false;
        }
    }  // out closes and flushes here

    // Atomic replace. std::rename does not overwrite on some platforms if the
    // target exists, so remove it first; the temp file still holds the data.
    std::remove(path_.c_str());
    if (std::rename(tmpPath.c_str(), path_.c_str()) != 0) {
        error = "Could not rename temporary file over " + path_;
        return false;
    }
    return true;
}

}  // namespace bank
