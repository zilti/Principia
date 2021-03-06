#include "ksp_plugin/pile_up.hpp"

#include <limits>
#include <map>
#include <vector>

#include "ksp_plugin/part.hpp"
#include "ksp_plugin/vessel.hpp"  // For the Default...Parameters.
#include "geometry/named_quantities.hpp"
#include "geometry/r3_element.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "integrators/mock_integrators.hpp"
#include "physics/mock_ephemeris.hpp"
#include "quantities/si.hpp"
#include "testing_utilities/almost_equals.hpp"
#include "testing_utilities/componentwise.hpp"

namespace principia {
namespace ksp_plugin {
namespace internal_pile_up {

using base::make_not_null_unique;
using geometry::Displacement;
using geometry::Position;
using geometry::R3Element;
using geometry::Vector;
using geometry::Velocity;
using integrators::MockFixedStepSizeIntegrator;
using physics::DegreesOfFreedom;
using physics::MassiveBody;
using physics::MockEphemeris;
using quantities::Acceleration;
using quantities::Length;
using quantities::Pow;
using quantities::Speed;
using quantities::Time;
using quantities::si::Kilogram;
using quantities::si::Metre;
using quantities::si::Micro;
using quantities::si::Newton;
using quantities::si::Second;
using testing_utilities::AlmostEquals;
using testing_utilities::Componentwise;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

// A helper class to expose the internal state of a pile-up for testing.
class TestablePileUp : public PileUp {
 public:
  using PileUp::PileUp;
  using RigidPileUp = PileUp::RigidPileUp;

  Mass const& mass() const {
    return mass_;
  }

  Vector<Force, Barycentric> const& intrinsic_force() const {
    return intrinsic_force_;
  }

  not_null<DiscreteTrajectory<Barycentric>*> psychohistory() const {
    return psychohistory_.get();
  }

  PartTo<DegreesOfFreedom<RigidPileUp>> const&
  actual_part_degrees_of_freedom() const {
    return actual_part_degrees_of_freedom_;
  }

  PartTo<DegreesOfFreedom<ApparentBubble>> const&
  apparent_part_degrees_of_freedom() const {
    return apparent_part_degrees_of_freedom_;
  }
};

class PileUpTest : public testing::Test {
 protected:
  using RigidPileUp = TestablePileUp::RigidPileUp;

  PileUpTest()
      : p1_(part_id1_, "p1", mass1_, p1_dof_, /*deletion_callback=*/nullptr),
        p2_(part_id2_, "p2", mass2_, p2_dof_, /*deletion_callback=*/nullptr) {}

