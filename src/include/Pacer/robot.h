/****************************************************************************
 * Copyright 2014 Samuel Zapolsky
 * This library is distributed under the terms of the Apache V2.0
 * License (obtainable from http://www.apache.org/licenses/LICENSE-2.0).
 ****************************************************************************/
#ifndef ROBOT_H
#define ROBOT_H

#include <Pacer/Log.h>

#include <Ravelin/RCArticulatedBodyd.h>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/icl/type_traits/to_string.hpp>

#include <Pacer/output.h>

#include <numeric>
#include <math.h>
#include <cmath>
#include <sys/types.h>
#include <sys/times.h>
#include <boost/any.hpp>
#ifdef USE_THREADS
#include <pthread.h>
#endif

namespace Pacer{
  
  const boost::shared_ptr<const Ravelin::Pose3d> GLOBAL;
  const double NEAR_ZERO = std::sqrt(std::numeric_limits<double>::epsilon());
  const Ravelin::Matrix3d ZEROS_3x3 = Ravelin::Matrix3d::zero();
  const Ravelin::Matrix3d IDENTITY_3x3 = Ravelin::Matrix3d::identity();
  const Ravelin::VectorNd EMPTY_VEC(0);
  
  class Robot;
  
  static const unsigned NSPATIAL = 6;
  static const unsigned NEULER = 7;
  
  class Robot{
  public:
    
    Robot(){
      controller_phase = INITIALIZATION;
    }
    
  protected:
    /// --------------------  Data Storage  -------------------- ///
  private:
    template<typename T>
    struct is_pointer { static const bool value = false; };
    
    template<typename T>
    struct is_pointer<T*> { static const bool value = true; };
    
    // Map for storing arbitrary data
    std::map< std::string , boost::any > _data_map;
#ifdef USE_THREADS
    pthread_mutex_t _data_map_mutex;
#endif
  public:
    
    // Returns 'true' if new key was created in map
    template<class T>
    bool set_data(std::string n, const T& v){
#ifdef LOG_TO_FILE
      OUT_LOG(logINFO) << "Set: " << n << " <-- " << v;
#endif
      if(is_pointer<T>::value){
        throw std::runtime_error("Can't save pointers! : " + n);
      }
      return set_data_internal(n,v);
    }
    
    // Returns 'true' if new key was created in map
    bool set_data_internal(std::string n, boost::any to_append);
    
    void remove_data(std::string n){
#ifdef USE_THREADS
      pthread_mutex_lock(&_data_map_mutex);
#endif
#ifdef LOG_TO_FILE
      OUT_LOG(logINFO) << "Remove: " << n;
#endif
      std::map<std::string,boost::any >::iterator it
      =_data_map.find(n);
      if (it != _data_map.end()){
        _data_map.erase(it);
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_data_map_mutex);
#endif
    }
    
    template<class T>
    T get_data(std::string n){
      using boost::any_cast;
      
      std::map<std::string,boost::any >::iterator it;
#ifdef USE_THREADS
      pthread_mutex_lock(&_data_map_mutex);
#endif
      it = _data_map.find(n);
      if(it != _data_map.end()){
        T v;
        //        T* v = (T*) (((*it).second).get());
        boost::any& operand = (*it).second;
        try
        {
          v = any_cast<T>(operand);
        }
        catch(const boost::bad_any_cast &)
        {
          throw std::runtime_error("Variable: \"" + n + "\" was requested as '" + typeid(T).name() + "' but is actually '" + operand.type().name() + "'");
          ;
        }
#ifdef USE_THREADS
        pthread_mutex_unlock(&_data_map_mutex);
#endif
#ifdef LOG_TO_FILE
        OUT_LOG(logINFO) << "Get: " << n << " ("<< operand.type().name() <<") --> " << v;
#endif
        return v;
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_data_map_mutex);
#endif
      
      // else
      throw std::runtime_error("Variable: \"" + n + "\" not found in data!");
    }
    
    /// @brief Get data we're not sure exists.
    /// Return false and do nothing to data if it doesnt exist
    template<class T>
    bool get_data(std::string n,T& val){
      try {
        val = get_data<T>(n);
        return true;
      } catch(std::runtime_error& e) {
        OUT_LOG(logDEBUG) << e.what() << std::endl;
        return false;
      }
    }
    
    
    /// ---------------------------  Getters  ---------------------------
  public:
    struct contact_t{
      std::string id;
      Ravelin::Vector3d point;
      Ravelin::Vector3d normal, tangent;
      Ravelin::Vector3d impulse;
      double mu_coulomb;
      double mu_viscous;
      double restitution;
      bool compliant;
    };
    
    
  private:
    /**
     * @brief The end_effector_t struct
     *
     * Stores relevent contact and link data for an end effector.
     */
    struct end_effector_t{
      // permanent data
      std::string            id;
      // Foot link pointer
      boost::shared_ptr<Ravelin::RigidBodyd>     link;
      std::vector<boost::shared_ptr<Ravelin::Jointd> >        chain_joints;
      std::vector<boost::shared_ptr<Ravelin::RigidBodyd> >    chain_links;
      // kinematic chain indexing generalized coordinates
      std::vector<unsigned> chain;
      std::vector<bool>     chain_bool;
      
      // For locomotion, this informs us if this link should be used to calculate a jocobian
      bool                   active;
      bool                   stance;
    };
    
    std::map<std::string,std::vector< boost::shared_ptr<const contact_t> > > _id_contacts_map;
    std::map<std::string,boost::shared_ptr<end_effector_t> > _id_end_effector_map;
    
