﻿
#include "physics/apsides.hpp"

#include <limits>
#include <map>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "physics/ephemeris.hpp"
#include "quantities/astronomy.hpp"
#include "testing_utilities/almost_equals.hpp"

namespace principia {
namespace physics {
namespace internal_apsides {

using base::not_null;
using geometry::Displacement;
using geometry::Frame;
using geometry::Velocity;
using integrators::DormandElMikkawyPrince1986RKN434FM;
using integrators::McLachlanAtela1992Order5Optimal;
using quantities::GravitationalParameter;
using quantities::Pow;
using quantities::Speed;
using quantities::Time;
using quantities::astronomy::JulianYear;
using quantities::astronomy::SolarMass;
using quantities::constants::GravitationalConstant;
using quantities::si::AstronomicalUnit;
using quantities::si::Hour;
using quantities::si::Kilo;
using quantities::si::Milli;
using quantities::si::Metre;
using quantities::si::Second;
using testing_utilities::AlmostEquals;

class ApsidesTest : public ::testing::Test {
 protected:
  using World =
      Frame<serialization::Frame::TestTag, serialization::Frame::TEST1, true>;
};

TEST_F(ApsidesTest, ComputeApsidesDiscreteTrajectory) {
  Instant const t0;
  GravitationalParameter const μ = GravitationalConstant * SolarMass;
  auto const b = new MassiveBody(μ);

  std::vector<not_null<std::unique_ptr<MassiveBody const>>> bodies;
  std::vector<DegreesOfFreedom<World>> initial_state;
  bodies.emplace_back(std::unique_ptr<MassiveBody const>(b));
  initial_state.emplace_back(World::origin, Velocity<World>());

  Ephemeris<World>
      ephemeris(
          std::move(bodies),
          initial_state,
          t0,
          5 * Milli(Metre),
          Ephemeris<World>::FixedStepParameters(
              McLachlanAtela1992Order5Optimal<Position<World>>(),
              1 * Hour));

  Displacement<World> r(
      {1 * AstronomicalUnit, 2 * AstronomicalUnit, 3 * AstronomicalUnit});
  Length const r_norm = r.Norm();
  Velocity<World> v({4 * Kilo(Metre) / Second,
                     5 * Kilo(Metre) / Second,
                     6 * Kilo(Metre) / Second});
  Speed const v_norm = v.Norm();

  Time const T = 2 * π * Sqrt(-(Pow<3>(r_norm) * Pow<2>(μ) /
                                Pow<3>(r_norm * Pow<2>(v_norm) - 2 * μ)));
  Length const a = -r_norm * μ / (r_norm * Pow<2>(v_norm) - 2 * μ);

  DiscreteTrajectory<World> trajectory;
  trajectory.Append(t0, DegreesOfFreedom<World>(World::origin + r, v));

  ephemeris.FlowWithAdaptiveStep(
      &trajectory,
      Ephemeris<World>::NoIntrinsicAcceleration,
      t0 + 10 * JulianYear,
      Ephemeris<World>::AdaptiveStepParameters(
          DormandElMikkawyPrince1986RKN434FM<Position<World>>(),
          std::numeric_limits<std::int64_t>::max(),
          1e-3 * Metre,
          1e-3 * Metre / Second),
      Ephemeris<World>::unlimited_max_ephemeris_steps,
      /*last_point_only=*/false);

  DiscreteTrajectory<World> apoapsides;
  DiscreteTrajectory<World> periapsides;
  ComputeApsides(*ephemeris.trajectory(b),
                 trajectory.Begin(),
                 trajectory.End(),
                 apoapsides,
                 periapsides);

  std::experimental::optional<Instant> previous_time;
  std::map<Instant, DegreesOfFreedom<World>> all_apsides;
  for (auto it = apoapsides.Begin(); it != apoapsides.End(); ++it) {
    Instant const time = it.time();
    all_apsides.emplace(time, it.degrees_of_freedom());
    if (previous_time) {
      EXPECT_THAT(time - *previous_time, AlmostEquals(T, 118, 2079));
    }
    previous_time = time;
  }

  previous_time = std::experimental::nullopt;
  for (auto it = periapsides.Begin(); it != periapsides.End(); ++it) {
    Instant const time = it.time();
    all_apsides.emplace(time, it.degrees_of_freedom());
    if (previous_time) {
      EXPECT_THAT(time - *previous_time, AlmostEquals(T, 143, 257));
    }
    previous_time = time;
  }

  EXPECT_EQ(6, all_apsides.size());

  previous_time = std::experimental::nullopt;
  std::experimental::optional<Position<World>> previous_position;
  for (auto const pair : all_apsides) {
    Instant const time = pair.first;
    Position<World> const position = pair.second.position();
    if (previous_time) {
      EXPECT_THAT(time - *previous_time,
                  AlmostEquals(0.5 * T, 103, 3567));
      EXPECT_THAT((position - *previous_position).Norm(),
                  AlmostEquals(2.0 * a, 0, 176));
    }
    previous_time = time;
    previous_position = position;
  }
}

}  // namespace internal_apsides
}  // namespace physics
}  // namespace principia