  void CheckPreDeformPileUpInvariants(TestablePileUp& pile_up) {
    EXPECT_EQ(3 * Kilogram, pile_up.mass());

    EXPECT_THAT(
        pile_up.psychohistory()->last().degrees_of_freedom(),
        Componentwise(AlmostEquals(Barycentric::origin +
                                       Displacement<Barycentric>(
                                           {13.0 / 3.0 * Metre,
                                            4.0 * Metre,
                                            11.0 / 3.0 * Metre}), 0),
                      AlmostEquals(Velocity<Barycentric>(
                                       {130.0 / 3.0 * Metre / Second,
                                        40.0 * Metre / Second,
                                        110.0 / 3.0 * Metre / Second}), 0)));

    EXPECT_THAT(
        pile_up.actual_part_degrees_of_freedom().at(&p1_),
        Componentwise(AlmostEquals(RigidPileUp::origin +
                                       Displacement<RigidPileUp>(
                                           {-10.0 / 3.0 * Metre,
                                            -2.0 * Metre,
                                            -2.0 / 3.0 * Metre}), 1),
                      AlmostEquals(Velocity<RigidPileUp>(
                                       {-100.0 / 3.0 * Metre / Second,
                                        -20.0 * Metre / Second,
                                        -20.0 / 3.0 * Metre / Second}), 3)));
    EXPECT_THAT(
        pile_up.actual_part_degrees_of_freedom().at(&p2_),
        Componentwise(AlmostEquals(RigidPileUp::origin +
                                       Displacement<RigidPileUp>(
                                           {5.0 / 3.0 * Metre,
                                            1.0 * Metre,
                                            1.0 / 3.0 * Metre}), 3),
                      AlmostEquals(Velocity<RigidPileUp>(
                                       {50.0 / 3.0 * Metre / Second,
                                        10.0 * Metre / Second,
                                        10.0 / 3.0 * Metre / Second}), 5)));

    // Centre of mass of |p1_| and |p2_| in |ApparentBubble|, in SI units:
    //   {1 / 9, -1 / 3, -2 / 9} {10 / 9, -10 / 3, -20 / 9}
    pile_up.SetPartApparentDegreesOfFreedom(
        &p1_,
        DegreesOfFreedom<ApparentBubble>(
            ApparentBubble::origin +
                Displacement<ApparentBubble>({-11.0 / 3.0 * Metre,
                                              -1.0 * Metre,
                                              2.0 / 3.0 * Metre}),
            Velocity<ApparentBubble>({-110.0 / 3.0 * Metre / Second,
                                      -10.0 * Metre / Second,
                                      20.0 / 3.0 * Metre / Second})));
    pile_up.SetPartApparentDegreesOfFreedom(
        &p2_,
        DegreesOfFreedom<ApparentBubble>(
            ApparentBubble::origin +
                Displacement<ApparentBubble>({2.0 * Metre,
                                              0.0 * Metre,
                                              -2.0 / 3.0 * Metre}),
            Velocity<ApparentBubble>({20.0 * Metre / Second,
                                      0.0 * Metre / Second,
                                      -20.0 / 3.0 * Metre / Second})));

    EXPECT_THAT(
        pile_up.apparent_part_degrees_of_freedom().at(&p1_),
        Componentwise(AlmostEquals(ApparentBubble::origin +
                                       Displacement<ApparentBubble>(
                                           {-11.0 / 3.0 * Metre,
                                            -1.0 * Metre,
                                            2.0 / 3.0 * Metre}), 0),
                      AlmostEquals(Velocity<ApparentBubble>(
                                       {-110.0 / 3.0 * Metre / Second,
                                        -10.0 * Metre / Second,
                                        20.0 / 3.0 * Metre / Second}), 0)));
    EXPECT_THAT(
        pile_up.apparent_part_degrees_of_freedom().at(&p2_),
        Componentwise(AlmostEquals(ApparentBubble::origin +
                                       Displacement<ApparentBubble>(
                                           {2.0 * Metre,
                                            0.0 * Metre,
                                            -2.0 / 3.0 * Metre}), 0),
                      AlmostEquals(Velocity<ApparentBubble>(
                                       {20.0 * Metre / Second,
                                        0.0 * Metre / Second,
                                        -20.0 / 3.0 * Metre / Second}), 0)));
  }

  void CheckPreAdvanceTimeInvariants(TestablePileUp& pile_up) {
    EXPECT_THAT(
        pile_up.actual_part_degrees_of_freedom().at(&p1_),
        Componentwise(AlmostEquals(RigidPileUp::origin +
                                       Displacement<RigidPileUp>(
                                           {-34.0 / 9.0 * Metre,
                                            -2.0 / 3.0 * Metre,
                                            8.0 / 9.0 * Metre}), 1),
                      AlmostEquals(Velocity<RigidPileUp>(
                                       {-340.0 / 9.0 * Metre / Second,
                                        -20.0 / 3.0 * Metre / Second,
                                        80.0 / 9.0 * Metre / Second}), 1)));
    EXPECT_THAT(
        pile_up.actual_part_degrees_of_freedom().at(&p2_),
        Componentwise(AlmostEquals(RigidPileUp::origin +
                                       Displacement<RigidPileUp>(
                                           {17.0 / 9.0 * Metre,
                                            1.0 / 3.0 * Metre,
                                            -4.0 / 9.0 * Metre}), 0),
                      AlmostEquals(Velocity<RigidPileUp>(
                                       {170.0 / 9.0 * Metre / Second,
                                        10.0 / 3.0 * Metre / Second,
                                        -40.0 / 9.0 * Metre / Second}), 0)));
    EXPECT_THAT(pile_up.apparent_part_degrees_of_freedom(), IsEmpty());
  }

  PartId const part_id1_ = 111;
  PartId const part_id2_ = 222;
  Mass const mass1_ = 1 * Kilogram;
  Mass const mass2_ = 2 * Kilogram;

