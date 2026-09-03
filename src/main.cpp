// Banking System — command line interface.
//
//   bank                use ./bank_data.txt as the data file
//   bank --db <path>    use a different data file
//   bank --help         show usage
//   bank --version      print the version
//
// Build:  make   (or)  g++ -std=c++17 -O2 -Isrc src/*.cpp -o bank

#include <iostream>
#include <limits>
#include <string>

#include "account.h"
#include "bank.h"
#include "bank_store.h"
#include "money.h"

namespace {

constexpr const char* kVersion = "1.0.0";

void printBanner(const bank::BankStore& store, const bank::Bank& bank) {
    std::cout << "\n=================================================\n"
              << "              B A N K I N G   S Y S T E M\n"
              << "=================================================\n"
              << "Data file: " << store.path() << "  (" << bank.accountCount()
              << " account(s))\n";
}

// Reads a whole line, returning false on end-of-input (e.g. a closed pipe).
bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    std::cout.flush();
    if (!std::getline(std::cin, out)) return false;
    return true;
}

// Reads a positive money amount, re-prompting until it parses. Returns false on
// end-of-input so the caller can abort cleanly.
bool readAmount(const std::string& prompt, bank::Money& out) {
    std::string line;
    while (readLine(prompt, line)) {
        if (bank::Money::parse(line, out) && !out.isNegative()) return true;
        std::cout << "  Please enter an amount like 100 or 49.99.\n";
    }
    return false;
}

bool readAccountId(const std::string& prompt, std::int64_t& out) {
    std::string line;
    while (readLine(prompt, line)) {
        try {
            std::size_t consumed = 0;
            const long long value = std::stoll(line, &consumed);
            if (consumed == line.size()) {
                out = value;
                return true;
            }
        } catch (const std::exception&) {
            // fall through to the retry message
        }
        std::cout << "  Please enter a numeric account number.\n";
    }
    return false;
}

void printResult(const bank::Result& result) {
    if (result.ok()) {
        std::cout << "  [ok] Done. Balance of account " << result.accountId << ": "
                  << result.balance.toString() << "\n";
    } else {
        std::cout << "  [!]  " << bank::statusMessage(result.status) << "\n";
    }
}

void printStatement(const bank::Account& account) {
    std::cout << "\n  Statement for account " << account.id() << " (" << account.owner() << ")\n";
    std::cout << "  " << std::string(58, '-') << "\n";
    std::cout << "  " << "TYPE          AMOUNT            BALANCE          NOTE\n";
    std::cout << "  " << std::string(58, '-') << "\n";

    for (const bank::LedgerEntry& entry : account.ledger()) {
        std::string type = bank::entryTypeName(entry.type);
        type.resize(12, ' ');

        std::string amount = entry.amount.toString();
        amount.resize(16, ' ');

        std::string balance = entry.balanceAfter.toString();
        balance.resize(16, ' ');

        std::cout << "  " << type << "  " << amount << "  " << balance << "  " << entry.note;
        if (entry.counterparty != 0) std::cout << " (acct " << entry.counterparty << ")";
        std::cout << "\n";
    }
    std::cout << "  " << std::string(58, '-') << "\n";
    std::cout << "  Current balance: " << account.balance().toString() << "\n";
}

void listAccounts(const bank::Bank& bank) {
    if (bank.accountCount() == 0) {
        std::cout << "  No accounts yet.\n";
        return;
    }
    std::cout << "\n  ACCOUNT   BALANCE            OWNER\n";
    std::cout << "  " << std::string(48, '-') << "\n";
    for (std::int64_t id : bank.accountIds()) {
        const bank::Account* account = bank.find(id);
        std::string idStr = std::to_string(id);
        idStr.resize(8, ' ');
        std::string balance = account->balance().toString();
        balance.resize(16, ' ');
        std::cout << "  " << idStr << "  " << balance << "  " << account->owner() << "\n";
    }
    std::cout << "  " << std::string(48, '-') << "\n";
    std::cout << "  Total held: " << bank.totalDeposits().toString() << " across "
              << bank.accountCount() << " account(s)\n";
}

// Saves and reports any failure; returns false so a caller can decide to stop.
bool persist(const bank::BankStore& store, const bank::Bank& bank) {
    std::string error;
    if (!store.save(bank, error)) {
        std::cout << "  [!]  Could not save: " << error << "\n";
        return false;
    }
    return true;
}

int runMenu(bank::BankStore& store, bank::Bank& bank) {
    for (;;) {
        std::cout << "\n  1. Open account\n"
                     "  2. Deposit\n"
                     "  3. Withdraw\n"
                     "  4. Transfer\n"
                     "  5. View statement\n"
                     "  6. List all accounts\n"
                     "  0. Exit\n";

        std::string choice;
        if (!readLine("  Choose: ", choice)) break;

        if (choice == "0" || choice == "exit" || choice == "quit") {
            break;
        } else if (choice == "1") {
            std::string owner;
            if (!readLine("  Account holder name: ", owner)) break;
            bank::Money opening;
            if (!readAmount("  Opening balance (0 for none): ", opening)) break;
            const bank::Result result = bank.openAccount(owner, opening);
            if (result.ok()) {
                std::cout << "  [ok] Opened account " << result.accountId << " for " << owner
                          << " with " << opening.toString() << ".\n";
                persist(store, bank);
            } else {
                printResult(result);
            }
        } else if (choice == "2") {
            std::int64_t id;
            if (!readAccountId("  Account number: ", id)) break;
            bank::Money amount;
            if (!readAmount("  Amount to deposit: ", amount)) break;
            printResult(bank.deposit(id, amount, "cash deposit"));
            persist(store, bank);
        } else if (choice == "3") {
            std::int64_t id;
            if (!readAccountId("  Account number: ", id)) break;
            bank::Money amount;
            if (!readAmount("  Amount to withdraw: ", amount)) break;
            printResult(bank.withdraw(id, amount, "cash withdrawal"));
            persist(store, bank);
        } else if (choice == "4") {
            std::int64_t fromId, toId;
            if (!readAccountId("  From account: ", fromId)) break;
            if (!readAccountId("  To account: ", toId)) break;
            bank::Money amount;
            if (!readAmount("  Amount to transfer: ", amount)) break;
            printResult(bank.transfer(fromId, toId, amount, "transfer"));
            persist(store, bank);
        } else if (choice == "5") {
            std::int64_t id;
            if (!readAccountId("  Account number: ", id)) break;
            const bank::Account* account = bank.find(id);
            if (account == nullptr) {
                std::cout << "  [!]  " << bank::statusMessage(bank::Status::AccountNotFound) << "\n";
            } else {
                printStatement(*account);
            }
        } else if (choice == "6") {
            listAccounts(bank);
        } else {
            std::cout << "  Unrecognised choice.\n";
        }
    }

    std::cout << "\n  Goodbye.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string dbPath = "bank_data.txt";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Banking System " << kVersion << "\n\n"
                         "USAGE\n"
                         "  bank [--db <path>]\n\n"
                         "  --db <path>    data file to use (default: bank_data.txt)\n"
                         "  --help         show this help\n"
                         "  --version      print the version\n";
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "bank " << kVersion << "\n";
            return 0;
        }
        if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
            continue;
        }
        std::cerr << "error: unrecognised argument \"" << arg << "\"\n";
        return 2;
    }

    bank::BankStore store(dbPath);
    bank::Bank bank;

    std::string error;
    if (!store.load(bank, error)) {
        std::cerr << "error: could not load " << dbPath << ": " << error << "\n";
        return 1;
    }

    printBanner(store, bank);
    return runMenu(store, bank);
}
