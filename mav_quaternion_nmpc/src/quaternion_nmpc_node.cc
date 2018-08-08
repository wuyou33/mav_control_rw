/*
Copyright (c) 2015, Barza Nisar, Mina Kamel, ASL, ETH Zurich, Switzerland

You can contact the author at <mina.kamel@mavt.ethz.ch> <nisarb@student.ethz.ch> 

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ETHZ-ASL nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETHZ-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ros/ros.h>
#include <mav_msgs/default_topics.h>
#include <mav_quaternion_nmpc/quaternion_nmpc_node.h>
#include <mav_control_interface/mav_control_interface.h>
#include <mav_control_interface/rc_interface_aci.h>
#include "acado_common.h"

namespace mav_control {

	NonLinearModelPredictiveControllerNode::NonLinearModelPredictiveControllerNode(
		const ros::NodeHandle& nh, const ros::NodeHandle& private_nh)
		: nh_(nh),
          private_nh_(private_nh),
		  nonlinear_mpc_(nh, private_nh),
		  controller_dyn_config_server_(private_nh)
	{
		dynamic_reconfigure::Server<mav_quaternion_nmpc::NonLinearMPCConfig>::CallbackType f_controller;
		f_controller = boost::bind(&NonLinearModelPredictiveControllerNode::ControllerDynConfigCallback,
			this, _1, _2);
		controller_dyn_config_server_.setCallback(f_controller);
	}

	NonLinearModelPredictiveControllerNode::~NonLinearModelPredictiveControllerNode()
	{

	}

	bool NonLinearModelPredictiveControllerNode::setReferenceArray(
		const mav_msgs::EigenTrajectoryPointDeque& reference_array)
	{
		nonlinear_mpc_.setCommandTrajectory(reference_array);
		return true;
	}

	bool NonLinearModelPredictiveControllerNode::setReference(
		const mav_msgs::EigenTrajectoryPoint& reference)
	{
		nonlinear_mpc_.setCommandTrajectoryPoint(reference);
		return true;
	}

	void NonLinearModelPredictiveControllerNode::ControllerDynConfigCallback(
		mav_quaternion_nmpc::NonLinearMPCConfig &config, uint32_t level)
	{
		Eigen::Vector3d q_position;
		Eigen::Vector3d q_velocity;
		Eigen::Vector3d q_attitude;
		Eigen::Vector3d q_angVel;


		Eigen::VectorXd control_limits(2);

		q_position << config.q_x, config.q_y, config.q_z;
		q_velocity << config.q_vx, config.q_vy, config.q_vz;
		q_attitude << config.q_q1, config.q_q2, config.q_q3;
		q_angVel << config.q_wx, config.q_wy, config.q_wz;



		control_limits << config.force_min, config.force_max;


		nonlinear_mpc_.setPositionPenality(q_position);
		nonlinear_mpc_.setVelocityPenality(q_velocity);
		nonlinear_mpc_.setAttitudePenality(q_attitude);
		nonlinear_mpc_.setAngVelPenality(q_angVel);
		nonlinear_mpc_.setCommandPenality(config.r_f);
		nonlinear_mpc_.setControlLimits(control_limits);

		nonlinear_mpc_.setAngVelRef(config.angVel_ref);
		nonlinear_mpc_.setAltitudeIntratorGain(config.Ki_altitude);
		nonlinear_mpc_.setXYIntratorGain(config.Ki_xy);

		nonlinear_mpc_.setEnableIntegrator(config.enable_integrator);


		nonlinear_mpc_.applyParameters();

	}



	bool NonLinearModelPredictiveControllerNode::setOdometry(const mav_msgs::EigenOdometry& odometry)
	{
		nonlinear_mpc_.setOdometry(odometry);
		return true;
	}

	bool NonLinearModelPredictiveControllerNode::calculateAttitudeThrustCommand(
		mav_msgs::EigenAttitudeThrust* attitude_thrust_command)
	{
		ROS_WARN("calculateAttitudeThrustCommand not implemented");
		return false;
	}

	bool NonLinearModelPredictiveControllerNode::calculateForcesCommand(mav_msgs::Actuators* forces_command) {

		Eigen::VectorXd normforces(ACADO_NU), forces(ACADO_NU);
		nonlinear_mpc_.calculateForcesCommand(&normforces, &forces);

		double force_const;

				force_const = nonlinear_mpc_.getForceConstant();

				(*forces_command).angular_velocities.clear();

				for (int i = 0; i < forces.size(); i++)
				 {

					if (forces[i] >=0)
					{
				      (*forces_command).angular_velocities.push_back(sqrt(forces[i]/force_const));
					}
				   else {
					   (*forces_command).angular_velocities.push_back(-sqrt(-forces[i]/force_const));
				        }
				 }


		return true;
	}
	

	bool NonLinearModelPredictiveControllerNode::getCurrentReference(
		mav_msgs::EigenTrajectoryPoint* reference) const
	{
		assert(reference != nullptr);
		return nonlinear_mpc_.getCurrentReference(reference);
	}

	bool NonLinearModelPredictiveControllerNode::getCurrentReference(
		mav_msgs::EigenTrajectoryPointDeque* reference) const
	{
		assert(reference != nullptr);
		return nonlinear_mpc_.getCurrentReference(reference);
	}

	bool NonLinearModelPredictiveControllerNode::getPredictedState(
		mav_msgs::EigenTrajectoryPointDeque* predicted_state) const
	{
		assert(predicted_state != nullptr);
	return nonlinear_mpc_.getPredictedState(predicted_state);

	}

};

int main(int argc, char** argv)
{
	ros::init(argc, argv, "NonLinearModelPredictiveControllerNode");

	ros::NodeHandle nh, private_nh("~");

	std::shared_ptr<mav_control::NonLinearModelPredictiveControllerNode> mpc(
		new mav_control::NonLinearModelPredictiveControllerNode(nh, private_nh));

	std::shared_ptr<mav_control_interface::RcInterfaceAci> rc(  //What is this for?
		new mav_control_interface::RcInterfaceAci(nh));

	mav_control_interface::MavControlInterface control_interface(nh, private_nh, mpc, rc);


	ros::spin();

	return 0;
}
