#ifndef BANK_STORE_H
#define BANK_STORE_H

#include <string>

#include "bank.h"

namespace bank {

// File-backed persistence for a Bank.
//
// The file is line-oriented and tab-separated. Each account is written as one
// header line followed by one line per ledger entry:
//
//   ACCOUNT <tab> id <tab> balance <tab> entryCount <tab> owner
//   ENTRY   <tab> type <tab> amount <tab> balanceAfter <tab> counterparty <tab> note
//
// Amounts are stored as integer minor units, so no precision is lost on a
// round trip. Saves are atomic: the data is written to a temporary file which
// is then renamed over the original. Rename is atomic on a single filesystem,
// so an interrupted save leaves the previous file intact rather than a
// half-written one.
class BankStore {
public:
    explicit BankStore(std::string path) : path_(std::move(path)) {}

    const std::string& path() const { return path_; }

    // Loads into `bank`. A missing file is treated as an empty bank (success),
    // so first run needs no setup. Malformed lines are reported via `error` and
    // cause the load to fail rather than silently dropping data.
    bool load(Bank& bank, std::string& error) const;

    // Writes `bank` to disk atomically. Returns false and sets `error` on an I/O
    // failure.
    bool save(const Bank& bank, std::string& error) const;

private:
    std::string path_;
};

}  // namespace bank

#endif  // BANK_STORE_H