  // Centre of mass of |p1_| and |p2_| in |Barycentric|, in SI units:
  //   {13 / 3, 4, 11 / 3} {130 / 3, 40, 110 / 3}
  DegreesOfFreedom<Barycentric> const p1_dof_ = DegreesOfFreedom<Barycentric>(
      Barycentric::origin +
          Displacement<Barycentric>({1 * Metre, 2 * Metre, 3 * Metre}),
      Velocity<Barycentric>(
          {10 * Metre / Second, 20 * Metre / Second, 30 * Metre / Second}));
  DegreesOfFreedom<Barycentric> const p2_dof_ = DegreesOfFreedom<Barycentric>(
      Barycentric::origin +
          Displacement<Barycentric>({6 * Metre, 5 * Metre, 4 * Metre}),
      Velocity<Barycentric>(
          {60 * Metre / Second, 50 * Metre / Second, 40 * Metre / Second}));

  Part p1_;
  Part p2_;
};

// Exercises the entire lifecycle of a |PileUp| that is subject to an intrinsic
// force.
TEST_F(PileUpTest, LifecycleWithIntrinsicForce) {
  MockEphemeris<Barycentric> ephemeris;
  p1_.increment_intrinsic_force(
      Vector<Force, Barycentric>({1 * Newton, 2 * Newton, 3 * Newton}));
  p2_.increment_intrinsic_force(
      Vector<Force, Barycentric>({11 * Newton, 21 * Newton, 31 * Newton}));
  TestablePileUp pile_up({&p1_, &p2_},
                         astronomy::J2000,
                         DefaultProlongationParameters(),
                         DefaultHistoryParameters(),
                         &ephemeris);
  EXPECT_THAT(pile_up.intrinsic_force(),
              AlmostEquals(Vector<Force, Barycentric>(
                  {12 * Newton, 23 * Newton, 34 * Newton}), 0));

  CheckPreDeformPileUpInvariants(pile_up);

  pile_up.DeformPileUpIfNeeded();

  CheckPreAdvanceTimeInvariants(pile_up);

  EXPECT_CALL(ephemeris, FlowWithAdaptiveStep(_, _, _, _, _, _))
      .WillOnce(DoAll(
          AppendToDiscreteTrajectory(DegreesOfFreedom<Barycentric>(
              Barycentric::origin +
                  Displacement<Barycentric>({1.0 * Metre,
                                             14.0 * Metre,
                                             31.0 / 3.0 * Metre}),
              Velocity<Barycentric>({10.0 * Metre / Second,
                                     140.0 * Metre / Second,
                                     310.0 / 3.0 * Metre / Second}))),
          Return(true)));
  pile_up.AdvanceTime(astronomy::J2000 + 1 * Second);

  EXPECT_EQ(1, p1_.tail().Size());
  EXPECT_TRUE(p1_.tail_is_authoritative());
  EXPECT_THAT(
      p1_.tail().last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {-25.0 / 9.0 * Metre,
                                          40.0 / 3.0 * Metre,
                                          101.0 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {-250.0 / 9.0 * Metre / Second,
                                      400.0 / 3.0 * Metre / Second,
                                      1010.0 / 9.0 * Metre / Second}), 1)));
  EXPECT_EQ(1, p2_.tail().Size());
  EXPECT_TRUE(p2_.tail_is_authoritative());
  EXPECT_THAT(
      p2_.tail().last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {26.0 / 9.0 * Metre,
                                          43.0 / 3.0 * Metre,
                                          89.0 / 9.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {260.0 / 9.0 * Metre / Second,
                                      430.0 / 3.0 * Metre / Second,
                                      890.0 / 9.0 * Metre / Second}), 0)));
  EXPECT_EQ(1, pile_up.psychohistory()->Size());
  EXPECT_THAT(
      pile_up.psychohistory()->last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {1.0 * Metre,
                                          14.0 * Metre,
                                          31.0 / 3.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {10.0 * Metre / Second,
                                      140.0 * Metre / Second,
                                      310.0 / 3.0 * Metre / Second}), 0)));

  pile_up.NudgeParts();

  EXPECT_THAT(
      p1_.degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {-25.0 / 9.0 * Metre,
                                          40.0 / 3.0 * Metre,
                                          101.0 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {-250.0 / 9.0 * Metre / Second,
                                      400.0 / 3.0 * Metre / Second,
                                      1010.0 / 9.0 * Metre / Second}), 1)));
  EXPECT_THAT(
      p2_.degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {26.0 / 9.0 * Metre,
                                          43.0 / 3.0 * Metre,
                                          89.0 / 9.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {260.0 / 9.0 * Metre / Second,
                                      430.0 / 3.0 * Metre / Second,
                                      890.0 / 9.0 * Metre / Second}), 0)));
}

