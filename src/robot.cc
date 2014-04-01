#include <robot.h>
#include <utilities.h>

extern bool new_sim_step;

double Robot::calc_energy(Ravelin::VectorNd& v, Ravelin::MatrixNd& M){
  // Potential Energy
  double PE = 0;
  for(int i=0;i<links_.size();i++){
     Moby::RigidBody& link = *links_[i];
     double m = link.get_mass();
     Ravelin::Pose3d link_com = *link.get_inertial_pose();
     link_com.update_relative_pose(Moby::GLOBAL);
     PE += link_com.x[2] * m * grav;
  }
  M.mult(v, workv_);
  double KE = workv_.dot(v)*0.5;
#ifndef NDEBUG
//  std::cout << "KE = " << KE << ", PE = " << PE << std::endl;
//  std::cout << "Total Energy = " << (KE + PE) << std::endl;
#endif
  return (KE + PE);
  // Kinetic Energy
}

void Robot::calculate_dyn_properties(Ravelin::MatrixNd& M, Ravelin::VectorNd& fext){
   M.resize(NDOFS,NDOFS);
   fext.resize(NDOFS);
   if(new_sim_step)
    abrobot_->get_generalized_inertia(M);
   abrobot_->get_generalized_forces(fext);
}

void Robot::compile(){
  dbrobot_ = boost::dynamic_pointer_cast<Moby::DynamicBody>(abrobot_);
  std::vector<Moby::JointPtr> joints = abrobot_->get_joints();
  joints_.resize(joints.size());

  NUM_FIXED_JOINTS = 0;
  for(unsigned i=0;i<joints.size();i++){
    if(joints[i]->q.rows() == 0){
      NUM_FIXED_JOINTS ++;
      continue;
    }
    joints_[joints[i]->get_coord_index()] = joints[i];
  }

  for(unsigned i=0;i<joints_.size()-NUM_FIXED_JOINTS;i++){
    if(joints_[i]->q.rows() == 0){
      continue;
    }
    joint_names_.push_back(joints_[i]->id);
    std::cout << joints_[i]->get_coord_index() << " "
              << joints_[i]->id << std::endl;
  }

  // Set up link references
  links_ = abrobot_->get_links();
}

void EndEffector::init(){
  Moby::JointPtr joint_ptr = link->get_inner_joint_explicit();
  Moby::RigidBodyPtr rb_ptr = link;
  OUT_LOG(logDEBUG) << id ;
  chain_bool.resize(joint_names_.size());
  rb_ptr = joint_ptr->get_inboard_link();
  while (rb_ptr->id.compare("ABDOMEN") != 0 && rb_ptr->id.compare("THORAX") != 0 && rb_ptr->id.compare("BASE") != 0){
    OUT_LOG(logDEBUG) << rb_ptr->id ;
    for(int j=0;j<joint_names_.size();j++){
      if(joint_ptr->id.compare(joint_names_[j]) == 0){
        OUT_LOG(logDEBUG) << "  " << j <<  " "<< joint_ptr->id;
        chain.push_back(j);
        chain_bool[j] = true;
      }
    }
    OUT_LOG(logDEBUG) ;
    rb_ptr = joint_ptr->get_inboard_link();
    joint_ptr = rb_ptr->get_inner_joint_explicit();
  }
  OUT_LOG(logDEBUG) ;
  OUT_LOG(logDEBUG) ;

  Ravelin::Pose3d pose = *link->get_pose();
  normal = Ravelin::Vector3d(0,0,1);
  point = pose.x;
  active = false;
}

void Robot::update(){

  NC = 0;
  for (unsigned i=0; i< NUM_EEFS;i++)
    if(eefs_[i].active)
      NC++;

  // fetch robot state vectors
  dbrobot_->get_generalized_acceleration(acc);
  dbrobot_->get_generalized_velocity(Moby::DynamicBody::eSpatial,vel);
  dbrobot_->get_generalized_coordinates(Moby::DynamicBody::eSpatial,gc);
  calc_contact_jacobians(N,ST,D,R);
  // Cn * M * v = iM * fext
  //      M * v = iM * fext * h
  // Get robot dynamics state
  // SRZ: Very Heavy Computation
  calculate_dyn_properties(M,fext);
//  calc_energy(vel,M);
  calc_com();

  // Get base frame
  base_frame = boost::shared_ptr<Ravelin::Pose3d>( new Ravelin::Pose3d(*links_[0]->get_pose()));
  base_frame->update_relative_pose(Moby::GLOBAL);

  base_horizontal_frame = boost::shared_ptr<Ravelin::Pose3d>( new Ravelin::Pose3d(Moby::GLOBAL));
  base_horizontal_frame->update_relative_pose(Moby::GLOBAL);
  base_horizontal_frame->x = base_frame->x;
  Ravelin::Matrix3d Rot(base_frame->q);
  Utility::R2rpy(Rot,roll_pitch_yaw);
//  OUTLOG(roll_pitch_yaw,"roll_pitch_yaw");
  // remove roll and pitch -- preserve yaw
  Utility::Rz(roll_pitch_yaw[2],Rot);
  base_horizontal_frame->x = base_frame->x;
  base_horizontal_frame->q = base_frame->q;//Quatd(Rot);
  for(int i=0;i<NUM_EEFS;i++)
    eefs_[i].origin.pose = base_horizontal_frame;

  base_frame_global = boost::shared_ptr<Ravelin::Pose3d>( new Ravelin::Pose3d(Moby::GLOBAL));
  base_frame_global->update_relative_pose(Moby::GLOBAL);
  base_frame_global->x = base_frame->x;
  base_frame_global->q.set_identity();


#ifdef VISUALIZE_MOBY
  draw_pose(*base_frame,sim);
  draw_pose(Moby::GLOBAL,sim);
#endif

  calc_eef_jacobians(J);

  if(NC != 0) {
    center_of_contact.point.set_zero();
    center_of_contact.point.pose = Moby::GLOBAL;
    center_of_contact.normal.pose = Moby::GLOBAL;
    for(int f=0;f<NUM_EEFS;f++){
      // set gait centers
      if(eefs_[f].active){
        center_of_contact.point += eefs_[f].point/NC;
        center_of_contact.normal = eefs_[f].normal;
      }
    }
  }

#ifdef VISUALIZE_MOBY
       // CONTACTS
       if(NC != 0){
         std::vector<EndEffector> active_eefs;
         if(eefs_[0].active)
           active_eefs.push_back(eefs_[0]);
         if(eefs_[1].active)
           active_eefs.push_back(eefs_[1]);
         if(eefs_[3].active)
           active_eefs.push_back(eefs_[3]);
         if(eefs_[2].active)
           active_eefs.push_back(eefs_[2]);

         for(int i=0;i<NC;i++){
           visualize_ray(active_eefs[i].point,
                         active_eefs[(i+1)%NC].point,
                         Ravelin::Vector3d(1,1,1),
                         sim);
         }
         for(int i=0;i<NC;i++)
           for(int j=0;j<active_eefs[i].contacts.size();j++)
             visualize_ray(active_eefs[i].contacts[j],
                           active_eefs[i].point,
                           Ravelin::Vector3d(1,1,1),
                           sim);
       }

       visualize_ray(center_of_contact.point,
                  center_of_contact.normal + center_of_contact.point,
                  Ravelin::Vector3d(1,1,0),
                  sim);
#endif
}
