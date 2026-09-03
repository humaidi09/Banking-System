// Unit tests for the banking system.
//
// Build and run:  make test
//
// Organised bottom-up: the Money value type first, then the Account ledger,
// then the Bank operations and their invariants, then persistence round-trips.

#include <cstdio>
#include <limits>
#include <string>

#include "../src/account.h"
#include "../src/bank.h"
#include "../src/bank_store.h"
#include "../src/money.h"
#include "test_framework.h"

using namespace bank;

namespace {

// A unique temp path per store test, cleaned up by the test that creates it.
std::string tempFile(const std::string& tag) {
    return "test_bank_" + tag + ".txt";
}

}  // namespace

// -------------------------------------------------------------------- Money

TEST_CASE("money is built from major and minor units") {
    CHECK_EQ(Money::of(12, 50).minorUnits(), 1250);
    CHECK_EQ(Money::of(0, 5).minorUnits(), 5);
    CHECK_EQ(Money::of(100).minorUnits(), 10000);
}

TEST_CASE("money avoids the classic floating point rounding error") {
    // 0.1 + 0.2 != 0.3 in binary floating point. In exact minor units it must.
    Money sum;
    CHECK(Money::of(0, 10).tryAdd(Money::of(0, 20), sum));
    CHECK(sum == Money::of(0, 30));
    CHECK_EQ(sum.minorUnits(), 30);
}

TEST_CASE("money parses well-formed decimal strings") {
    Money value;
    CHECK(Money::parse("12.50", value));   CHECK_EQ(value.minorUnits(), 1250);
    CHECK(Money::parse("100", value));     CHECK_EQ(value.minorUnits(), 10000);
    CHECK(Money::parse("0.99", value));    CHECK_EQ(value.minorUnits(), 99);
    CHECK(Money::parse("1.5", value));     CHECK_EQ(value.minorUnits(), 150);
    CHECK(Money::parse("-3.05", value));   CHECK_EQ(value.minorUnits(), -305);
    CHECK(Money::parse("0", value));       CHECK_EQ(value.minorUnits(), 0);
}

TEST_CASE("money rejects malformed strings instead of guessing") {
    Money value;
    CHECK_FALSE(Money::parse("", value));
    CHECK_FALSE(Money::parse("abc", value));
    CHECK_FALSE(Money::parse("1.234", value));   // more than two decimals
    CHECK_FALSE(Money::parse("1.2.3", value));
    CHECK_FALSE(Money::parse("-", value));
    CHECK_FALSE(Money::parse("$5", value));
}

TEST_CASE("money formats with two decimals and thousands separators") {
    CHECK_EQ(Money::of(1234, 50).toString(), std::string("1,234.50"));
    CHECK_EQ(Money::of(0, 5).toString(), std::string("0.05"));
    CHECK_EQ(Money::of(1000000).toString(), std::string("1,000,000.00"));
    CHECK_EQ((-Money::of(12, 30)).toString(), std::string("-12.30"));
}

TEST_CASE("checked addition reports overflow instead of wrapping") {
    Money max(std::numeric_limits<std::int64_t>::max());
    Money out;
    CHECK_FALSE(max.tryAdd(Money(1), out));   // would overflow
    CHECK(max.tryAdd(Money(0), out));         // fits
    CHECK(out == max);
}

TEST_CASE("checked subtraction reports underflow") {
    Money min(std::numeric_limits<std::int64_t>::min());
    Money out;
    CHECK_FALSE(min.trySubtract(Money(1), out));
}

// ------------------------------------------------------------------ Account

TEST_CASE("an opened account carries an opening ledger entry") {
    Account account(1001, "Ada");
    account.open(Money::of(50), "account opened");
    CHECK_EQ(account.balance().minorUnits(), 5000);
    CHECK_EQ(account.ledger().size(), std::size_t(1));
    CHECK(account.ledger().front().type == EntryType::Open);
}

TEST_CASE("applying movements updates the balance and records history") {
    Account account(1001, "Ada");
    account.open(Money::of(100), "opened");
    account.apply(EntryType::Deposit, Money::of(50), 0, "pay");
    account.apply(EntryType::Withdrawal, -Money::of(30), 0, "atm");

    CHECK_EQ(account.balance().minorUnits(), 12000);   // 100 + 50 - 30 = 120.00
    CHECK_EQ(account.ledger().size(), std::size_t(3));
    CHECK(account.verifyLedger());
}

