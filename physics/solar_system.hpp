﻿
#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>

#include "base/not_null.hpp"
#include "integrators/ordinary_differential_equations.hpp"
#include "physics/continuous_trajectory.hpp"
#include "physics/degrees_of_freedom.hpp"
#include "physics/ephemeris.hpp"
#include "physics/massive_body.hpp"
#include "serialization/astronomy.pb.h"

namespace principia {
namespace physics {
namespace internal_solar_system {

using base::not_null;
using geometry::Instant;
using quantities::GravitationalParameter;
using quantities::Length;

template<typename Frame>
class SolarSystem final {
 public:
  // Initializes this object from the given files, which must contain text
  // format for SolarSystemFile protocol buffers.
  void Initialize(
      std::experimental::filesystem::path const& gravity_model_filename,
      std::experimental::filesystem::path const& initial_state_filename);

  // Constructs an ephemeris for this object using the specified parameters.
  // The bodies and initial state are constructed from the data passed to
  // |Initialize|.
  std::unique_ptr<Ephemeris<Frame>> MakeEphemeris(
      Length const& fitting_tolerance,
      typename Ephemeris<Frame>::FixedStepParameters const& parameters);

  // The time origin for the initial state.
  Instant const& epoch() const;

  // The names of the bodies, sorted alphabetically.
  std::vector<std::string> const& names() const;

  // The index of the body named |name| in the vector |names()| and in the
  // bodies of the ephemeris.
  int index(std::string const& name) const;

  // The initial state of the body named |name|.
  DegreesOfFreedom<Frame> initial_state(std::string const& name) const;

  // The gravitational parameter of the body named |name|.
  GravitationalParameter gravitational_parameter(std::string const& name) const;

  // The mean radius of the body named |name|.
  Length mean_radius(std::string const& name) const;

  // The |MassiveBody| for the body named |name|, extracted from the given
  // |ephemeris|.
  not_null<MassiveBody const*> massive_body(Ephemeris<Frame> const& ephemeris,
                                            std::string const& name) const;

  // The |ContinuousTrajectory| for the body named |name|, extracted from the
  // given |ephemeris|.
  ContinuousTrajectory<Frame> const& trajectory(
      Ephemeris<Frame> const& ephemeris,
      std::string const& name) const;

  // The configuration protocol buffers for the body named |name|.
  serialization::GravityModel::Body const& gravity_model_message(
      std::string const& name) const;
  serialization::InitialState::Body const& initial_state_message(
      std::string const& name) const;

  // Factory functions for converting configuration protocol buffers into
  // structured objects.
  static DegreesOfFreedom<Frame> MakeDegreesOfFreedom(
      serialization::InitialState::Body const& body);
  static not_null<std::unique_ptr<MassiveBody>> MakeMassiveBody(
      serialization::GravityModel::Body const& body);
  static not_null<std::unique_ptr<RotatingBody<Frame>>> MakeRotatingBody(
      serialization::GravityModel::Body const& body);
  static not_null<std::unique_ptr<OblateBody<Frame>>> MakeOblateBody(
      serialization::GravityModel::Body const& body);

  // Utilities for patching the internal protocol buffers after initialization.
  // Should only be used in tests.
  void RemoveMassiveBody(std::string const& name);
  void RemoveOblateness(std::string const& name);

 private:
  // Fails if the given |body| doesn't have a consistent set of fields.
  static void Check(serialization::GravityModel::Body const& body);

  // Factory functions for the parameter classes of the bodies.
  static not_null<std::unique_ptr<MassiveBody::Parameters>>
  MakeMassiveBodyParameters(serialization::GravityModel::Body const& body);
  static not_null<std::unique_ptr<typename RotatingBody<Frame>::Parameters>>
  MakeRotatingBodyParameters(serialization::GravityModel::Body const& body);
  static not_null<std::unique_ptr<typename OblateBody<Frame>::Parameters>>
  MakeOblateBodyParameters(serialization::GravityModel::Body const& body);

  std::vector<not_null<std::unique_ptr<MassiveBody const>>>
  MakeAllMassiveBodies();
  std::vector<DegreesOfFreedom<Frame>> MakeAllDegreesOfFreedom();

  // Note that the maps below hold pointers into these protocol buffers.
  serialization::SolarSystemFile gravity_model_;
  serialization::SolarSystemFile initial_state_;

  Instant epoch_;
  std::vector<std::string> names_;
  std::map<std::string,
           serialization::GravityModel::Body*> gravity_model_map_;
  std::map<std::string,
           serialization::InitialState::Body const*> initial_state_map_;
};

}  // namespace internal_solar_system

using internal_solar_system::SolarSystem;

}  // namespace physics
}  // namespace principia

#include "physics/solar_system_body.hpp"
