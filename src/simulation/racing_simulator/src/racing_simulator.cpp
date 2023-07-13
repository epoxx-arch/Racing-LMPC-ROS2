// Copyright 2023 Haoru Xue
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <math.h>
#include <exception>
#include <vector>
#include <iostream>
#include <chrono>

#include "racing_simulator/racing_simulator.hpp"
#include "lmpc_utils/utils.hpp"

namespace lmpc
{
namespace simulation
{
namespace racing_simulator
{
RacingSimulator::RacingSimulator(
  const double & dt,
  const casadi::DM & x0,
  RacingTrajectory::SharedPtr track,
  SingleTrackPlanarModel::SharedPtr model)
: dt_(dt),
  x_(x0),
  track_(track),
  model_(model)
{
  // check if dt is positive
  if (dt <= 0) {
    throw std::invalid_argument("dt must be positive");
  }

  // build discrete dynamics
  const auto x_sym = casadi::MX::sym("x", model_->nx());
  const auto u_sym = casadi::MX::sym("u", model_->nu());
  const auto k = track_->curvature_interpolation_function()(x_sym(XIndex::PX))[0];

  const auto out1 = model_->dynamics()(casadi::MXDict{{"x", x_sym}, {"u", u_sym}, {"k", k}});
  const auto k1 = out1.at("x_dot");
  const auto out2 = model_->dynamics()({{"x", x_sym + dt_ / 2.0 * k1}, {"u", u_sym}, {"k", k}});
  const auto k2 = out2.at("x_dot");
  const auto out3 = model_->dynamics()({{"x", x_sym + dt_ / 2.0 * k2}, {"u", u_sym}, {"k", k}});
  const auto k3 = out3.at("x_dot");
  const auto out4 = model_->dynamics()({{"x", x_sym + dt_ * k3}, {"u", u_sym}, {"k", k}});
  const auto k4 = out4.at("x_dot");
  const auto out = x_sym + dt_ / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
  discrete_dynamics_ = casadi::Function("discrete_dynamics", {x_sym, u_sym}, {out});
}

SingleTrackPlanarModel & RacingSimulator::get_model()
{
  return *model_;
}

RacingTrajectory & RacingSimulator::get_track()
{
  return *track_;
}

const casadi::DM & RacingSimulator::x() const
{
  return x_;
}

const casadi::DM & RacingSimulator::u() const
{
  return u_;
}

void RacingSimulator::set_state(const casadi::DM & x)
{
  x_ = x;
}

void RacingSimulator::step(const casadi::DM & u)
{
  // velocity cannot be exactly zero for single track planar model
  const auto v_val = static_cast<double>(x_(XIndex::V));
  if (abs(v_val) < 1e-6) {
    x_(XIndex::V) = std::copysign(1e-6, v_val);
  }

  // update state
  u_ = u;
  x_ = discrete_dynamics_(casadi::DMVector{x_, u_})[0];
}
}  // namespace racing_simulator
}  // namespace simulation
}  // namespace lmpc