TEST_CASE("verifyLedger catches a balance that does not match its history") {
    Account account(1001, "Ada");
    account.open(Money::of(100), "opened");
    account.apply(EntryType::Deposit, Money::of(50), 0, "pay");
    account.setBalance(Money::of(999));   // tamper
    CHECK_FALSE(account.verifyLedger());
}

// --------------------------------------------------------------------- Bank

TEST_CASE("opening an account assigns sequential ids and stores the owner") {
    Bank bank;
    const Result a = bank.openAccount("Ada");
    const Result b = bank.openAccount("Grace");
    CHECK(a.ok());
    CHECK(b.ok());
    CHECK_EQ(b.accountId - a.accountId, std::int64_t(1));
    CHECK_EQ(bank.find(a.accountId)->owner(), std::string("Ada"));
}

TEST_CASE("opening rejects an empty owner name") {
    Bank bank;
    CHECK(bank.openAccount("   ").status == Status::EmptyOwnerName);
    CHECK_EQ(bank.accountCount(), std::size_t(0));
}

TEST_CASE("opening rejects a negative opening balance") {
    Bank bank;
    CHECK(bank.openAccount("Ada", -Money::of(1)).status == Status::InvalidAmount);
}

TEST_CASE("deposit increases the balance") {
    Bank bank;
    const std::int64_t id = bank.openAccount("Ada").accountId;
    const Result r = bank.deposit(id, Money::of(75, 25));
    CHECK(r.ok());
    CHECK_EQ(r.balance.minorUnits(), 7525);
}

TEST_CASE("deposit and withdrawal reject a non-positive amount") {
    Bank bank;
    const std::int64_t id = bank.openAccount("Ada", Money::of(100)).accountId;
    CHECK(bank.deposit(id, Money::of(0)).status == Status::InvalidAmount);
    CHECK(bank.deposit(id, -Money::of(5)).status == Status::InvalidAmount);
    CHECK(bank.withdraw(id, Money::of(0)).status == Status::InvalidAmount);
}

TEST_CASE("operations on an unknown account are refused") {
    Bank bank;
    CHECK(bank.deposit(9999, Money::of(10)).status == Status::AccountNotFound);
    CHECK(bank.withdraw(9999, Money::of(10)).status == Status::AccountNotFound);
}

TEST_CASE("withdrawal is refused when funds are insufficient") {
    Bank bank;
    const std::int64_t id = bank.openAccount("Ada", Money::of(50)).accountId;
    const Result r = bank.withdraw(id, Money::of(50, 1));   // one cent too much
    CHECK(r.status == Status::InsufficientFunds);
    CHECK_EQ(bank.find(id)->balance().minorUnits(), 5000);   // unchanged
}

TEST_CASE("withdrawing the exact balance is allowed and leaves zero") {
    Bank bank;
    const std::int64_t id = bank.openAccount("Ada", Money::of(50)).accountId;
    CHECK(bank.withdraw(id, Money::of(50)).ok());
    CHECK(bank.find(id)->balance().isZero());
}

TEST_CASE("a transfer moves money between two accounts") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(100)).accountId;
    const std::int64_t b = bank.openAccount("Bob", Money::of(20)).accountId;

    CHECK(bank.transfer(a, b, Money::of(30)).ok());
    CHECK_EQ(bank.find(a)->balance().minorUnits(), 7000);   // 70.00
    CHECK_EQ(bank.find(b)->balance().minorUnits(), 5000);   // 50.00
}

TEST_CASE("a transfer leaves a matching entry in both ledgers") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(100)).accountId;
    const std::int64_t b = bank.openAccount("Bob").accountId;
    bank.transfer(a, b, Money::of(40));

    CHECK(bank.find(a)->ledger().back().type == EntryType::TransferOut);
    CHECK(bank.find(b)->ledger().back().type == EntryType::TransferIn);
    CHECK_EQ(bank.find(a)->ledger().back().counterparty, b);
    CHECK_EQ(bank.find(b)->ledger().back().counterparty, a);
}

TEST_CASE("a failed transfer moves no money at all (atomicity)") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(10)).accountId;
    const std::int64_t b = bank.openAccount("Bob", Money::of(10)).accountId;

    const Result r = bank.transfer(a, b, Money::of(50));   // more than Ada has
    CHECK(r.status == Status::InsufficientFunds);
    CHECK_EQ(bank.find(a)->balance().minorUnits(), 1000);   // both untouched
    CHECK_EQ(bank.find(b)->balance().minorUnits(), 1000);
    CHECK_EQ(bank.find(a)->ledger().size(), std::size_t(1));   // no stray entries
    CHECK_EQ(bank.find(b)->ledger().size(), std::size_t(1));
}

