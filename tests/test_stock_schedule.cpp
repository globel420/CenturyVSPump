#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "../components/vgreen_evo_controller/stock_schedule.h"

namespace {

using vgreen_evo::stock::ScheduleSet;

struct ExpectedProfile {
  ScheduleSet set;
  uint8_t number;
  bool prime_on_start;
  std::array<uint16_t, 24> rpm_by_hour;
};

constexpr ExpectedProfile kExpectedProfiles[] = {
    {ScheduleSet::A, 1U, true,
     {3450, 3450, 2750, 2750, 1750, 1750, 1750, 1750, 1750, 1750,
      1150, 1150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {ScheduleSet::A, 2U, true,
     {3450, 3450, 2850, 2850, 1850, 1850, 1850, 1850, 1850, 1850,
      1250, 1250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {ScheduleSet::A, 3U, true,
     {3450, 3450, 3450, 3450, 1750, 1750, 1750, 1750, 1150, 1150,
      1150, 1150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {ScheduleSet::A, 4U, true,
     {3250, 3250, 1150, 1150, 1150, 1150, 1150, 1150, 1150, 1150,
      3250, 3250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {ScheduleSet::A, 5U, true,
     {1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725,
      1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725,
      1725, 1725, 1725, 1725}},
    {ScheduleSet::A, 6U, false,
     {1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100,
      1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100, 1100,
      1100, 1100, 1100, 1100}},
    {ScheduleSet::A, 7U, false,
     {1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725,
      1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725, 1725,
      1725, 1725, 1725, 1725}},
    {ScheduleSet::A, 8U, false,
     {3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450,
      3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450, 3450,
      3450, 3450, 3450, 3450}},
    {ScheduleSet::B, 1U, true,
     {3450, 3450, 2850, 2850, 2850, 2850, 2250, 2250, 2250, 2250,
      1550, 1550, 1550, 1550, 1550, 1550, 1550, 1550, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 2U, true,
     {3350, 3350, 2750, 2750, 2750, 2750, 2150, 2150, 2150, 2150,
      1450, 1450, 1450, 1450, 1450, 1450, 1450, 1450, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 3U, true,
     {3250, 3250, 2650, 2650, 2650, 2650, 1950, 1950, 1950, 1950,
      1350, 1350, 1350, 1350, 1350, 1350, 1350, 1350, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 4U, true,
     {3150, 3150, 3150, 3150, 2550, 2550, 2550, 2550, 1850, 1850,
      1850, 1850, 1250, 1250, 1250, 1250, 1250, 1250, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 5U, true,
     {3050, 3050, 3050, 3050, 2450, 2450, 2450, 2450, 1750, 1750,
      1750, 1750, 1150, 1150, 1150, 1150, 1150, 1150, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 6U, true,
     {2950, 2950, 2950, 2950, 2350, 2350, 2350, 2350, 1650, 1650,
      1650, 1650, 1050, 1050, 1050, 1050, 1050, 1050, 0, 0, 0, 0,
      0, 0}},
    {ScheduleSet::B, 7U, true,
     {3450, 3450, 2850, 2850, 2850, 2850, 2250, 2250, 2250, 2250,
      1550, 1550, 1550, 1550, 1550, 1550, 1550, 1550, 1550, 1550,
      1550, 1550, 1550, 1550}},
    {ScheduleSet::B, 8U, true,
     {2950, 2950, 2950, 2950, 2350, 2350, 2350, 2350, 2350, 2350,
      1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650, 1050, 1050,
      1050, 1050, 1050, 1050}},
};

int failures = 0;

std::string profile_name(const ExpectedProfile& profile) {
  return std::string(profile.set == ScheduleSet::A ? "A" : "B") +
         std::to_string(profile.number);
}

void expect(bool condition, const std::string& message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void test_constants_and_validation() {
  using namespace vgreen_evo::stock;
  static_assert(static_cast<uint8_t>(ScheduleSet::A) == 0U, "stable A");
  static_assert(static_cast<uint8_t>(ScheduleSet::B) == 1U, "stable B");
  static_assert(kPrimeRpm == 3450U, "manual prime RPM");
  static_assert(kPrimeDurationSeconds == 180U, "manual prime duration");
  static_assert(kSecondsPerDay == 86400U, "24-hour day");

  expect(is_valid_schedule_set(ScheduleSet::A), "Set A must be valid");
  expect(is_valid_schedule_set(ScheduleSet::B), "Set B must be valid");
  expect(!is_valid_schedule_set(static_cast<ScheduleSet>(2U)),
         "Unknown schedule set must be invalid");

  for (uint8_t number = 1U; number <= 8U; ++number) {
    expect(is_valid_profile_number(number),
           "Profile 1-8 must be valid: " + std::to_string(number));
    expect(is_valid_profile(ScheduleSet::A, number),
           "A profile must validate: " + std::to_string(number));
    expect(is_valid_profile(ScheduleSet::B, number),
           "B profile must validate: " + std::to_string(number));
  }

  for (uint8_t number : {uint8_t{0U}, uint8_t{9U}, uint8_t{255U}}) {
    expect(!is_valid_profile_number(number),
           "Out-of-range profile must be invalid: " +
               std::to_string(number));
    expect(find_profile(ScheduleSet::A, number) == nullptr,
           "Invalid A profile lookup must be null");
    expect(find_profile(ScheduleSet::B, number) == nullptr,
           "Invalid B profile lookup must be null");
  }

  const ScheduleSet invalid_set = static_cast<ScheduleSet>(255U);
  expect(find_profile(invalid_set, 1U) == nullptr,
         "Invalid set lookup must be null");
  const ScheduleState invalid = evaluate(invalid_set, 1U, 0U);
  expect(!invalid.valid && invalid.rpm == 0U && !invalid.prime_on_start,
         "Invalid evaluation must fail closed");
}

void test_profile(const ExpectedProfile& expected) {
  using namespace vgreen_evo::stock;
  const std::string name = profile_name(expected);
  const StockScheduleProfile* actual =
      find_profile(expected.set, expected.number);
  expect(actual != nullptr, name + " lookup");
  if (actual == nullptr) return;

  expect(actual->set == expected.set, name + " set identity");
  expect(actual->number == expected.number, name + " profile identity");
  expect(actual->prime_on_start == expected.prime_on_start,
         name + " prime flag");
  expect(requires_priming(expected.set, expected.number) ==
             expected.prime_on_start,
         name + " priming helper");
  expect(profile_table_is_valid(*actual), name + " segment coverage");

  // Check every second of the 24-hour stock cycle, not just sample points.
  for (uint32_t second = 0U; second < kSecondsPerDay; ++second) {
    const uint8_t hour = static_cast<uint8_t>(second / kSecondsPerHour);
    const uint16_t expected_rpm = expected.rpm_by_hour[hour];
    const ScheduleState state =
        evaluate(expected.set, expected.number, second);
    if (!state.valid || state.rpm != expected_rpm || state.hour != hour ||
        state.second_in_day != second ||
        state.prime_on_start != expected.prime_on_start) {
      expect(false, name + " mismatch at second " + std::to_string(second));
      break;
    }
    if (is_scheduled_running(state) != (expected_rpm != 0U)) {
      expect(false,
             name + " running helper mismatch at second " +
                 std::to_string(second));
      break;
    }
  }

  // Validate each manual profile boundary at the last second before, exact
  // boundary, and first second after. Hour 0 also verifies the 24-hour wrap.
  for (uint8_t hour = 0U; hour < 24U; ++hour) {
    const uint8_t previous_hour = hour == 0U ? 23U : hour - 1U;
    if (hour != 0U &&
        expected.rpm_by_hour[hour] == expected.rpm_by_hour[previous_hour]) {
      continue;
    }

    const uint32_t boundary = static_cast<uint32_t>(hour) * kSecondsPerHour;
    const uint64_t previous =
        boundary == 0U ? static_cast<uint64_t>(kSecondsPerDay - 1U)
                       : static_cast<uint64_t>(boundary - 1U);
    const ScheduleState before =
        evaluate(expected.set, expected.number, previous);
    const ScheduleState at =
        evaluate(expected.set, expected.number, boundary);
    const ScheduleState after =
        evaluate(expected.set, expected.number, boundary + 1U);
    expect(before.rpm == expected.rpm_by_hour[previous_hour],
           name + " RPM before hour " + std::to_string(hour));
    expect(at.rpm == expected.rpm_by_hour[hour],
           name + " RPM at hour " + std::to_string(hour));
    expect(after.rpm == expected.rpm_by_hour[hour],
           name + " RPM after hour " + std::to_string(hour));
  }

  // Every second must repeat exactly after one and seven full days.
  for (uint32_t second = 0U; second < kSecondsPerDay; ++second) {
    const uint16_t base =
        evaluate(expected.set, expected.number, second).rpm;
    const uint16_t next_day =
        evaluate(expected.set, expected.number,
                 static_cast<uint64_t>(kSecondsPerDay) + second)
            .rpm;
    const uint16_t next_week =
        evaluate(expected.set, expected.number,
                 7ULL * kSecondsPerDay + second)
            .rpm;
    if (base != next_day || base != next_week) {
      expect(false, name + " 24-hour wrap at second " +
                        std::to_string(second));
      break;
    }
  }
}

void test_priming_matrix() {
  using namespace vgreen_evo::stock;
  for (uint8_t number = 1U; number <= 8U; ++number) {
    expect(requires_priming(ScheduleSet::A, number) == (number <= 5U),
           "Set A prime matrix profile " + std::to_string(number));
    expect(requires_priming(ScheduleSet::B, number),
           "Set B prime matrix profile " + std::to_string(number));
  }
}

void test_large_clock_normalization() {
  using namespace vgreen_evo::stock;
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  expect(normalize_day_seconds(maximum) == maximum % kSecondsPerDay,
         "uint64 clock normalization");
  const ScheduleState state = evaluate(ScheduleSet::A, 1U, maximum);
  expect(state.valid, "large elapsed clock remains valid");
  expect(state.second_in_day == normalize_day_seconds(maximum),
         "large elapsed clock state normalization");
}

}  // namespace

int main() {
  test_constants_and_validation();
  for (const ExpectedProfile& profile : kExpectedProfiles) {
    test_profile(profile);
  }
  test_priming_matrix();
  test_large_clock_normalization();

  if (failures != 0) {
    std::cerr << failures << " stock schedule test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "PASS: all 16 stock profiles, boundaries, priming flags, and "
               "24-hour wrap\n";
  return EXIT_SUCCESS;
}