// Same as above, but without an intrinsic force.
TEST_F(PileUpTest, LifecycleWithoutIntrinsicForce) {
  MockEphemeris<Barycentric> ephemeris;
  TestablePileUp pile_up({&p1_, &p2_},
                         astronomy::J2000,
                         DefaultProlongationParameters(),
                         DefaultHistoryParameters(),
                         &ephemeris);
  EXPECT_THAT(pile_up.intrinsic_force(),
              AlmostEquals(Vector<Force, Barycentric>(), 0));

  CheckPreDeformPileUpInvariants(pile_up);

  pile_up.DeformPileUpIfNeeded();

  CheckPreAdvanceTimeInvariants(pile_up);

  auto psychohistory = pile_up.psychohistory();
  auto instance = make_not_null_unique<MockFixedStepSizeIntegrator<
      Ephemeris<Barycentric>::NewtonianMotionEquation>::MockInstance>();
  EXPECT_CALL(ephemeris,
              NewInstance(ElementsAre(pile_up.psychohistory()), _, _))
      .WillOnce(Return(ByMove(std::move(instance))));
  EXPECT_CALL(ephemeris, FlowWithFixedStep(_, _))
      .WillOnce(DoAll(
          AppendToDiscreteTrajectory(
              &psychohistory,
              astronomy::J2000 + 0.4 * Second,
              DegreesOfFreedom<Barycentric>(
                  Barycentric::origin +
                      Displacement<Barycentric>(
                          {1.1 * Metre, 14.1 * Metre, 31.1 / 3.0 * Metre}),
                  Velocity<Barycentric>({10.1 * Metre / Second,
                                         140.1 * Metre / Second,
                                         310.1 / 3.0 * Metre / Second}))),
          AppendToDiscreteTrajectory(
              &psychohistory,
              astronomy::J2000 + 0.8 * Second,
              DegreesOfFreedom<Barycentric>(
                  Barycentric::origin +
                      Displacement<Barycentric>(
                          {1.2 * Metre, 14.2 * Metre, 31.2 / 3.0 * Metre}),
                  Velocity<Barycentric>({10.2 * Metre / Second,
                                         140.2 * Metre / Second,
                                         310.2 / 3.0 * Metre / Second})))));
  EXPECT_CALL(ephemeris, FlowWithAdaptiveStep(_, _, _, _, _, _))
      .WillOnce(DoAll(
          AppendToDiscreteTrajectory(DegreesOfFreedom<Barycentric>(
              Barycentric::origin +
                  Displacement<Barycentric>({1.0 * Metre,
                                             14.0 * Metre,
                                             31.0 / 3.0 * Metre}),
              Velocity<Barycentric>({10.0 * Metre / Second,
                                     140.0 * Metre / Second,
                                     310.0 / 3.0 * Metre / Second}))),
          Return(true)));
  pile_up.AdvanceTime(astronomy::J2000 + 1 * Second);

  EXPECT_EQ(3, p1_.tail().Size());
  EXPECT_FALSE(p1_.tail_is_authoritative());
  EXPECT_THAT(
      p1_.tail().Begin().degrees_of_freedom(),
      Componentwise(
          AlmostEquals(Barycentric::origin +
                           Displacement<Barycentric>({-24.1 / 9.0 * Metre,
                                                      40.3 / 3.0 * Metre,
                                                      101.3 / 9.0 * Metre}),
                       1),
          AlmostEquals(Velocity<Barycentric>({-249.1 / 9.0 * Metre / Second,
                                              400.3 / 3.0 * Metre / Second,
                                              1010.3 / 9.0 * Metre / Second}),
                       1)));
  EXPECT_THAT(
      (++p1_.tail().Begin()).degrees_of_freedom(),
      Componentwise(
          AlmostEquals(Barycentric::origin +
                           Displacement<Barycentric>({-23.2 / 9.0 * Metre,
                                                      40.6 / 3.0 * Metre,
                                                      101.6 / 9.0 * Metre}),
                       1),
          AlmostEquals(Velocity<Barycentric>({-248.2 / 9.0 * Metre / Second,
                                              400.6 / 3.0 * Metre / Second,
                                              1010.6 / 9.0 * Metre / Second}),
                       1)));
  EXPECT_THAT(
      p1_.tail().last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {-25.0 / 9.0 * Metre,
                                          40.0 / 3.0 * Metre,
                                          101.0 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {-250.0 / 9.0 * Metre / Second,
                                      400.0 / 3.0 * Metre / Second,
                                      1010.0 / 9.0 * Metre / Second}), 1)));
  EXPECT_EQ(3, p2_.tail().Size());
  EXPECT_FALSE(p2_.tail_is_authoritative());
  EXPECT_THAT(
      p2_.tail().Begin().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {26.9 / 9.0 * Metre,
                                          43.3 / 3.0 * Metre,
                                          89.3 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {260.9 / 9.0 * Metre / Second,
                                      430.3 / 3.0 * Metre / Second,
                                      890.3 / 9.0 * Metre / Second}), 1)));
  EXPECT_THAT(
      (++p2_.tail().Begin()).degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {27.8 / 9.0 * Metre,
                                          43.6 / 3.0 * Metre,
                                          89.6 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {261.8 / 9.0 * Metre / Second,
                                      430.6 / 3.0 * Metre / Second,
                                      890.6 / 9.0 * Metre / Second}), 1)));
  EXPECT_THAT(
      p2_.tail().last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {26.0 / 9.0 * Metre,
                                          43.0 / 3.0 * Metre,
                                          89.0 / 9.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {260.0 / 9.0 * Metre / Second,
                                      430.0 / 3.0 * Metre / Second,
                                      890.0 / 9.0 * Metre / Second}), 0)));
  EXPECT_EQ(2, pile_up.psychohistory()->Size());
  EXPECT_THAT(
      pile_up.psychohistory()->Begin().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {1.2 * Metre,
                                          14.2 * Metre,
                                          31.2 / 3.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {10.2 * Metre / Second,
                                      140.2 * Metre / Second,
                                      310.2 / 3.0 * Metre / Second}), 0)));
  EXPECT_THAT(
      pile_up.psychohistory()->last().degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {1.0 * Metre,
                                          14.0 * Metre,
                                          31.0 / 3.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {10.0 * Metre / Second,
                                      140.0 * Metre / Second,
                                      310.0 / 3.0 * Metre / Second}), 0)));

  pile_up.NudgeParts();

  EXPECT_THAT(
      p1_.degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {-25.0 / 9.0 * Metre,
                                          40.0 / 3.0 * Metre,
                                          101.0 / 9.0 * Metre}), 1),
                    AlmostEquals(Velocity<Barycentric>(
                                     {-250.0 / 9.0 * Metre / Second,
                                      400.0 / 3.0 * Metre / Second,
                                      1010.0 / 9.0 * Metre / Second}), 1)));
  EXPECT_THAT(
      p2_.degrees_of_freedom(),
      Componentwise(AlmostEquals(Barycentric::origin +
                                     Displacement<Barycentric>(
                                         {26.0 / 9.0 * Metre,
                                          43.0 / 3.0 * Metre,
                                          89.0 / 9.0 * Metre}), 0),
                    AlmostEquals(Velocity<Barycentric>(
                                     {260.0 / 9.0 * Metre / Second,
                                      430.0 / 3.0 * Metre / Second,
                                      890.0 / 9.0 * Metre / Second}), 0)));
}

