# Banking System

[![CI](https://github.com/humaidi09/Banking-System/actions/workflows/ci.yml/badge.svg)](https://github.com/humaidi09/Banking-System/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-29%20passing-brightgreen.svg)](tests/test_bank.cpp)

A file-backed banking back-end in C++17 — accounts, deposits, withdrawals and
transfers — built around the two things a system that handles money must never
get wrong: **every amount is exact**, and **every balance is backed by a ledger
that proves it**.

Zero dependencies. The core does no I/O, so all of it is unit-tested.

## Features

- **Accounts** with sequential numbers and an owner
- **Deposit / withdraw** with validation and overdraft protection
- **Transfers** that are atomic — both sides succeed or neither does
- **A per-account ledger** — every movement is recorded, and the balance is
  re-derivable from history
- **Integrity checking** — a stored balance that does not match its ledger is
  rejected on load, so a tampered or corrupted file cannot pass silently
- **Atomic saves** — a crash mid-write cannot corrupt the data file
- **29 unit tests** and CI across four compiler / OS combinations

## Why money is never a `double`

The most common bug in a beginner banking program is storing money as a
floating-point number. Binary floating point cannot represent most decimal
fractions exactly:

```
0.1 + 0.2 == 0.30000000000000004   // not 0.3
```

Over thousands of transactions those tiny errors accumulate into real
discrepancies. This system stores every amount as a **whole number of minor
units** (cents / paisa) in a 64-bit integer, so `10.00` is the integer `1000`
and arithmetic is exact by construction. The [`Money`](src/money.h) type wraps
that integer and is the only way money enters the system — it parses and formats
decimal strings at the edges, but the value in the middle is always exact.

`Money` also does **checked** arithmetic: adding two amounts that would overflow
a 64-bit integer returns `false` rather than silently wrapping to a negative
balance, so a corrupted or malicious amount cannot fabricate money.

## Why every account keeps a ledger

Each account stores not just a balance but the full list of movements that
produced it — an `OPEN`, then every `DEPOSIT`, `WITHDRAWAL`, `TRANSFER_IN` and
`TRANSFER_OUT`. This is the same idea as double-entry bookkeeping: the balance
is a **derived** quantity, and it must always equal the sum of the history.

`verifyLedger()` replays the entries from zero and checks the running total
against the stored balance at every step. The data store runs this on every
account when loading a file:

```
if (!bank.verifyIntegrity()) {
    error = "a stored balance does not match its history";
    return false;   // refuse to open a tampered or corrupted file
}
```

So editing a balance in the saved file by hand — without a matching ledger
entry — is detected immediately instead of quietly corrupting the books. There
is a test for exactly this, and CI feeds the program a hand-tampered file and
asserts it is refused.

## Transfers are atomic

A transfer is two movements: a debit from one account and a credit to another.
If the debit succeeds but the credit fails, money vanishes. So the transfer
checks **everything that could fail before it moves anything** — both accounts
exist, they differ, the source has the funds, and the destination's new balance
will not overflow — and only then applies both legs:

```
if (from->balance() < amount)              return InsufficientFunds;   // nothing moved
if (!to->balance().tryAdd(amount, out))    return Overflow;            // nothing moved
from->apply(TransferOut, -amount, toId);   // both legs, now that both are safe
to->apply(TransferIn,    +amount, fromId);
```

A test asserts that after a failed transfer both balances and both ledgers are
completely untouched, and that a successful transfer conserves the total money
in the bank.

## Build

```bash
make          # build ./bank
make test     # build and run the 29 unit tests
```

Or without make:

```bash
g++ -std=c++17 -O2 -Isrc src/*.cpp -o bank
```

## Usage

Run with no command for the interactive menu, or pass a command to script it.

```
bank                              use ./bank_data.txt as the data file
bank --db <path>                  use a different data file
bank --help                       show usage
bank --version                    print the version
```

### Scriptable commands

Each command runs one operation, prints the result, saves, and exits with a
status code (`0` success, `1` refused/not found, `2` bad usage) — so it drops
straight into a shell script or CI without driving the menu through a pipe.

```console
$ ./bank --db bank.txt open Ada 100
opened account 1001 for Ada with 100.00
$ ./bank --db bank.txt open Bob 0
opened account 1002 for Bob with 0.00
$ ./bank --db bank.txt deposit 1001 50.50
deposit ok; balance of 1001 is 150.50
$ ./bank --db bank.txt transfer 1001 1002 30
transferred 30.00 from 1001 to 1002; 1001 now holds 120.50
$ ./bank --db bank.txt withdraw 1002 999
error: Insufficient funds for this operation.
$ echo $?
1
```

Available commands: `open <owner> <amount>`, `deposit <id> <amount>`,
`withdraw <id> <amount>`, `transfer <from> <to> <amount>`, `statement <id>`,
`list`.

### Interactive menu

```console
$ ./bank --db examples/demo_bank.txt

=================================================
              B A N K I N G   S Y S T E M
=================================================
Data file: examples/demo_bank.txt  (2 account(s))

  1. Open account
  2. Deposit
  3. Withdraw
  4. Transfer
  5. View statement
  6. List all accounts
  0. Exit
  Choose: 5
  Account number: 1001

  Statement for account 1001 (Ada Lovelace)
  ----------------------------------------------------------
  TYPE          AMOUNT            BALANCE          NOTE
  ----------------------------------------------------------
  OPEN          1,000.00          1,000.00         account opened
  DEPOSIT       500.00            1,500.00         salary
  WITHDRAWAL    150.00            1,350.00         rent
  TRANSFER_OUT  99.25             1,250.75         split dinner (acct 1002)
  ----------------------------------------------------------
  Current balance: 1,250.75
```

## Project layout

```
src/
  money.h/.cpp        exact integer-minor-unit money, parsing, formatting, checked maths
  account.h/.cpp      an account, its balance and its immutable ledger
  bank.h/.cpp         the operations: open / deposit / withdraw / transfer, and invariants
  bank_store.h/.cpp   file-backed persistence with atomic rename-over-write and load-time integrity check
  main.cpp            the CLI: interactive menu and scriptable commands
tests/
  test_framework.h    tiny header-only assertion harness
  test_bank.cpp       29 cases across money, account, bank and persistence
examples/
  demo_bank.txt       a small two-account data file to explore
.github/workflows/ci.yml
```

`Bank` performs **no console or file I/O** — it takes values and returns a
`Result` carrying a status code and the resulting balance. That seam is what
makes every rule here directly testable, and it means the same core could back a
GUI or a web handler unchanged. Returning a status enum rather than a `bool`
keeps the tests honest: they assert on `Status::InsufficientFunds` rather than
matching message text, so rephrasing a message cannot break a check.

### Storage format

Line-oriented and tab-separated: one header line per account followed by one
line per ledger entry. Amounts are integer minor units, so a round trip loses
no precision.

```
ACCOUNT <tab> id <tab> balance <tab> entryCount <tab> owner
ENTRY   <tab> type <tab> amount <tab> balanceAfter <tab> counterparty <tab> note
```

Saves write to `bank_data.txt.tmp` and then rename over the original. Rename is
atomic on a single filesystem, so an interrupted save leaves the previous file
intact rather than a truncated one.

## Testing

```console
$ make test
Running 29 test case(s)

  ok    money avoids the classic floating point rounding error
  ok    checked addition reports overflow instead of wrapping
  ok    a failed transfer moves no money at all (atomicity)
  ok    total deposits is the sum of every balance and money is conserved
  ok    verifyLedger catches a balance that does not match its history
  ok    a saved bank reloads to an identical state
  ...

29 passed, 0 failed, 0 assertion failure(s)
```

Covered: money construction, parsing, formatting and checked overflow; the
account ledger and its integrity check; every `Bank` operation and its failure
modes; transfer atomicity and money conservation; and persistence round-trips
including id continuity and notes with spaces.

## License

[MIT](LICENSE) © Hussain Ahmed Humaidi
