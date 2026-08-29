#pragma once

#include <cstddef>
#include <cstdint>

namespace vgreen_evo {
namespace stock {

// Values transcribed from the VGreen Evo manual, sections 4.5.1 and 4.5.2.
// The caller owns the boot-relative clock. RPM 0 is a scheduled stop.
constexpr uint32_t kSecondsPerHour = 60U * 60U;
constexpr uint32_t kSecondsPerDay = 24U * kSecondsPerHour;
constexpr uint16_t kPrimeRpm = 3450U;
constexpr uint32_t kPrimeDurationSeconds = 3U * 60U;
constexpr uint8_t kFirstProfileNumber = 1U;
constexpr uint8_t kLastProfileNumber = 8U;

// These numeric values are part of the controller-facing API. Keep them stable.
enum class ScheduleSet : uint8_t {
  A = 0U,
  B = 1U,
};

struct ScheduleSegment {
  uint8_t start_hour;
  uint8_t end_hour;
  uint16_t rpm;
};

struct StockScheduleProfile {
  ScheduleSet set;
  uint8_t number;
  bool prime_on_start;
  const ScheduleSegment* segments;
  size_t segment_count;
};

struct ScheduleState {
  bool valid;
  uint16_t rpm;
  bool prime_on_start;
  uint8_t hour;
  uint32_t second_in_day;
};

template <typename T, size_t N>
constexpr size_t array_size(const T (&)[N]) {
  return N;
}

// Schedule Set A: dip-switch selection and physical behavior are deliberately
// outside this pure lookup library.
constexpr ScheduleSegment kA1[] = {
    {0U, 2U, 3450U}, {2U, 4U, 2750U}, {4U, 10U, 1750U},
    {10U, 12U, 1150U}, {12U, 24U, 0U},
};
constexpr ScheduleSegment kA2[] = {
    {0U, 2U, 3450U}, {2U, 4U, 2850U}, {4U, 10U, 1850U},
    {10U, 12U, 1250U}, {12U, 24U, 0U},
};
constexpr ScheduleSegment kA3[] = {
    {0U, 4U, 3450U}, {4U, 8U, 1750U}, {8U, 12U, 1150U},
    {12U, 24U, 0U},
};
constexpr ScheduleSegment kA4[] = {
    {0U, 2U, 3250U}, {2U, 10U, 1150U}, {10U, 12U, 3250U},
    {12U, 24U, 0U},
};
constexpr ScheduleSegment kA5[] = {{0U, 24U, 1725U}};
constexpr ScheduleSegment kA6[] = {{0U, 24U, 1100U}};
constexpr ScheduleSegment kA7[] = {{0U, 24U, 1725U}};
constexpr ScheduleSegment kA8[] = {{0U, 24U, 3450U}};

// Schedule Set B: California Energy Commission 2021 compliant profiles.
constexpr ScheduleSegment kB1[] = {
    {0U, 2U, 3450U}, {2U, 6U, 2850U}, {6U, 10U, 2250U},
    {10U, 18U, 1550U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB2[] = {
    {0U, 2U, 3350U}, {2U, 6U, 2750U}, {6U, 10U, 2150U},
    {10U, 18U, 1450U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB3[] = {
    {0U, 2U, 3250U}, {2U, 6U, 2650U}, {6U, 10U, 1950U},
    {10U, 18U, 1350U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB4[] = {
    {0U, 4U, 3150U}, {4U, 8U, 2550U}, {8U, 12U, 1850U},
    {12U, 18U, 1250U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB5[] = {
    {0U, 4U, 3050U}, {4U, 8U, 2450U}, {8U, 12U, 1750U},
    {12U, 18U, 1150U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB6[] = {
    {0U, 4U, 2950U}, {4U, 8U, 2350U}, {8U, 12U, 1650U},
    {12U, 18U, 1050U}, {18U, 24U, 0U},
};
constexpr ScheduleSegment kB7[] = {
    {0U, 2U, 3450U}, {2U, 6U, 2850U}, {6U, 10U, 2250U},
    {10U, 24U, 1550U},
};
constexpr ScheduleSegment kB8[] = {
    {0U, 4U, 2950U}, {4U, 10U, 2350U}, {10U, 18U, 1650U},
    {18U, 24U, 1050U},
};

constexpr StockScheduleProfile kProfiles[] = {
    {ScheduleSet::A, 1U, true, kA1, array_size(kA1)},
    {ScheduleSet::A, 2U, true, kA2, array_size(kA2)},
    {ScheduleSet::A, 3U, true, kA3, array_size(kA3)},
    {ScheduleSet::A, 4U, true, kA4, array_size(kA4)},
    {ScheduleSet::A, 5U, true, kA5, array_size(kA5)},
    {ScheduleSet::A, 6U, false, kA6, array_size(kA6)},
    {ScheduleSet::A, 7U, false, kA7, array_size(kA7)},
    {ScheduleSet::A, 8U, false, kA8, array_size(kA8)},
    {ScheduleSet::B, 1U, true, kB1, array_size(kB1)},
    {ScheduleSet::B, 2U, true, kB2, array_size(kB2)},
    {ScheduleSet::B, 3U, true, kB3, array_size(kB3)},
    {ScheduleSet::B, 4U, true, kB4, array_size(kB4)},
    {ScheduleSet::B, 5U, true, kB5, array_size(kB5)},
    {ScheduleSet::B, 6U, true, kB6, array_size(kB6)},
    {ScheduleSet::B, 7U, true, kB7, array_size(kB7)},
    {ScheduleSet::B, 8U, true, kB8, array_size(kB8)},
};

constexpr bool is_valid_schedule_set(ScheduleSet set) {
  return set == ScheduleSet::A || set == ScheduleSet::B;
}

constexpr bool is_valid_profile_number(uint8_t number) {
  return number >= kFirstProfileNumber && number <= kLastProfileNumber;
}

constexpr bool is_valid_profile(ScheduleSet set, uint8_t number) {
  return is_valid_schedule_set(set) && is_valid_profile_number(number);
}

constexpr const StockScheduleProfile* find_profile(ScheduleSet set,
                                                    uint8_t number) {
  if (!is_valid_profile(set, number)) return nullptr;
  for (size_t index = 0U; index < array_size(kProfiles); ++index) {
    if (kProfiles[index].set == set && kProfiles[index].number == number) {
      return &kProfiles[index];
    }
  }
  return nullptr;
}

constexpr uint32_t normalize_day_seconds(uint64_t elapsed_seconds) {
  return static_cast<uint32_t>(elapsed_seconds % kSecondsPerDay);
}

constexpr ScheduleState evaluate(ScheduleSet set, uint8_t number,
                                 uint64_t elapsed_seconds) {
  const StockScheduleProfile* profile = find_profile(set, number);
  const uint32_t second_in_day = normalize_day_seconds(elapsed_seconds);
  const uint8_t hour =
      static_cast<uint8_t>(second_in_day / kSecondsPerHour);
  if (profile == nullptr) {
    return {false, 0U, false, hour, second_in_day};
  }

  for (size_t index = 0U; index < profile->segment_count; ++index) {
    const ScheduleSegment& segment = profile->segments[index];
    if (hour >= segment.start_hour && hour < segment.end_hour) {
      return {true, segment.rpm, profile->prime_on_start, hour,
              second_in_day};
    }
  }
  return {false, 0U, profile->prime_on_start, hour, second_in_day};
}

constexpr bool requires_priming(ScheduleSet set, uint8_t number) {
  const StockScheduleProfile* profile = find_profile(set, number);
  return profile != nullptr && profile->prime_on_start;
}

constexpr bool is_scheduled_running(const ScheduleState& state) {
  return state.valid && state.rpm != 0U;
}

constexpr bool profile_table_is_valid(const StockScheduleProfile& profile) {
  if (!is_valid_profile(profile.set, profile.number) ||
      profile.segments == nullptr || profile.segment_count == 0U) {
    return false;
  }

  uint8_t expected_start = 0U;
  for (size_t index = 0U; index < profile.segment_count; ++index) {
    const ScheduleSegment& segment = profile.segments[index];
    if (segment.start_hour != expected_start ||
        segment.end_hour <= segment.start_hour || segment.end_hour > 24U) {
      return false;
    }
    expected_start = segment.end_hour;
  }
  return expected_start == 24U;
}

constexpr bool all_profile_tables_are_valid() {
  if (array_size(kProfiles) != 16U) return false;
  for (size_t index = 0U; index < array_size(kProfiles); ++index) {
    if (!profile_table_is_valid(kProfiles[index])) return false;
    for (size_t other = index + 1U; other < array_size(kProfiles); ++other) {
      if (kProfiles[index].set == kProfiles[other].set &&
          kProfiles[index].number == kProfiles[other].number) {
        return false;
      }
    }
  }
  return true;
}

static_assert(static_cast<uint8_t>(ScheduleSet::A) == 0U,
              "ScheduleSet::A value is part of the persisted API");
static_assert(static_cast<uint8_t>(ScheduleSet::B) == 1U,
              "ScheduleSet::B value is part of the persisted API");
static_assert(all_profile_tables_are_valid(),
              "Stock schedule tables must cover each 24-hour day exactly");

}  // namespace stock
}  // namespace vgreen_evo