TEST_F(PileUpTest, MidStepIntrinsicForce) {
  // An empty ephemeris; the parameters don't matter, since there are no bodies
  // to integrate.
  // NOTE(egg): ... except we have to put a body because |Ephemeris| doesn't
  // want to be empty.  We put a tiny one very far.
  std::vector<not_null<std::unique_ptr<MassiveBody const>>> bodies;
  bodies.emplace_back(make_not_null_unique<MassiveBody>(1 * Kilogram));
  std::vector<DegreesOfFreedom<Barycentric>> initial_state{
      DegreesOfFreedom<Barycentric>{
          Barycentric::origin +
              Displacement<Barycentric>(
                  {std::pow(2, 100) * Metre, 0 * Metre, 0 * Metre}),
          Velocity<Barycentric>{}}};
  Ephemeris<Barycentric> ephemeris{
      std::move(bodies),
      initial_state,
      /*initial_time=*/astronomy::J2000,
      /*fitting_tolerance=*/1 * Metre,
      Ephemeris<Barycentric>::FixedStepParameters{
          integrators::BlanesMoan2002SRKN6B<Position<Barycentric>>(),
          1 * Second}};

  Time const fixed_step = 10 * Second;
  Ephemeris<Barycentric>::FixedStepParameters fixed_parameters{
      integrators::BlanesMoan2002SRKN6B<Position<Barycentric>>(), fixed_step};
  Ephemeris<Barycentric>::AdaptiveStepParameters adaptive_parameters{
      integrators::DormandElMikkawyPrince1986RKN434FM<Position<Barycentric>>(),
      /*max_steps=*/std::numeric_limits<std::int64_t>::max(),
      /*length_integration_tolerance*/ 1 * Micro(Metre),
      /*speed_integration_tolerance=*/1 * Micro(Metre) / Second};

  TestablePileUp pile_up({&p1_}, astronomy::J2000,
                         DefaultProlongationParameters(),
                         DefaultHistoryParameters(),
                         &ephemeris);
  Velocity<Barycentric> const old_velocity =
      p1_.degrees_of_freedom().velocity();

  pile_up.AdvanceTime(astronomy::J2000 + 1.5 * fixed_step);
  pile_up.NudgeParts();
  EXPECT_THAT(p1_.degrees_of_freedom().velocity(), Eq(old_velocity));

  Vector<Acceleration, Barycentric> const a{{1729 * Metre / Pow<2>(Second),
                                             -168 * Metre / Pow<2>(Second),
                                             504 * Metre / Pow<2>(Second)}};
  pile_up.set_intrinsic_force(p1_.mass() * a);
  pile_up.AdvanceTime(astronomy::J2000 + 2 * fixed_step);
  pile_up.NudgeParts();
  EXPECT_THAT(p1_.degrees_of_freedom().velocity(),
              AlmostEquals(old_velocity + 0.5 * fixed_step * a, 1));
}