  public:
    bool is_end_effector(const std::string& id){
      std::map<std::string,boost::shared_ptr<end_effector_t> >::iterator
      it = _id_end_effector_map.find(id);
      if(it != _id_end_effector_map.end())
        return true;
      return false;
    }
    
    std::vector<std::string> get_end_effector_names(){
      std::vector<std::string> eefs;
      std::map<std::string,boost::shared_ptr<end_effector_t> >::iterator
      it = _id_end_effector_map.begin();
      for(;it != _id_end_effector_map.end();it++)
        eefs.push_back((*it).first);
      return eefs;
    }
    
    Ravelin::MatrixNd calc_link_jacobian(const Ravelin::VectorNd& q, const std::string& link){
      return calc_jacobian(q,link,Ravelin::Origin3d(0,0,0));
    }
    
    const std::map<std::string,boost::shared_ptr<Ravelin::RigidBodyd> >& get_links(){
      return _id_link_map;
    }
    
    const boost::shared_ptr<Ravelin::RigidBodyd> get_link(const std::string& link){
      return _id_link_map[link];
    }
    
    
    void add_contact(
                     std::string& id,
                     Ravelin::Vector3d point,
                     Ravelin::Vector3d normal,
                     Ravelin::Vector3d tangent,
                     Ravelin::Vector3d impulse = Ravelin::Vector3d(),
                     double mu_coulomb = 0,double mu_viscous = 0,double restitution = 0, bool compliant = false)
    {
      check_phase(misc_sensor);
      _id_contacts_map[id].push_back(create_contact(id,point,normal, tangent, impulse,mu_coulomb,mu_viscous,restitution,compliant));
    }
    
    boost::shared_ptr<contact_t> create_contact(
                                                std::string& id,
                                                Ravelin::Vector3d point,
                                                Ravelin::Vector3d normal,
                                                Ravelin::Vector3d tangent,
                                                Ravelin::Vector3d impulse = Ravelin::Vector3d(),
                                                double mu_coulomb = 0,double mu_viscous = 0,double restitution = 0, bool compliant = false)
    {
      boost::shared_ptr<contact_t> c(new contact_t);
      c->id         = id;
      c->point      = point;
      c->normal     = normal;
      c->tangent    = tangent;
      c->impulse    = impulse;
      c->mu_coulomb = mu_coulomb;
      c->mu_viscous = mu_viscous;
      c->restitution= restitution;
      c->compliant  = compliant;
      return c;
    }
    
    void add_contact(boost::shared_ptr<const contact_t>& c)
    {
      check_phase(misc_sensor);
      _id_contacts_map[c->id].push_back(c);
    }
    
    void get_contacts(std::map<std::string,std::vector< boost::shared_ptr<const contact_t> > >& id_contacts_map){
      id_contacts_map = _id_contacts_map;
    }
    
    int get_all_contacts(std::vector< boost::shared_ptr<const contact_t> >& contacts){
      for(int i=0;i<_link_ids.size();i++){
        std::vector< boost::shared_ptr<const contact_t> >& c = _id_contacts_map[_link_ids[i]];
        contacts.insert(contacts.end(), c.begin(), c.end());
      }
      return contacts.size();
    }
    
    void get_link_contacts(const std::string& link_id, std::vector< boost::shared_ptr<const contact_t> >& contacts){
      contacts = _id_contacts_map[link_id];
    }
    
    void get_link_contacts(const std::vector<std::string> link_id,std::vector< boost::shared_ptr<const contact_t> >& contacts){
      for(int i=0;i<link_id.size();i++){
        std::vector< boost::shared_ptr<const contact_t> >& c = _id_contacts_map[link_id[i]];
        contacts.insert(contacts.end(), c.begin(), c.end());
      }
    }
    
    void reset_contact(){
      check_phase(clean_up);
      _id_contacts_map.clear();
    }
    
  public:
    enum unit_e  {       //  ang  |  lin
                         // SET OUTSIDE CONTROL LOOP
      misc_sensor=0,           //  SENSORS
      position=1,           //  rad  |   m
      velocity=2,           // rad/s |  m/s
      acceleration=3,       // rad/ss|  m/ss
      load=4,               //  N.m  |   N
                            // SET IN CONROLLER
      misc_planner=5,
      position_goal=6,      //  rad  |   m
      velocity_goal=7,      // rad/s |  m/s
      acceleration_goal=8,  // rad/ss|  m/ss
      misc_controller=9,
      load_goal=10,// TORQUE//  N.m  |   N
      initialization=11,
      clean_up=12};
  private:
    const char * enum_string(const unit_e& e){
      int i = static_cast<int>(e);
      return (const char *[]) {
        "misc_sensor",
        "position",
        "velocity",
        "acceleration",
        "load",
        "misc_planner",
        "position_goal",
        "velocity_goal",
        "acceleration_goal",
        "misc_controller",
        "load_goal",
        "initialization",
        "clean_up"
      }[i];
    }
    
    //    std::deque<std::map<unit_e , std::map<std::string, Ravelin::VectorNd > > > _state_history;
    //    std::deque<std::map<unit_e , Ravelin::VectorNd> > _base_state_history;
    