TEST_CASE("a transfer to the same account is refused") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(100)).accountId;
    CHECK(bank.transfer(a, a, Money::of(10)).status == Status::SameAccount);
}

TEST_CASE("a transfer to an unknown destination is refused and reverses nothing") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(100)).accountId;
    CHECK(bank.transfer(a, 9999, Money::of(10)).status == Status::AccountNotFound);
    CHECK_EQ(bank.find(a)->balance().minorUnits(), 10000);
}

TEST_CASE("total deposits is the sum of every balance and money is conserved") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(100)).accountId;
    const std::int64_t b = bank.openAccount("Bob", Money::of(60)).accountId;
    const Money before = bank.totalDeposits();
    CHECK_EQ(before.minorUnits(), 16000);

    bank.transfer(a, b, Money::of(25));   // a transfer must not create or destroy money
    CHECK(bank.totalDeposits() == before);
}

TEST_CASE("the bank reports intact integrity after a series of operations") {
    Bank bank;
    const std::int64_t a = bank.openAccount("Ada", Money::of(500)).accountId;
    const std::int64_t b = bank.openAccount("Bob").accountId;
    bank.deposit(b, Money::of(200));
    bank.withdraw(a, Money::of(50));
    bank.transfer(a, b, Money::of(125, 50));
    CHECK(bank.verifyIntegrity());
}

// -------------------------------------------------------------- Persistence

TEST_CASE("a saved bank reloads to an identical state") {
    const std::string path = tempFile("roundtrip");
    std::remove(path.c_str());

    std::int64_t a = 0, b = 0;
    {
        Bank bank;
        a = bank.openAccount("Ada", Money::of(100)).accountId;
        b = bank.openAccount("Bob", Money::of(50)).accountId;
        bank.deposit(a, Money::of(25, 75));
        bank.transfer(a, b, Money::of(40));

        BankStore store(path);
        std::string error;
        CHECK(store.save(bank, error));
    }

    Bank reloaded;
    BankStore store(path);
    std::string error;
    CHECK(store.load(reloaded, error));

    CHECK_EQ(reloaded.accountCount(), std::size_t(2));
    CHECK_EQ(reloaded.find(a)->balance().minorUnits(), bank::Money::of(85, 75).minorUnits());
    CHECK_EQ(reloaded.find(b)->balance().minorUnits(), 9000);
    CHECK_EQ(reloaded.find(a)->owner(), std::string("Ada"));
    CHECK(reloaded.verifyIntegrity());

    std::remove(path.c_str());
}

TEST_CASE("loading a missing file yields an empty bank, not an error") {
    Bank bank;
    BankStore store("test_bank_does_not_exist.txt");
    std::string error;
    CHECK(store.load(bank, error));
    CHECK_EQ(bank.accountCount(), std::size_t(0));
}

TEST_CASE("ids keep climbing after a reload so a new account cannot collide") {
    const std::string path = tempFile("ids");
    std::remove(path.c_str());

    std::int64_t b = 0;
    {
        Bank bank;
        bank.openAccount("Ada");
        b = bank.openAccount("Bob").accountId;
        BankStore store(path);
        std::string error;
        CHECK(store.save(bank, error));
    }

    Bank reloaded;
    BankStore store(path);
    std::string error;
    CHECK(store.load(reloaded, error));
    const std::int64_t next = reloaded.openAccount("Cleo").accountId;
    CHECK(next > b);

    std::remove(path.c_str());
}

TEST_CASE("a note containing spaces survives a round trip") {
    const std::string path = tempFile("note");
    std::remove(path.c_str());

    std::int64_t a = 0;
    {
        Bank bank;
        a = bank.openAccount("Ada", Money::of(100)).accountId;
        bank.deposit(a, Money::of(10), "salary for March 2026");
        BankStore store(path);
        std::string error;
        CHECK(store.save(bank, error));
    }

    Bank reloaded;
    BankStore store(path);
    std::string error;
    CHECK(store.load(reloaded, error));
    CHECK_EQ(reloaded.find(a)->ledger().back().note, std::string("salary for March 2026"));

    std::remove(path.c_str());
}

int main() { return testing::run(); }
