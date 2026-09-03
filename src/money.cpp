#include "money.h"

#include <cstdlib>
#include <limits>

namespace bank {

namespace {
constexpr std::int64_t kMinorPerMajor = 100;  // two-decimal currency
}

Money Money::of(std::int64_t major, std::int64_t minor) {
    return Money(major * kMinorPerMajor + minor);
}

bool Money::parse(const std::string& text, Money& out) {
    if (text.empty()) return false;

    std::size_t i = 0;
    bool negative = false;
    if (text[i] == '+' || text[i] == '-') {
        negative = (text[i] == '-');
        ++i;
    }
    if (i == text.size()) return false;   // a lone sign is not a number

    std::int64_t major = 0;
    bool anyDigit = false;
    for (; i < text.size() && text[i] != '.'; ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        major = major * 10 + (text[i] - '0');
        anyDigit = true;
    }

    std::int64_t minor = 0;
    if (i < text.size() && text[i] == '.') {
        ++i;
        int digits = 0;
        for (; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') return false;
            if (digits < 2) {
                minor = minor * 10 + (text[i] - '0');
                ++digits;
            } else {
                return false;   // more than two decimal places is not representable
            }
            anyDigit = true;
        }
        if (digits == 1) minor *= 10;   // "1.5" means 1.50
    }

    if (!anyDigit) return false;

    const std::int64_t total = major * kMinorPerMajor + minor;
    out = Money(negative ? -total : total);
    return true;
}

std::string Money::toString() const {
    std::int64_t value = units_;
    const bool negative = value < 0;
    if (negative) value = -value;

    const std::int64_t major = value / kMinorPerMajor;
    const std::int64_t minor = value % kMinorPerMajor;

    // Group the integer part into thousands.
    std::string majorStr = std::to_string(major);
    std::string grouped;
    int count = 0;
    for (auto it = majorStr.rbegin(); it != majorStr.rend(); ++it) {
        if (count != 0 && count % 3 == 0) grouped.push_back(',');
        grouped.push_back(*it);
        ++count;
    }
    std::string result(grouped.rbegin(), grouped.rend());

    result.push_back('.');
    result.push_back(static_cast<char>('0' + minor / 10));
    result.push_back(static_cast<char>('0' + minor % 10));

    if (negative) result.insert(result.begin(), '-');
    return result;
}

bool Money::tryAdd(Money other, Money& out) const {
    const std::int64_t a = units_;
    const std::int64_t b = other.units_;
    if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) return false;
    if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) return false;
    out = Money(a + b);
    return true;
}

bool Money::trySubtract(Money other, Money& out) const {
    return tryAdd(-other, out);
}

}  // namespace bank
