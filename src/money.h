#ifndef MONEY_H
#define MONEY_H

#include <cstdint>
#include <string>

namespace bank {

// Money is stored as a whole number of minor units (cents / paisa), never as a
// floating-point value. 0.1 + 0.2 != 0.3 in binary floating point, and that
// rounding error is unacceptable for currency, so every amount in the system is
// an exact integer count of the smallest denomination.
//
// A Money holds a signed 64-bit count of minor units, which spans about ±92
// quadrillion units (±922 trillion in a two-decimal currency) — far beyond any
// balance this system needs, while making overflow checks straightforward.
class Money {
public:
    Money() = default;
    explicit Money(std::int64_t minorUnits) : units_(minorUnits) {}

    // Builds a Money from whole units and minor units, e.g. Money::of(12, 50)
    // is 12.50. `minor` is expected in [0, 99]; values outside are folded in.
    static Money of(std::int64_t major, std::int64_t minor = 0);

    // Parses a decimal string such as "12.50", "-3.05", "100" or "0.99".
    // Returns false on anything malformed rather than guessing.
    static bool parse(const std::string& text, Money& out);

    std::int64_t minorUnits() const { return units_; }
    bool isNegative() const { return units_ < 0; }
    bool isZero() const { return units_ == 0; }

    // Formatted with two decimals and a thousands separator, e.g. "1,234.50".
    std::string toString() const;

    // Checked arithmetic: returns false on 64-bit overflow instead of wrapping
    // silently, so a corrupted or malicious amount cannot fabricate money.
    bool tryAdd(Money other, Money& out) const;
    bool trySubtract(Money other, Money& out) const;

    Money operator+(Money other) const { return Money(units_ + other.units_); }
    Money operator-(Money other) const { return Money(units_ - other.units_); }
    Money operator-() const { return Money(-units_); }

    bool operator==(Money other) const { return units_ == other.units_; }
    bool operator!=(Money other) const { return units_ != other.units_; }
    bool operator<(Money other) const { return units_ < other.units_; }
    bool operator<=(Money other) const { return units_ <= other.units_; }
    bool operator>(Money other) const { return units_ > other.units_; }
    bool operator>=(Money other) const { return units_ >= other.units_; }

private:
    std::int64_t units_ = 0;
};

}  // namespace bank

#endif  // MONEY_H
