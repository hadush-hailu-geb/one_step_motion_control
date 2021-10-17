#include <ros/ros.h>
#include <std_msgs/Float32.h>
#include <actionlib/server/simple_action_server.h>
#include <one_step_motor_control/OsmcAction.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_datatypes.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <geometry_msgs/Twist.h>
#include <math.h>

#define PI 3.14159265

class OsmcServer
{
private:
	ros::NodeHandle nh_;
	actionlib::SimpleActionServer<one_step_motor_control::OsmcAction> as_;
	std::string action_name_;
	one_step_motor_control::OsmcFeedback feedback_;
	one_step_motor_control::OsmcResult result_;
    
	nav_msgs::Odometry odom_;
        nav_msgs::Path path_;	
	ros::Subscriber sub_;
	ros::Publisher pub_;
	ros::Publisher pubPath_;


public:
    
  OsmcServer(std::string name) : 
    as_(nh_, name, boost::bind(&OsmcServer::executeCB, this, _1),false),
    action_name_(name)
  {
    as_.start();
    sub_ = nh_.subscribe("/odom", 100, &OsmcServer::odomTopicCB, this);
    pub_ = nh_.advertise<geometry_msgs::Twist>("/cmd_vel", 1000);
    pubPath_ = nh_.advertise<nav_msgs::Path>("/osmc/globalPath",100);
  }

  ~OsmcServer(void)
  {
  }

 void odomTopicCB(const nav_msgs::Odometry  msg){
	 odom_ = msg;
 }

 geometry_msgs::Twist zeroTwist(geometry_msgs::Twist twist){
 	twist.linear.x = 0;
        twist.linear.y = 0;
	twist.linear.z = 0;
	twist.angular.x = 0;
	twist.angular.y = 0;							     
	twist.angular.z = 0;
	return twist;

 }

 void executeCB(const one_step_motor_control::OsmcGoalConstPtr &msg){

	 //helper variables
	 ros::Rate r(5);
	 bool sucess = true;
	 double ang_z,lin_x;
	 geometry_msgs::Twist current_cmd;
	 
	 //get goal
	 geometry_msgs::PoseStamped goal;
	 goal = msg->target_pose;

	 //conver odom to poseStamped
	 geometry_msgs::PoseStamped odomPoseStamped;


	 double target_x = goal.pose.position.x;
         double target_y = goal.pose.position.y;
         double target_yaw = tf::getYaw(goal.pose.orientation);

         ROS_INFO("TARGET : pose (%f,%f) yaw %f \n",target_x,target_y,target_yaw);


	 while(1){
	 
	 //zero twist
	 current_cmd = zeroTwist(current_cmd);

	 //get robot's current pos and yaw
	 double current_x = odom_.pose.pose.position.x;
	 double current_y = odom_.pose.pose.position.y;
	 double current_yaw = tf::getYaw(odom_.pose.pose.orientation);

	 odomPoseStamped.header = odom_.header;
	 odomPoseStamped.pose = odom_.pose.pose;
	 //push_back to path
	 path_.header = odom_.header;
	 path_.poses.push_back(odomPoseStamped);
	 path_.poses.push_back(goal);
	 pubPath_.publish(path_);

	 ROS_INFO("CURRENT : pose (%f,%f) yaw %f \n",current_x,current_y,current_yaw);

	 double path_yaw = atan((target_y - current_y)/(target_x - current_x));

	 double diff_yaw = path_yaw - current_yaw;
	 ROS_INFO("diff_yaw: %f current_yaw: %f target_yaw:%f \n",diff_yaw,current_yaw,target_yaw);
	 //control angular
	 if(fabs(diff_yaw) < 0.05){ // three degree
       		 ang_z = 0;
	 }else if(diff_yaw > 0){
		 ang_z = 0.5; 
	 }else{
		 ang_z = -0.5;
	 }

	 //control linear
	 double diff_lin_x = target_x - current_x;
	 if(fabs(diff_lin_x) < 0.02){ //two centmeter
	 	lin_x = 0;
	 }else if(diff_lin_x > 0){
	 	lin_x = 0.1;
	 }else{
	 	lin_x = -0.1;
	 }

	 current_cmd.linear.x = lin_x;
	 current_cmd.angular.z = ang_z;
	 pub_.publish(current_cmd);

	 //exit
	 if(lin_x == 0 and ang_z == 0)
		 break;

	 r.sleep();
	 }
	 
	  
	 as_.setSucceeded();
 }
  
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "osmc_serve");

  OsmcServer server_node ("osmc");
  ros::spin();

  return 0;
}