TEST_F(PileUpTest, Serialization) {
  MockEphemeris<Barycentric> ephemeris;
  p1_.increment_intrinsic_force(
      Vector<Force, Barycentric>({1 * Newton, 2 * Newton, 3 * Newton}));
  p2_.increment_intrinsic_force(
      Vector<Force, Barycentric>({11 * Newton, 21 * Newton, 31 * Newton}));
  TestablePileUp pile_up({&p1_, &p2_},
                         astronomy::J2000,
                         DefaultProlongationParameters(),
                         DefaultHistoryParameters(),
                         &ephemeris);

  serialization::PileUp message;
  pile_up.WriteToMessage(&message);

  EXPECT_EQ(2, message.part_id_size());
  EXPECT_EQ(part_id1_, message.part_id(0));
  EXPECT_EQ(part_id2_, message.part_id(1));
  EXPECT_EQ(1, message.psychohistory().timeline_size());
  EXPECT_EQ(2, message.actual_part_degrees_of_freedom().size());
  EXPECT_TRUE(message.apparent_part_degrees_of_freedom().empty());

  auto const part_id_to_part = [this](PartId const part_id) {
    if (part_id == part_id1_) {
      return &p1_;
    }
    if (part_id == part_id2_) {
      return &p2_;
    }
    LOG(FATAL) << "Unexpected part id " << part_id;
    base::noreturn();
  };
  auto const p = PileUp::ReadFromMessage(message, part_id_to_part, &ephemeris);

  serialization::PileUp second_message;
  p.WriteToMessage(&second_message);
  EXPECT_EQ(message.SerializeAsString(), second_message.SerializeAsString());
}

}  // namespace internal_pile_up
}  // namespace ksp_plugin
}  // namespace principia
