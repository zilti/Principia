#pragma once

#include "physics/discrete_trajectory.hpp"
#include "physics/trajectory.hpp"

namespace principia {
namespace physics {
namespace internal_apsides {

// Computes the apsides with respect to |reference| for the discrete trajectory
// segment given by |begin| and |end|.  Appends to the given trajectories one
// point for each apsis.
template<typename Frame>
void ComputeApsides(Trajectory<Frame> const& reference,
                    typename DiscreteTrajectory<Frame>::Iterator begin,
                    typename DiscreteTrajectory<Frame>::Iterator end,
                    DiscreteTrajectory<Frame>& apoapsides,
                    DiscreteTrajectory<Frame>& periapsides);

// TODO(egg): when we can usefully iterate over an arbitrary |Trajectory|, move
// the following from |Ephemeris|.
#if 0
template<typename Frame>
void ComputeApsides(Trajectory<Frame> const& trajectory1,
                    Trajectory<Frame> const& trajectory2,
                    DiscreteTrajectory<Frame>& apoapsides1,
                    DiscreteTrajectory<Frame>& periapsides1,
                    DiscreteTrajectory<Frame>& apoapsides2,
                    DiscreteTrajectory<Frame>& periapsides2);
#endif

}  // namespace internal_apsides

using internal_apsides::ComputeApsides;

}  // namespace physics
}  // namespace principia

#include "physics/apsides_body.hpp"