    std::map<unit_e , std::map<std::string, Ravelin::VectorNd > > _state;
    std::map<unit_e , Ravelin::VectorNd> _base_state;
    std::map<unit_e , std::map<std::string, Ravelin::Origin3d > > _foot_state;
    std::map<std::string, bool > _foot_is_set;
    
#ifdef USE_THREADS
    pthread_mutex_t _state_mutex;
    pthread_mutex_t _base_state_mutex;
    pthread_mutex_t _foot_state_mutex;
#endif
    // TODO: Populate these value in Robot::compile()
    
    // JOINT_NAME --> {gcoord_dof1,...}
    std::map<std::string,std::vector<int> > _id_dof_coord_map;
    
    // gcoord --> (JOINT_NAME,dof)
    std::map<int,std::pair<std::string, int> > _coord_id_map;
  protected:
    enum ControllerPhase{
      INITIALIZATION = 0,
      PERCEPTION = 1,
      PLANNING = 2,
      CONTROL = 3,
      WAITING = 4,
      INCREMENT=5
    };
  private:
    ControllerPhase controller_phase;
    const char * enum_string(const ControllerPhase& e){
      int i = static_cast<int>(e);
      return (const char *[]) {
        "INITIALIZATION",
        "PERCEPTION",
        "PLANNING",
        "CONTROL",
        "WAITING",
        "INCREMENT"
      }[i];
    }
  protected:
    
    // Enforce that values are only being assigned during the correct phase
    void check_phase(const unit_e& u){
      OUT_LOG(logDEBUG) << "-- SCHEDULER -- " << "check unit: " << enum_string(u) << " against phase: " << enum_string(controller_phase);
      switch (u) {
        case initialization:
          if (controller_phase != INITIALIZATION && controller_phase != WAITING){
            throw std::runtime_error("controller must be in INITIALIZATION phase to set Pacer internal parameters.");
          }
          break;
        case misc_sensor:
        case position:
        case velocity:
        case acceleration:
        case load:
          if (controller_phase != PERCEPTION && controller_phase != INITIALIZATION  && controller_phase != WAITING ){
            throw std::runtime_error("controller must be in PERCEPTION, INITIALIZATION or WAITING phase to set state information: {misc_sensor,position,velocity,acceleration,load}");
          }
          break;
        case misc_planner:
        case position_goal:
        case velocity_goal:
        case acceleration_goal:
          if (controller_phase != PLANNING && controller_phase != INITIALIZATION && controller_phase != WAITING){
            // NOTE: if this is the first PLANNING call and we were in PERCEPTION phase, increment to PLANNING phase
            if(controller_phase == PERCEPTION){
              increment_phase(PLANNING);
              break;
            }
            throw std::runtime_error("controller must be in PLANNING phase to set goal state information: {misc_planner,position_goal,velocity_goal,acceleration_goal}");
          }
          break;
        case misc_controller:
        case load_goal:
          if (controller_phase != CONTROL && controller_phase != INITIALIZATION && controller_phase != WAITING){
            // NOTE: if this is the first CONTROL call and we were in PLANNING phase, increment to CONTROL phase
            if(controller_phase == PLANNING){
              increment_phase(CONTROL);
              break;
            }
            
            if(controller_phase == PERCEPTION){
              increment_phase(CONTROL);
              break;
            }
            throw std::runtime_error("controller must be in CONTROL phase to set commands: {misc_controller,load_goal}");
          }
          break;
        case clean_up:
          if (controller_phase != WAITING && controller_phase != INITIALIZATION )
            throw std::runtime_error("controller must be in WAITING or INITIALIZATION phase to perform clean_up duties.");
          break;
        default:
          throw std::runtime_error("unknown unit being set in state data");
          break;
      }
    }
    
    void reset_phase(){
      controller_phase = PERCEPTION;
      OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase reset: ==> PLANNING";
    }
    
    void increment_phase(ControllerPhase phase){
      if (phase == INCREMENT) {
        switch (controller_phase) {
          case INITIALIZATION:
            controller_phase = PERCEPTION;
            OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase change: *INITIALIZATION* ==> PERCEPTION";
            break;
          case WAITING:
            throw std::runtime_error("Cannot increment waiting controller. call reset_phase()");
            break;
          case PERCEPTION:
            controller_phase = PLANNING;
            OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase change: PERCEPTION ==> PLANNING";
            break;
          case PLANNING:
            controller_phase = CONTROL;
            OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase change: PLANNING ==> CONTROL";
            break;
          case CONTROL:
            controller_phase = WAITING;
            OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase change: CONTROL ==> *WAITING*";
            break;
          default:
            throw std::runtime_error("controller state dropped off list of valid states");
            break;
        }
      } else {
        OUT_LOG(logINFO) << "-- SCHEDULER -- " << "Controller Phase change: " << enum_string(controller_phase) << " ==> " << enum_string(phase);
        controller_phase =phase;
      }
    }
  public:
    int get_joint_dofs(const std::string& id)
    {
      return _id_dof_coord_map[id].size();
    }
    
    //////////////////////////////////////////////////////
    /// ------------ GET/SET JOINT value  ------------ ///
    
    double get_joint_value(const std::string& id, unit_e u, int dof)
    {
      OUT_LOG(logDEBUG) << "Get: "<< id << "_" << enum_string(u) << "["<< dof << "] --> " << _state[u][id][dof];
      return _state[u][id][dof];
    }
    
    Ravelin::VectorNd get_joint_value(const std::string& id, unit_e u)
    {
      OUT_LOG(logDEBUG) << "Get: "<< id << "_" << enum_string(u) << " --> " << _state[u][id];
      return _state[u][id];
    }
    
