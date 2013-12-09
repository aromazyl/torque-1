#ifndef JOBINFO_H_
#define JOBINFO_H_

#include <map>
#include <vector>
#include <string>

#include "data_types.h"

struct job_info
  {
  JobState state;
  const char* state_string() { return JobStateString[state]; }

  unsigned is_starving: 1; /* job has waited passed starvation time */
  unsigned can_not_run: 1; /* set in a cycle of a job cant run this cycle*/
  unsigned can_never_run: 1; /* set if a job can never be run */
  unsigned is_exclusive: 1; /* the job needs to be run exclusively */
  unsigned is_multinode: 1; /* job requeires multiple nodes */

  enum ClusterMode cluster_mode;
  char *cluster_name; /**< cluster name passed from -l cluster=...*/

  char *name;   /* name of job */
  char *comment;  /* comment field of job */
  char *account;  /* username of the owner of the job */
  char *group;   /* groupname of the owner of the job */
  char *custom_name; /**< user selected job/cluster name */

  char *nodespec; /**< processed nodespec from the server */
  pars_spec *parsed_nodespec;

  struct queue_info *queue; /* queue where job resides */
  int priority;   /* PBS priority of job */
  int sch_priority;  /* internal scheduler priority */
  time_t qtime;   /* the time the job was last queued */
  resource_req *resreq;  /* list of resources requested */
  resource_req *resused; /* a list of resources used */
  group_info *ginfo;  /* the fair share node for the owner */

  double calculated_fairshare;

  std::map< std::string, std::vector<pars_spec_node*> > schedule; /* currently considered schedule */

  /** \brief Determine whether a sjob is already planned on node */
  bool on_node(node_info *ninfo);
  /** \brief Determine whether a job is already planned on node, or any of its virtual sisters */
  bool on_host(node_info *ninfo);

  void plan_on_node(node_info* ninfo, pars_spec_node* spec);
  void unplan_from_node(node_info* ninfo, pars_spec_node* spec);
  void plan_on_queue(queue_info* qinfo);
  void plan_on_server(server_info* sinfo);

  int preprocess();

  double calculate_fairshare_cost(const std::vector<node_info *>& nodes) const;
  };

#endif /* JOBINFO_H_ */