    void get_joint_value(const std::string& id, unit_e u, Ravelin::VectorNd& dof_val)
    {
      dof_val = _state[u][id];
#ifdef LOG_TO_FILE
      OUT_LOG(logDEBUG) << "Get: "<< id << "_" << enum_string(u) << " --> " << dof_val;
#endif
    }
    
    void get_joint_value(const std::string& id, unit_e u,std::vector<double>& dof_val)
    {
      Ravelin::VectorNd& dof = _state[u][id];
      dof_val.resize(dof.rows());
      for(int i=0;i<dof.rows();i++)
        dof_val[i] = dof[i];
#ifdef LOG_TO_FILE
      OUT_LOG(logDEBUG) << "Get: "<< id << "_" << enum_string(u) << " --> " << dof_val;
#endif
    }
    
    void set_joint_value(const std::string& id, unit_e u, int dof, double val)
    {
      OUT_LOG(logDEBUG) << "Set: "<< id << "_" << enum_string(u) << "["<< dof <<"] <-- " << val;
      
      check_phase(u);
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      _state[u][id][dof] = val;
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    void set_joint_value(const std::string& id, unit_e u, const Ravelin::VectorNd& dof_val)
    {
#ifdef LOG_TO_FILE
      OUT_LOG(logDEBUG) << "Set: "<< id << "_" << enum_string(u) << " <-- " << dof_val;
#endif
      check_phase(u);
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      Ravelin::VectorNd& dof = _state[u][id];
      if(dof.rows() != dof_val.rows())
        throw std::runtime_error("Missized dofs in joint "+id+": internal="+boost::icl::to_string<double>::apply(dof.rows())+" , provided="+boost::icl::to_string<double>::apply(dof_val.rows()));
      dof = dof_val;
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    void set_joint_value(const std::string& id, unit_e u, const std::vector<double>& dof_val)
    {
#ifdef LOG_TO_FILE
      OUT_LOG(logDEBUG) << "Set: "<< id << "_" << enum_string(u) << " <-- " << dof_val;
#endif
      
      check_phase(u);
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      Ravelin::VectorNd& dof = _state[u][id];
      if(dof.rows() != dof_val.size())
        throw std::runtime_error("Missized dofs in joint "+id+": internal="+boost::icl::to_string<double>::apply(dof.rows())+" , provided="+boost::icl::to_string<double>::apply(dof_val.size()));
      for(int i=0;i<dof.rows();i++)
        dof[i] = dof_val[i];
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    void get_joint_value(unit_e u, std::map<std::string,std::vector<double> >& id_dof_val_map){
      std::map<std::string,Ravelin::VectorNd>::iterator it;
      for(it=_state[u].begin();it!=_state[u].end();it++){
        const Ravelin::VectorNd& dof_val_internal = (*it).second;
        std::vector<double>& dof_val = id_dof_val_map[(*it).first];
        dof_val.resize(dof_val_internal.size());
        for(int j=0;j<(*it).second.rows();j++){
          dof_val[j] = dof_val_internal[j];
        }
#ifdef LOG_TO_FILE
        OUT_LOG(logDEBUG) << "Get: "<< (*it).first << "_" << enum_string(u) << " --> " << dof_val;
#endif
      }
    }
    
    void get_joint_value(unit_e u, std::map<std::string,Ravelin::VectorNd >& id_dof_val_map){
      const std::vector<std::string>& keys = _joint_ids;
      for(int i=0;i<keys.size();i++){
        const Ravelin::VectorNd& dof_val_internal = _state[u][keys[i]];
        id_dof_val_map[keys[i]] = dof_val_internal;
#ifdef LOG_TO_FILE
        OUT_LOG(logDEBUG) << "Get: "<< keys[i] << "_" << enum_string(u) << " --> " << dof_val_internal;
#endif
      }
    }
    
    void set_joint_value(unit_e u,const std::map<std::string,std::vector<double> >& id_dof_val_map){
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      std::map<std::string,std::vector<double> >::const_iterator it;
      
      for(it=id_dof_val_map.begin();it!=id_dof_val_map.end();it++){
        Ravelin::VectorNd& dof_val_internal = _state[u][(*it).first];
        const std::vector<double>& dof_val = (*it).second;
#ifdef LOG_TO_FILE
        OUT_LOG(logDEBUG) << "Set: "<< (*it).first << "_" << enum_string(u) << " <-- " << dof_val;
#endif
        if(dof_val_internal.rows() != dof_val.size()){
          std::cerr << "Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof_val_internal.rows())+" , provided="+boost::icl::to_string<double>::apply(dof_val.size()) << std::endl;
          throw std::runtime_error("Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof_val_internal.rows())+" , provided="+boost::icl::to_string<double>::apply(dof_val.size()));
        }
        for(int j=0;j<(*it).second.size();j++){
          dof_val_internal[j] = dof_val[j];
        }
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    void set_joint_value(unit_e u,const std::map<std::string,Ravelin::VectorNd >& id_dof_val_map){
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      std::map<std::string,Ravelin::VectorNd >::const_iterator it;
      for(it=id_dof_val_map.begin();it!=id_dof_val_map.end();it++){
        Ravelin::VectorNd& dof_val_internal = _state[u][(*it).first];
        OUT_LOG(logDEBUG) << "Set: "<< (*it).first << "_" << enum_string(u) << " <-- " << (*it).second;
        if(dof_val_internal.rows() != (*it).second.rows())
          throw std::runtime_error("Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof_val_internal.rows())+" , provided="+boost::icl::to_string<double>::apply((*it).second.rows()));
        dof_val_internal = (*it).second;
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    /// ------------- GENERALIZED VECTOR CONVERSIONS  ------------- ///
    
    
    void convert_to_generalized(const std::map<std::string,std::vector<double> >& id_dof_val_map, Ravelin::VectorNd& generalized_vec){
      generalized_vec.set_zero(NUM_JOINT_DOFS);
      std::map<std::string,std::vector<double> >::const_iterator it;
      for(it=id_dof_val_map.begin();it!=id_dof_val_map.end();it++){
        const std::vector<int>& dof = _id_dof_coord_map[(*it).first];
        const std::vector<double>& dof_val = (*it).second;
        if(dof.size() != dof_val.size())
          throw std::runtime_error("Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof.size())+" , provided="+boost::icl::to_string<double>::apply(dof_val.size()));
        for(int j=0;j<(*it).second.size();j++){
          generalized_vec[dof[j]] = dof_val[j];
        }
      }
    }
    
    template <typename T>
    void convert_to_generalized(const std::map<std::string,std::vector<T> >& id_dof_val_map, std::vector<T>& generalized_vec){
      typename std::map<std::string,std::vector<T> >::const_iterator it;
      generalized_vec.resize(NUM_JOINT_DOFS);
      it = id_dof_val_map.begin();
      for(it=id_dof_val_map.begin();it!=id_dof_val_map.end();it++){
        const std::vector<int>& dof = _id_dof_coord_map[(*it).first];
        const std::vector<T>& dof_val = (*it).second;
        if(dof.size() != dof_val.size())
          throw std::runtime_error("Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof.size())+" , provided="+boost::icl::to_string<double>::apply(dof_val.size()));
        for(int j=0;j<(*it).second.size();j++){
          generalized_vec[dof[j]] = dof_val[j];
        }
      }
    }
    
    
    void convert_to_generalized(const std::map<std::string,Ravelin::VectorNd >& id_dof_val_map, Ravelin::VectorNd& generalized_vec){
      generalized_vec.set_zero(NUM_JOINT_DOFS);
      std::map<std::string,Ravelin::VectorNd >::const_iterator it;
      for(it=id_dof_val_map.begin();it!=id_dof_val_map.end();it++){
        const std::vector<int>& dof = _id_dof_coord_map[(*it).first];
        const Ravelin::VectorNd& dof_val = (*it).second;
        if(dof.size() != dof_val.rows())
          throw std::runtime_error("Missized dofs in joint "+(*it).first+": internal="+boost::icl::to_string<double>::apply(dof.size())+" , provided="+boost::icl::to_string<double>::apply(dof_val.rows()));
        for(int j=0;j<(*it).second.rows();j++){
          generalized_vec[dof[j]] = dof_val[j];
        }
      }
    }
    
    template <typename K, typename V>
    std::vector<K> get_map_keys(const std::map<K,V>& m){
      std::vector<K> v;
      typedef const std::pair<K,V> map_pair;
      BOOST_FOREACH(map_pair me, m) {
        v.push_back(me.first);
      }
      return v;
    }
    
    template <typename T>
    std::map<std::string,std::vector<T> > make_id_value_map(){
      std::map<std::string,std::vector<T> > id_dof_val_map;
      std::map<std::string,std::vector<int> >::iterator it;
      for(it=_id_dof_coord_map.begin();it!=_id_dof_coord_map.end();it++){
        const std::vector<int>& dof = (*it).second;
        std::vector<T>& dof_val = id_dof_val_map[(*it).first];
        dof_val.resize(dof.size());
      }
      return id_dof_val_map;
    }
    
    template <typename T>
    void convert_from_generalized(const std::vector<T>& generalized_vec, std::map<std::string,std::vector<T> >& id_dof_val_map){
      if(generalized_vec.size() != NUM_JOINT_DOFS)
        throw std::runtime_error("Missized generalized vector: internal="+boost::icl::to_string<double>::apply(NUM_JOINT_DOFS)+" , provided="+boost::icl::to_string<double>::apply(generalized_vec.size()));
      
      //      std::map<std::string,std::vector<int> >::iterator it;
      std::vector<std::string> keys = get_map_keys(_id_dof_coord_map);
      
      for(int i=0;i<keys.size();i++){
        const std::vector<int>& dof = _id_dof_coord_map[keys[i]];
        std::vector<T>& dof_val = id_dof_val_map[keys[i]];
        dof_val.resize(dof.size());
        for(int j=0;j<dof.size();j++){
          dof_val[j] = generalized_vec[dof[j]];
        }
      }
    }
    
    void convert_from_generalized(const Ravelin::VectorNd& generalized_vec, std::map<std::string,std::vector<double> >& id_dof_val_map){
      if(generalized_vec.rows() != NUM_JOINT_DOFS)
        throw std::runtime_error("Missized generalized vector: internal="+boost::icl::to_string<double>::apply(NUM_JOINT_DOFS)+" , provided="+boost::icl::to_string<double>::apply(generalized_vec.rows()));
      
      //      std::map<std::string,std::vector<int> >::iterator it;
      std::vector<std::string> keys = get_map_keys(_id_dof_coord_map);
      
      for(int i=0;i<keys.size();i++){
        const std::vector<int>& dof = _id_dof_coord_map[keys[i]];
        std::vector<double>& dof_val = id_dof_val_map[keys[i]];
        dof_val.resize(dof.size());
        for(int j=0;j<dof.size();j++){
          dof_val[j] = generalized_vec[dof[j]];
        }
      }
    }
    
    void convert_from_generalized(const Ravelin::VectorNd& generalized_vec, std::map<std::string,Ravelin::VectorNd >& id_dof_val_map){
      if(generalized_vec.rows() != NUM_JOINT_DOFS)
        throw std::runtime_error("Missized generalized vector: internal="+boost::icl::to_string<double>::apply(NUM_JOINT_DOFS)+" , provided="+boost::icl::to_string<double>::apply(generalized_vec.rows()));
      
      //      std::map<std::string,std::vector<int> >::iterator it;
      std::vector<std::string> keys = get_map_keys(_id_dof_coord_map);
      
      for(int i=0;i<keys.size();i++){
        const std::vector<int>& dof = _id_dof_coord_map[keys[i]];
        Ravelin::VectorNd& dof_val = id_dof_val_map[keys[i]];
        dof_val.resize(dof.size());
        for(int j=0;j<dof.size();j++){
          dof_val[j] = generalized_vec[dof[j]];
        }
      }
    }
    
    /// ------------ GET/SET JOINT GENERALIZED value  ------------ ///
    
    void set_joint_generalized_value(unit_e u, const Ravelin::VectorNd& generalized_vec)
    {
      check_phase(u);
      if(generalized_vec.rows() != NUM_JOINT_DOFS)
        throw std::runtime_error("Missized generalized vector: internal="+boost::icl::to_string<double>::apply(NUM_JOINT_DOFS)+" , provided="+boost::icl::to_string<double>::apply(generalized_vec.rows()));
      
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      
      // TODO: make this more efficient ITERATORS dont work
      //      std::map<std::string,Ravelin::VectorNd>::iterator it;
      const std::vector<std::string>& keys = _joint_ids;
      
      for(int i=0;i<keys.size();i++){
        const std::vector<int>& dof = _id_dof_coord_map[keys[i]];
        Ravelin::VectorNd& dof_val = _state[u][keys[i]];
        if(dof.size() != dof_val.rows())
          throw std::runtime_error("Missized dofs in joint "+keys[i]+": internal="+boost::icl::to_string<double>::apply(dof.size())+" , provided="+boost::icl::to_string<double>::apply(dof_val.rows()));
        for(int j=0;j<dof.size();j++){
          dof_val[j] = generalized_vec[dof[j]];
        }
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
      OUT_LOG(logDEBUG) << "Set: joint_generalized_" << enum_string(u) << " <-- " << generalized_vec;
    }
    
    void get_joint_generalized_value(unit_e u, Ravelin::VectorNd& generalized_vec){
      generalized_vec.set_zero(NUM_JOINT_DOFS);
      std::map<std::string,Ravelin::VectorNd>::iterator it;
      for(it=_state[u].begin();it!=_state[u].end();it++){
        const std::vector<int>& dof = _id_dof_coord_map[(*it).first];
        const Ravelin::VectorNd& dof_val = (*it).second;
        for(int j=0;j<(*it).second.size();j++){
          generalized_vec[dof[j]] = dof_val[j];
        }
      }
      OUT_LOG(logDEBUG) << "Get: joint_generalized_" << enum_string(u) << " --> " << generalized_vec;
    }
    
    Ravelin::VectorNd get_joint_generalized_value(unit_e u){
      Ravelin::VectorNd generalized_vec;
      get_joint_generalized_value(u,generalized_vec);
      return generalized_vec;
    }
    
    /// With Base
    void set_generalized_value(unit_e u,const Ravelin::VectorNd& generalized_vec){
      check_phase(u);
      switch(u){
        case(position_goal):
        case(position):
          set_base_value(u,generalized_vec.segment(NUM_JOINT_DOFS,NUM_JOINT_DOFS+NEULER));
          break;
        default:
          set_base_value(u,generalized_vec.segment(NUM_JOINT_DOFS,NUM_JOINT_DOFS+NSPATIAL));
          break;
      }
      set_joint_generalized_value(u,generalized_vec.segment(0,NUM_JOINT_DOFS));
      OUT_LOG(logDEBUG) << "Set: generalized_" << enum_string(u) << " <-- " << generalized_vec;
    }
    
    /// With Base
    void get_generalized_value(unit_e u, Ravelin::VectorNd& generalized_vec){
      switch(u){
        case(position_goal):
        case(position):
          generalized_vec.set_zero(NUM_JOINT_DOFS+NEULER);
          break;
        default:
          generalized_vec.set_zero(NUM_JOINT_DOFS+NSPATIAL);
          break;
      }
      generalized_vec.set_sub_vec(0,get_joint_generalized_value(u));
      generalized_vec.set_sub_vec(NUM_JOINT_DOFS,get_base_value(u));
      OUT_LOG(logDEBUG) << "Get: generalized_" << enum_string(u) << " --> " << generalized_vec;
    }
    
    /// With Base
    Ravelin::VectorNd get_generalized_value(unit_e u){
      Ravelin::VectorNd generalized_vec;
      get_generalized_value(u,generalized_vec);
      return generalized_vec;
    }
    
    void set_base_value(unit_e u,const Ravelin::VectorNd& vec){
      OUT_LOG(logDEBUG) << "Set: base_" << enum_string(u) << " <-- " << vec;
      check_phase(u);
      switch(u){
        case(position_goal):
        case(position):
          if(vec.rows() != NEULER)
            throw std::runtime_error("position vector should have 7 rows [lin(x y z), quat(x y z w)]");
          break;
        default:
          if(vec.rows() != NSPATIAL)
            throw std::runtime_error("spatial vector should have 6 rows [lin(x y z), ang(x y z)]");
          break;
      }
      
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      _base_state[u] = vec;
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
      
    }
    
    void get_base_value(unit_e u, Ravelin::VectorNd& vec){
      vec = _base_state[u];
      OUT_LOG(logDEBUG) << "Get: base_" << enum_string(u) << " --> " << vec;
    }
    
    Ravelin::VectorNd get_base_value(unit_e u){
      Ravelin::VectorNd vec;
      get_base_value(u,vec);
      return vec;
    }
    
    void set_foot_value(const std::string& id, unit_e u, const Ravelin::Origin3d& val)
    {
      OUT_LOG(logDEBUG) << "Set: foot "<< id <<"_" << enum_string(u) << " <-- " << val;
      check_phase(u);
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      _foot_state[u][id] = val;
      _foot_is_set[id] = true;
      
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    Ravelin::Origin3d& get_foot_value(const std::string& id, unit_e u, Ravelin::Origin3d& val)
    {
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      val = _foot_state[u][id];
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
      OUT_LOG(logDEBUG) << "Get: foot "<< id <<"_" << enum_string(u) << " --> " << val;
      return val;
    }
    
    Ravelin::Origin3d get_foot_value(const std::string& id, unit_e u)
    {
      
      Ravelin::Origin3d val;
      get_foot_value(id,u,val);
      return val;
    }
    
    void set_foot_value(unit_e u, const std::map<std::string,Ravelin::Origin3d>& val)
    {
      check_phase(u);
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      std::map<std::string, Ravelin::Origin3d >::const_iterator it;
      for (it=val.begin(); it != val.end(); it++) {
        std::map<std::string, Ravelin::Origin3d >::iterator jt = _foot_state[u].find((*it).first);
        if(jt != _foot_state[u].end()){
          OUT_LOG(logDEBUG) << "Set: foot "<< (*it).first << "_" << enum_string(u) << " <-- " << (*it).second;
          (*jt).second = (*it).second;
          _foot_is_set[(*it).first] = true;
        }
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
    }
    
    std::map<std::string,Ravelin::Origin3d>& get_foot_value(unit_e u, std::map<std::string,Ravelin::Origin3d>& val)
    {
#ifdef USE_THREADS
      pthread_mutex_lock(&_state_mutex);
#endif
      std::map<std::string, Ravelin::Origin3d >::iterator it;
      for (it=_foot_state[u].begin(); it != _foot_state[u].end(); it++) {
        OUT_LOG(logDEBUG) << "Get: foot "<< (*it).first << "_" << enum_string(u) << " --> " << (*it).second;
        val[(*it).first] = (*it).second;
      }
#ifdef USE_THREADS
      pthread_mutex_unlock(&_state_mutex);
#endif
      return val;
    }
    
    std::map<std::string,Ravelin::Origin3d> get_foot_value(unit_e u)
    {
      std::map<std::string,Ravelin::Origin3d> val;
      get_foot_value(u,val);
      return val;
    }
    
    const std::vector<std::string>& get_foot_ids(){
      return _foot_ids;
    }
    
    void init_state(){
      check_phase(initialization);
      reset_contact();
      // TODO: make this more efficient ITERATORS dont work
      //      std::map<std::string,Ravelin::VectorNd>::iterator it;
      
      const int num_state_units = 8;
      unit_e state_units[num_state_units] = {position,position_goal,velocity,velocity_goal,acceleration,acceleration_goal,load, load_goal};
      for(int j=0; j<num_state_units;j++){
        unit_e& u = state_units[j];
        _foot_state[u] = std::map<std::string, Ravelin::Origin3d >();
        const std::vector<std::string>& foot_keys = _foot_ids;
        for(int i=0;i<foot_keys.size();i++){
          _foot_state[u][foot_keys[i]].set_zero();
          _foot_is_set[foot_keys[i]] = false;
        }
        
        _state[u] = std::map<std::string, Ravelin::VectorNd >();
        const std::vector<std::string>& keys = _joint_ids;
        for(int i=0;i<keys.size();i++){
          const int N = get_joint_dofs(keys[i]);
          _state[u][keys[i]].set_zero(N);
        }
      }
    }
    
    void reset_state(){
      check_phase(clean_up);
      reset_contact();
      // TODO: make this more efficient ITERATORS dont work
      //      std::map<std::string,Ravelin::VectorNd>::iterator it;
      
      const int num_state_units = 8;
      unit_e state_units[num_state_units] = {position,position_goal,velocity,velocity_goal,acceleration,acceleration_goal,load, load_goal};
      for(int j=0; j<num_state_units;j++){
        unit_e& u = state_units[j];
        const std::vector<std::string>& foot_keys = _foot_ids;
        for(int i=0;i<foot_keys.size();i++){
          _foot_state[u][foot_keys[i]].set_zero();
          _foot_is_set[foot_keys[i]] = false;
        }
        
        const std::vector<std::string>& keys = _joint_ids;
        for(int i=0;i<keys.size();i++){
          const int N = get_joint_dofs(keys[i]);
          _state[u][keys[i]].set_zero(N);
        }
      }
    }
  protected:
    
    /// @brief Sets up robot after construction and parameter import
    void init_robot();
    
    /// @brief Pulls data from _state and updates robot data
    void update();
    
  private:
    /// @brief Update poses of robot based on  currently set 'generalized' parameters
    void update_poses();
    
  public:
    /// @brief Calcultate energy of robot
    double calc_energy(const Ravelin::VectorNd& v, const Ravelin::MatrixNd& M) const;
    
    /// @brief Calculate Center of mass(x,xd,xdd,zmp)
    void calc_com();
    
    /// @brief Set Plugin internal model to input state
    void set_model_state(const Ravelin::VectorNd& q,const Ravelin::VectorNd& qd = Ravelin::VectorNd::zero(0));
    
    /// @brief Calculate N (normal), S (1st tangent), T (2nd tangent) contact jacobians
    void calc_contact_jacobians(const Ravelin::VectorNd& q, std::vector<boost::shared_ptr<const contact_t> > c ,Ravelin::MatrixNd& N,Ravelin::MatrixNd& S,Ravelin::MatrixNd& T);
    
    /// @brief Calculate N (normal), S (1st tangent), T (2nd tangent) contact jacobians
    void calc_contact_jacobians(const Ravelin::VectorNd& q, std::vector<boost::shared_ptr<contact_t> > c ,Ravelin::MatrixNd& N,Ravelin::MatrixNd& S,Ravelin::MatrixNd& T);
    
    /// @brief Calculate 6x(N+6) jacobian for point(in frame) on link at state q
    Ravelin::MatrixNd calc_jacobian(const Ravelin::VectorNd& q, const std::string& link, Ravelin::Origin3d point);
    
    /// @brief Resolved Motion Rate control (iterative inverse kinematics)
    /// iterative inverse kinematics for a 3d (linear) goal
    void RMRC(const end_effector_t& foot,const Ravelin::VectorNd& q,const Ravelin::Origin3d& goal,Ravelin::VectorNd& q_des, double TOL = 1e-4);
    
    /// @brief Resolved Motion Rate control (iterative inverse kinematics)
    /// iterative inverse kinematics for a 6d (linear and angular) goal
    void RMRC(const end_effector_t& foot,const Ravelin::VectorNd& q,const Ravelin::VectorNd& goal,Ravelin::VectorNd& q_des, double TOL = 1e-4);
    
    /// @brief N x 3d kinematics
    Ravelin::VectorNd& link_kinematics(const Ravelin::VectorNd& x,const end_effector_t& foot,const boost::shared_ptr<const Ravelin::Pose3d> frame, const Ravelin::Origin3d& goal, Ravelin::VectorNd& fk, Ravelin::MatrixNd& gk);
    
    /// @brief N x 6d Jacobian
    Ravelin::MatrixNd& link_jacobian(const Ravelin::VectorNd& x,const end_effector_t& foot,const boost::shared_ptr<const Ravelin::Pose3d> frame, Ravelin::MatrixNd& gk);
    
    /// @brief N x 6d kinematics
    Ravelin::VectorNd& link_kinematics(const Ravelin::VectorNd& x,const end_effector_t& foot,const boost::shared_ptr<const Ravelin::Pose3d> frame, const Ravelin::VectorNd& goal, Ravelin::VectorNd& fk, Ravelin::MatrixNd& gk);
    
    Ravelin::VectorNd& dist_to_goal(const Ravelin::VectorNd& x,const end_effector_t& foot,const boost::shared_ptr<const Ravelin::Pose3d> frame, const Ravelin::Origin3d& goal, Ravelin::VectorNd& dist);
    
    
    void end_effector_inverse_kinematics(
                                         const std::vector<std::string>& foot_id,
                                         const std::vector<Ravelin::Origin3d>& foot_pos,
                                         const std::vector<Ravelin::Origin3d>& foot_vel,
                                         const std::vector<Ravelin::Origin3d>& foot_acc,
                                         const Ravelin::VectorNd& q,
                                         Ravelin::VectorNd& q_des,
                                         Ravelin::VectorNd& qd_des,
                                         Ravelin::VectorNd& qdd_des, double TOL = 1e-4);
    
    void calc_generalized_inertia(const Ravelin::VectorNd& q, Ravelin::MatrixNd& M);
    
    const boost::shared_ptr<Ravelin::RigidBodyd> get_root_link(){return _root_link;}
    
    int joint_dofs(){return NUM_JOINT_DOFS;}
    
    boost::shared_ptr<Ravelin::ArticulatedBodyd>& get_abrobot(){
      return _abrobot;
    };

  private:
    /// @brief N x (3/6)d kinematics for RMRC
    Ravelin::VectorNd& contact_kinematics(const Ravelin::VectorNd& x,const end_effector_t& foot, Ravelin::VectorNd& fk, Ravelin::MatrixNd& gk);
    
    boost::shared_ptr<Ravelin::ArticulatedBodyd>        _abrobot;
    
    std::map<std::string,boost::shared_ptr<Ravelin::RigidBodyd> > _id_link_map;
    std::map<std::string,boost::shared_ptr<Ravelin::RigidBodyd> > _id_foot_map;
    boost::shared_ptr<Ravelin::RigidBodyd> _root_link;
    std::map<std::string,boost::shared_ptr<Ravelin::Jointd> > _id_joint_map;
    std::vector<std::string> _link_ids;
    std::vector<std::string> _joint_ids;
    std::vector<std::string> _foot_ids;
    
    // NDFOFS for forces, accel, & velocities
    unsigned NDOFS, NUM_JOINT_DOFS;
    
    std::vector<bool> _disabled_dofs; 
    
    /// set up internal models after kineamtic model is set (called from init)
    void compile();
  };
}

#endif // ROBOT_H