#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xmlutils.h>
#include <crm/cib.h>
#include <glib.h>
#include <libxml/tree.h>

#include <pengine.h>

color_t *create_color(GSListPtr nodes, gboolean create_only);
void add_color_to_rsc(resource_t *rsc, color_t *color);

gint sort_rsc_priority(gconstpointer a, gconstpointer b);
gint sort_cons_strength(gconstpointer a, gconstpointer b);
gint sort_color_weight(gconstpointer a, gconstpointer b);
gint sort_node_weight(gconstpointer a, gconstpointer b);


gboolean unpack_constraints(xmlNodePtr constraints);
gboolean unpack_resources(xmlNodePtr resources);
gboolean unpack_nodes(xmlNodePtr nodes);
gboolean unpack_status(xmlNodePtr status);

gboolean apply_node_constraints(GSListPtr constraints, 
				GSListPtr resources,
				GSListPtr nodes);
void color_resource(resource_t *lh_resource);

color_t *find_color(GSListPtr candidate_colors, color_t *other_color);
gboolean is_active(rsc_constraint_t *cons);
rsc_constraint_t *invert_constraint(rsc_constraint_t *constraint);
gboolean filter_nodes(resource_t *rsc);
resource_t *pe_find_resource(GSListPtr rsc_list, const char *id_rh);
node_t *pe_find_node(GSListPtr node_list, const char *id);
gboolean choose_node_from_list(color_t *color,
			       GSListPtr nodes);
rsc_constraint_t *copy_constraint(rsc_constraint_t *constraint);

GSListPtr node_list_dup(GSListPtr list1);
GSListPtr node_list_and(GSListPtr list1, GSListPtr list2);
GSListPtr node_list_xor(GSListPtr list1, GSListPtr list2);
GSListPtr node_list_minus(GSListPtr list1, GSListPtr list2);
gboolean node_list_eq(GSListPtr list1, GSListPtr list2);
node_t *node_copy(node_t *this_node) ;
node_t *find_list_node(GSListPtr list, const char *id);


gboolean unpack_rsc_to_attr(xmlNodePtr xml_obj);
gboolean unpack_rsc_to_node(xmlNodePtr xml_obj);
gboolean unpack_rsc_to_rsc (xmlNodePtr xml_obj);
gboolean choose_color(resource_t *lh_resource, GSListPtr candidate_colors);
gboolean strict_postproc(rsc_constraint_t *constraint,
			 color_t *local_color,
			 color_t *other_color);
gboolean strict_preproc(rsc_constraint_t *constraint,
			color_t *local_color,
			color_t *other_color);
gboolean update_node_weight(rsc_constraint_t *cons, node_t *node_rh);
gboolean process_node_lrm_state(node_t *node, xmlNodePtr lrm_state);
GSListPtr match_attrs(xmlNodePtr attr_exp, GSListPtr node_list);

GSListPtr rsc_list = NULL; 
GSListPtr node_list = NULL;
GSListPtr cons_list = NULL;
GSListPtr colors = NULL;
GSListPtr stonith_list = NULL;
GSListPtr shutdown_list = NULL;
color_t *current_color = NULL;
color_t *no_color = NULL;
gboolean pe_debug = FALSE;
gboolean pe_debug_saved = FALSE;
int max_valid_nodes = 0;

gboolean
stage0(xmlNodePtr cib)
{
	xmlNodePtr cib_nodes = get_object_root("nodes", cib);
	xmlNodePtr cib_resources = get_object_root("resources", cib);
	xmlNodePtr cib_constraints = get_object_root("constraints", cib);
	xmlNodePtr cib_status = get_object_root("status", cib);

	unpack_nodes(cib_nodes->children);
	unpack_resources(cib_resources->children);
	unpack_status(cib_status->children);
	unpack_constraints(cib_constraints->children);

	return TRUE;
} 

gboolean
stage1(GSListPtr nodes)
{
	int lpc = 0;
	// filter out unavailable nodes
	slist_iter(
		node, node_t, node_list, lpc,
		pdebug_action(print_node("Before", node, FALSE));
		);
	
	slist_iter(
		node, node_t, nodes, lpc,
		if(node == NULL) {
			// error
		} else if(node->weight < 0.0
			  || node->details->online == FALSE
			  || node->details->type == node_ping) {
/* 			pdebug_action(print_node("Removing", node)); */
/* 			nodes = g_slist_remove(nodes, node); */
/* 			lpc--; */
		} else {
			max_valid_nodes++;
		}	
		);

	node_list = nodes;
	apply_node_constraints(cons_list, node_list, rsc_list);

	return TRUE;
} 

void
color_resource(resource_t *lh_resource)
{
	int lpc = 0;

	pdebug_action(print_resource("Coloring", lh_resource, FALSE));
	
	if(lh_resource->provisional == FALSE) {
			// already processed this resource
		return;
	}
	lh_resource->constraints = g_slist_sort(lh_resource->constraints, sort_cons_strength);

	pdebug("=== Pre-processing");
	//------ Pre-processing
	for(; lh_resource->runnable
		    && lpc < g_slist_length(lh_resource->constraints); lpc++) {
		color_t *other_color = NULL;
		color_t *local_color = NULL;
		rsc_constraint_t *constraint = (rsc_constraint_t*)
			g_slist_nth_data(lh_resource->constraints, lpc);

		pdebug_action(print_cons("Processing constraint", constraint, FALSE));

		if(constraint->is_placement == FALSE) {
			continue;
		}

		if(constraint->type != rsc_to_rsc) {
			continue;
		}
		if(constraint->rsc_rh == NULL) {
			cl_log(LOG_ERR, "rsc_rh was NULL for %s", constraint->id);
			continue;
		}
		other_color = constraint->rsc_rh->color;
		local_color = find_color(lh_resource->candidate_colors, other_color);
		strict_preproc(constraint, local_color, other_color);
		
	}
	
	// filter out nodes with a negative weight
	filter_nodes(lh_resource);
  
	/* Choose a color from the candidates or,
	 *  create a new one if no color is suitable 
	 * (this may need modification pending further napkin drawings)
	 */
	choose_color(lh_resource, lh_resource->candidate_colors);	
  
	pdebug("* Colors %d, Nodes %d",
		      g_slist_length(colors),
		      max_valid_nodes);
	
	if(lh_resource->provisional
		&& g_slist_length(colors) < max_valid_nodes) {
		// Create new color
		pdebug("Create a new color");
		current_color = create_color(lh_resource->allowed_nodes, FALSE);
		lh_resource->color = current_color;
		lh_resource->provisional = FALSE;
	} else if(lh_resource->provisional) {
		cl_log(LOG_ERR, "Could not color resource %s", lh_resource->id);
		print_resource("ERROR: No color", lh_resource, FALSE);
		lh_resource->color = no_color;
		lh_resource->provisional = FALSE;
		
	}

	pdebug_action(print_resource("Post-processing", lh_resource, FALSE));

	//------ Post-processing

	for(lpc = 0; lh_resource->color != no_color
		    && lpc < g_slist_length(lh_resource->constraints);lpc++) {
		color_t *local_color = lh_resource->color;
		color_t *other_color = NULL;
		rsc_constraint_t *constraint = (rsc_constraint_t*)
			g_slist_nth_data(lh_resource->constraints, lpc);

		if(constraint->is_placement == FALSE) {
			continue;
		} else if(constraint->type != rsc_to_rsc) {
			continue;
		}
		
		other_color = find_color(constraint->rsc_rh->candidate_colors,
					 local_color);

		strict_postproc(constraint, local_color, other_color);
	}
	
	pdebug_action(print_resource("Colored", lh_resource, FALSE));
}

gboolean
stage2(GSListPtr sorted_rsc, 
       GSListPtr sorted_nodes, 
       GSListPtr operations)
{
	int lpc = 0; 
	// Set initial color
	// Set color.candidate_nodes = all active nodes
	no_color = create_color(NULL, TRUE);
	colors = NULL; // leave the "no" color out of the list
	current_color = create_color(node_list, FALSE);
	
	// Set resource.color = color (all resources)
	// Set resource.provisional = TRUE (all resources)
	slist_iter(
		this_resource, resource_t, sorted_rsc, lpc,

		this_resource->color = current_color;
		this_resource->provisional = TRUE;
		);

	pdebug("initialized resources to default color");
  
	// Take (next) highest resource
	for(lpc = 0; lpc < g_slist_length(sorted_rsc); lpc++) {
		resource_t *lh_resource = (resource_t*)g_slist_nth_data(sorted_rsc, lpc);

		// if resource.provisional == FALSE, repeat 
		if(lh_resource->provisional == FALSE
		   || lh_resource->runnable == FALSE) {
			// already processed this resource
			continue;
		}
    
		color_resource(lh_resource);
		// next resource
	}
	return TRUE;
}

gboolean
stage3(void)
{
	// not sure if this is a good ide aor not
	if(g_slist_length(colors) > max_valid_nodes) {
		// we need to consolidate some
	} else if(g_slist_length(colors) < max_valid_nodes) {
		// we can create a few more
	}
	
	
	return TRUE;
}

gboolean
stage5(GSListPtr resources)
{
	int lpc = 0;
	slist_iter(
		rsc, resource_t, resources, lpc,
		if(rsc->runnable == FALSE) {
			cl_log(LOG_ERR,
			       "Resource %s was not runnable",
			       rsc->id);
			if(rsc->cur_node_id != NULL) {
				
				cl_log(LOG_WARNING,
				       "Stopping Resource (%s) on node %s",
				       rsc->id,
				       rsc->cur_node_id);
			}

		} else if(rsc->color->details->chosen_node == NULL) {
			cl_log(LOG_ERR,
			       "Could not allocate Resource %s",
			       rsc->id);
			if(rsc->cur_node_id != NULL) {
				
				cl_log(LOG_WARNING,
				       "Stopping Resource (%s) on node %s",
				       rsc->id,
				       rsc->cur_node_id);
			}
			
		} else if(safe_str_eq(rsc->cur_node_id,
				      rsc->color->details->chosen_node->details->id)){
			cl_log(LOG_DEBUG,
			       "No change for Resource %s (%s)",
			       rsc->id,
			       rsc->cur_node_id);
			
		} else if(rsc->cur_node_id == NULL) {
			cl_log(LOG_INFO,
			       "Starting Resource %s on %s",
			       rsc->id,
			       rsc->color->details->chosen_node->details->id);
			
		} else {
			cl_log(LOG_INFO,
			       "Moving Resource %s from %s to %s",
			       rsc->id,
			       rsc->cur_node_id,
			       rsc->color->details->chosen_node->details->id);
		}
		);
	
	
	return TRUE;
}

#define color_n_nodes color_n->details->candidate_nodes
#define color_n_plus_1_nodes color_n_plus_1->details->candidate_nodes
gboolean
stage4(GSListPtr colors)
{

	int lpc = 0;
	color_t *color_n = NULL;
	color_t *color_n_plus_1 = NULL;
	
	for(lpc = 0; lpc < g_slist_length(colors); lpc++) {
		color_n = color_n_plus_1;
		color_n_plus_1 = (color_t*)g_slist_nth_data(colors, lpc);

		pdebug_action(print_color("Choose node for...", color_n, FALSE));
//		print_color(color_n_plus_1, FALSE);
		
		if(color_n == NULL) {
			continue;
		}


		GSListPtr xor = node_list_xor(color_n_nodes,
					      color_n_plus_1_nodes);
		GSListPtr minus = node_list_minus(color_n_nodes,
						  color_n_plus_1_nodes);

		if(g_slist_length(xor) == 0 || g_slist_length(minus) == 0) {
			pdebug(				      "Choose any node from our list");
			choose_node_from_list(color_n, color_n_nodes);

		} else {
			pdebug("Choose a node not in n+1");
			choose_node_from_list(color_n, minus);      
		}

	}

	// chose last color
	if(color_n_plus_1 != NULL) {
		pdebug_action(print_color("Choose node for last color...",
				   color_n_plus_1,
				   FALSE));

		choose_node_from_list(color_n_plus_1, 
				      color_n_plus_1_nodes);
	}
	pdebug("done %s", __FUNCTION__);
	return TRUE;
	
}

gboolean
choose_node_from_list(color_t *color, GSListPtr nodes)
{
	/*
	  1. Sort by weight
	  2. color.chosen_node = highest wieghted node 
	  3. remove color.chosen_node from all other colors
	*/
	int lpc = 0;
	nodes = g_slist_sort(nodes, sort_node_weight);
	color->details->chosen_node = (node_t*)g_slist_nth_data(nodes, 0);

	if(color->details->chosen_node == NULL) {
		cl_log(LOG_ERR, "Could not allocate a node for color %d", color->id);
		return FALSE;
	}

	slist_iter(
		color_n, color_t, colors, lpc,
		node_t *other_node = pe_find_node(color_n->details->candidate_nodes,
						  color->details->chosen_node->details->id);
		color_n->details->candidate_nodes =
			g_slist_remove(color_n->details->candidate_nodes,
				       other_node);
		);
	
	return TRUE;
}


gboolean
unpack_nodes(xmlNodePtr nodes)
{
	pdebug("Begining unpack... %s", __FUNCTION__);
	while(nodes != NULL) {
		pdebug("Processing node...");
		xmlNodePtr xml_obj = nodes;
		xmlNodePtr attrs = xml_obj->children;
		const char *id = xmlGetProp(xml_obj, "id");
		const char *type = xmlGetProp(xml_obj, "type");
		if(attrs != NULL) {
			attrs = attrs->children;
		}
		
		nodes = nodes->next;
	
		if(id == NULL) {
			cl_log(LOG_ERR, "Must specify id tag in <node>");
			continue;
		}
		if(type == NULL) {
			cl_log(LOG_ERR, "Must specify type tag in <node>");
			continue;
		}
		node_t *new_node = cl_malloc(sizeof(node_t));
		new_node->weight = 1.0;
		new_node->fixed = FALSE;
		new_node->details = (struct node_shared_s*)
			cl_malloc(sizeof(struct node_shared_s*));
		new_node->details->online = FALSE;
		new_node->details->unclean = FALSE;
		new_node->details->running_rsc = NULL;
		new_node->details->id = cl_strdup(id);
		new_node->details->attrs =
			g_hash_table_new(g_str_hash, g_str_equal);
		new_node->details->type = node_ping;
		if(safe_str_eq(type, "node")) {
			new_node->details->type = node_member;
		}
		

		while(attrs != NULL){
			const char *name = xmlGetProp(attrs, "name");
			const char *value = xmlGetProp(attrs, "value");
			if(name != NULL && value != NULL) {
				g_hash_table_insert(new_node->details->attrs,
						    cl_strdup(name),
						    cl_strdup(value));
			}
			attrs = attrs->next;
		}
		
		pdebug("Adding node id... %s (%p)",
			      id, new_node);

		node_list = g_slist_append(node_list, new_node);    
	}
  
	node_list = g_slist_sort(node_list, sort_node_weight);

	return TRUE;
}


gboolean 
unpack_resources(xmlNodePtr resources)
{
	pdebug("Begining unpack... %s", __FUNCTION__);
	while(resources != NULL) {
		xmlNodePtr xml_obj = resources;
		const char *id = xmlGetProp(xml_obj, "id");
		const char *priority = xmlGetProp(xml_obj, "priority");
		float priority_f = atof(priority);
		resources = resources->next;

		pdebug("Processing resource...");
		
		if(id == NULL) {
			cl_log(LOG_ERR, "Must specify id tag in <resource>");
			continue;
		}
		resource_t *new_rsc = cl_malloc(sizeof(resource_t));
		new_rsc->xml = xml_obj; // copy first 
		new_rsc->priority = priority_f; 
		new_rsc->candidate_colors = NULL;
		new_rsc->color = NULL; 
		new_rsc->runnable = TRUE; 
		new_rsc->provisional = TRUE; 
		new_rsc->allowed_nodes = node_list_dup(node_list);    
		new_rsc->constraints = NULL; 
		new_rsc->id = cl_strdup(id);

		pdebug_action(print_resource("Added", new_rsc, FALSE));
		rsc_list = g_slist_append(rsc_list, new_rsc);

	}
	rsc_list = g_slist_sort(rsc_list, sort_rsc_priority);

	return TRUE;
}



gboolean 
unpack_constraints(xmlNodePtr constraints)
{
	pdebug("Begining unpack... %s", __FUNCTION__);
	while(constraints != NULL) {
		const char *id = xmlGetProp(constraints, "id");
		xmlNodePtr xml_obj = constraints;
		constraints = constraints->next;
		if(id == NULL) {
			cl_log(LOG_ERR, "Constraint must have an id");
			continue;
		}

		pdebug("Processing constraint %s %s",
			      xml_obj->name,id);
		if(safe_str_eq("rsc_to_rsc", xml_obj->name)) {
			unpack_rsc_to_rsc(xml_obj);

		} else if(safe_str_eq("rsc_to_node", xml_obj->name)) {
			unpack_rsc_to_node(xml_obj);
			
		} else if(safe_str_eq("rsc_to_attr", xml_obj->name)) {
			unpack_rsc_to_attr(xml_obj);
			
		} else {
			cl_log(LOG_ERR, "Unsupported constraint type: %s",
			       xml_obj->name);
		}
	}

	return TRUE;
}


gboolean 
apply_node_constraints(GSListPtr constraints, 
		       GSListPtr resources,
		       GSListPtr nodes)
{
	pdebug("Applying constraints... %s", __FUNCTION__);
	int lpc = 0;
	for(lpc = 0; lpc < g_slist_length(constraints); lpc++) {
		rsc_constraint_t *cons = (rsc_constraint_t *)
			g_slist_nth_data(constraints, lpc);
		
		pdebug_action(print_cons("Applying", cons, FALSE));
		// take "lifetime" into account
		if(cons == NULL) {
			cl_log(LOG_ERR, "Constraint (%d) is NULL", lpc); 	
			continue;
			
		} else if(is_active(cons) == FALSE) {
			cl_log(LOG_INFO, "Constraint (%d) is not active", lpc); 	
			// warning
			continue;
		}
    
		resource_t *rsc_lh = cons->rsc_lh;
		if(rsc_lh == NULL) {
			cl_log(LOG_ERR, "LHS of rsc_to_node (%s) is NULL", cons->id); 	
			continue;
		}

		GSListPtr rsc_cons_list = cons->rsc_lh->constraints;
		rsc_lh->constraints = g_slist_append(rsc_cons_list, cons);

		if(cons->type == rsc_to_rsc) {
			// nothing 
			pdebug("nothing to do");
			continue;
			
		} else if(cons->type == rsc_to_node
			  || cons->type == rsc_to_attr) {
			if(cons->node_list_rh == NULL) {
				cl_log(LOG_ERR,
				       "RHS of rsc_to_node (%s) is NULL",
				       cons->id);
				continue;
			} else {
				int llpc = 0;
				slist_iter(node_rh, node_t, cons->node_list_rh, llpc,
					   update_node_weight(cons, node_rh));
			}
			
			/* dont add it to the resource,
			 *  the information is in the resouce's node list
			 */

		} else {
			// error
		}
	}
	return TRUE;
	
}


// remove nodes that are down, stopping
// create +ve rsc_to_node constraints between resources and the nodes they are running on
// anything else?
gboolean
unpack_status(xmlNodePtr status)
{
	pdebug("Begining unpack %s", __FUNCTION__);
	while(status != NULL) {
		const char *id = xmlGetProp(status, "id");
		const char *state = xmlGetProp(status, "state");
		const char *exp_state = xmlGetProp(status, "exp_state");
		xmlNodePtr lrm_state = find_xml_node(status, "lrm");
		xmlNodePtr attrs = find_xml_node(status, "attributes");

		lrm_state = find_xml_node(lrm_state, "lrm_resources");
		lrm_state = find_xml_node(lrm_state, "rsc_state");
		status = status->next;

		pdebug("Processing node %s", id);

		if(id == NULL){
			// error
			continue;
		}
		pdebug("Processing node attrs");
		
		node_t *this_node = pe_find_node(node_list, id);
		while(attrs != NULL){
			const char *name = xmlGetProp(attrs, "name");
			const char *value = xmlGetProp(attrs, "value");
			
			if(name != NULL && value != NULL) {
				pdebug("Adding %s => %s",
					      name, value);
				g_hash_table_insert(this_node->details->attrs,
						    cl_strdup(name),
						    cl_strdup(value));
			}
			attrs = attrs->next;
		}

		pdebug("determining node state");
		
		if(safe_str_eq(exp_state, "active")
		   && safe_str_eq(state, "active")) {
			// process resource, make +ve preference
			this_node->details->online = TRUE;
			
		} else {
			pdebug("remove %s", __FUNCTION__);
			// remove node from contention
			this_node->weight = -1;
			this_node->fixed = TRUE;

			pdebug("state %s, expected %s",
				      state, exp_state);
			
			if(safe_str_eq(state, "shutdown")){
				// create shutdown req
				shutdown_list = g_slist_append(shutdown_list,
							       this_node);

			} else if(safe_str_eq(exp_state, "active")
				  && safe_str_neq(state, "active")) {
				// mark unclean in the xml
				state = "unclean";
				this_node->details->unclean = TRUE;
				
				// remove any running resources from being allocated
			}
      
			if(safe_str_eq(state, "unclean")) {
				stonith_list = g_slist_append(stonith_list,
							      this_node);
			}
		}

		pdebug("Processing node lrm state");
		process_node_lrm_state(this_node, lrm_state);
		
	}
	cons_list = g_slist_sort(cons_list, sort_cons_strength);

	return TRUE;
	
}

gboolean
is_active(rsc_constraint_t *cons)
{
	return TRUE;
}



gboolean
strict_preproc(rsc_constraint_t *constraint,
	       color_t *local_color,
	       color_t *other_color)
{
	resource_t * lh_resource = constraint->rsc_lh;
	switch(constraint->strength) {
		case must:
			if(constraint->rsc_rh->runnable == FALSE) {
				cl_log(LOG_WARNING,
				       "Resource %s must run on the same node"
				       " as %s (cons %s), but %s is not"
				       " runnable.",
				       constraint->rsc_lh->id,
				       constraint->rsc_rh->id,
				       constraint->id,
				       constraint->rsc_rh->id);
				constraint->rsc_lh->runnable = FALSE;
			}
			break;
			
			// x * should * should_not = x
		case should:
			if(constraint->rsc_rh->provisional == FALSE) {
				local_color->local_weight = 
					local_color->local_weight * 2.0;
			}
				break;
		case should_not:
			if(constraint->rsc_rh->provisional == FALSE) {
				local_color->local_weight = 
					local_color->local_weight * 0.5;
			}
			pdebug("# Colors %d, Nodes %d",
				      g_slist_length(colors),
				      max_valid_nodes);
			       
			if(g_slist_length(colors) < max_valid_nodes
//			   && g_slist_length(lh_resource->candidate_colors)==1
				) {
//				create_color(lh_resource->allowed_nodes);
//			} else if(g_slist_length(lh_resource->candidate_colors)==1) {
				create_color(lh_resource->allowed_nodes, FALSE);
			} 
			
			
			break;
		case must_not:
			if(constraint->rsc_rh->provisional == FALSE) {
				lh_resource->candidate_colors =
					g_slist_remove(
						lh_resource->candidate_colors,
						local_color);
			}
			break;
		default:
			// error
			break;
	}
	return TRUE;
}

gboolean
strict_postproc(rsc_constraint_t *constraint,
		color_t *local_color,
		color_t *other_color)
{
	print_cons("Post processing", constraint, FALSE);
	
	switch(constraint->strength) {
		case must:
			if(constraint->rsc_rh->provisional == TRUE) {
				constraint->rsc_rh->color = other_color;
				constraint->rsc_rh->provisional = FALSE;
				color_resource(constraint->rsc_rh);
			}
			// else check for error
			if(constraint->rsc_lh->runnable == FALSE) {
				cl_log(LOG_WARNING,
				       "Resource %s must run on the same node"
				       " as %s (cons %s), but %s is not"
				       " runnable.",
				       constraint->rsc_rh->id,
				       constraint->rsc_lh->id,
				       constraint->id,
				       constraint->rsc_lh->id);
				constraint->rsc_rh->runnable = FALSE;
				
			}
			
			break;
			
		case should:
			break;
		case should_not:
			break;
		case must_not:
			if(constraint->rsc_rh->provisional == TRUE) {
				// check for error
			}
			break;
		default:
			// error
			break;
	}
	return TRUE;
}

gboolean
choose_color(resource_t *lh_resource, GSListPtr candidate_colors)
{
	int lpc = 0;

	if(lh_resource->runnable == FALSE) {
		lh_resource->color = no_color;
		lh_resource->provisional = FALSE;
	} else {
		GSListPtr sorted_colors = g_slist_sort(candidate_colors,
						       sort_color_weight);
		
		lh_resource->candidate_colors = sorted_colors;
	
		pdebug(			      "Choose a color from %d possibilities",
			      g_slist_length(sorted_colors));
	}

	if(lh_resource->provisional) {
		slist_iter(
			this_color, color_t,lh_resource->candidate_colors, lpc,
			GSListPtr intersection = node_list_and(
				this_color->details->candidate_nodes, 
				lh_resource->allowed_nodes);

			if(g_slist_length(intersection) != 0) {
				// TODO: merge node weights
				g_slist_free(this_color->details->candidate_nodes);
				this_color->details->candidate_nodes = intersection;
				lh_resource->color = this_color;
				lh_resource->provisional = FALSE;
				break;
			}
			);
	}
	return !lh_resource->provisional;
}

gboolean
unpack_rsc_to_node(xmlNodePtr xml_obj)	
{
	
	xmlNodePtr node_ref = xml_obj->children;
	rsc_constraint_t *new_con = cl_malloc(sizeof(rsc_constraint_t));
	const char *id_lh =  xmlGetProp(xml_obj, "from");
	const char *id =  xmlGetProp(xml_obj, "id");

	const char *mod = xmlGetProp(xml_obj, "modifier");
	const char *weight = xmlGetProp(xml_obj, "weight");
	float weight_f = atof(weight);

	resource_t *rsc_lh = pe_find_resource(rsc_list, id_lh);
	if(rsc_lh == NULL) {
		cl_log(LOG_ERR, "No resource (con=%s, rsc=%s)",
		       id, id_lh);
	}

	new_con->id = cl_strdup(id);
	new_con->rsc_lh = rsc_lh;
	new_con->type = rsc_to_node;
	new_con->rsc_rh = NULL;
	new_con->weight = weight_f;
			
	if(safe_str_eq(mod, "set")){
		new_con->modifier = set;
	} else if(safe_str_eq(mod, "inc")){
		new_con->modifier = inc;
	} else if(safe_str_eq(mod, "dec")){
		new_con->modifier = dec;
	} else {
		// error
	}
/*
  <rsc_to_node>
  <node_ref id= type= name=/>
  <node_ref id= type= name=/>
  <node_ref id= type= name=/>
*/		
//			

	while(node_ref != NULL) {
		const char *id_rh = xmlGetProp(node_ref, "name");
		node_t *node_rh =  pe_find_node(node_list, id_rh);
		if(node_rh == NULL) {
			// error
			cl_log(LOG_ERR,
			       "node %s (from %s) not found",
			       id_rh, node_ref->name);
			continue;
		}
		
		new_con->node_list_rh =
			g_slist_append(new_con->node_list_rh,
				       node_rh);

		
		/* dont add it to the resource,
		 *  the information is in the resouce's node list
		 */
		node_ref = node_ref->next;
	}
	cons_list = g_slist_append(cons_list, new_con);

	return TRUE;
}


gboolean
unpack_rsc_to_attr(xmlNodePtr xml_obj)	
{
/*
       <rsc_to_attr id="cons4" from="rsc2" weight="20.0" modifier="inc">
       <attr_expression id="attr_exp_1"/>
          <node_match id="node_match_1" type="has_attr" target="cpu"/>
          <node_match id="node_match_2" type="attr_value" target="kernel" value="2.6"/>
       </attr_expression>
       <attr_expression id="attr_exp_2"/>
          <node_match id="node_match_3" type="has_attr" target="hdd"/>
          <node_match id="node_match_4" type="attr_value" target="kernel" value="2.4"/>
       </attr_expression>

   Translation:
       give any node a +ve weight of 20.0 to run rsc2 if:
          attr "cpu" is set _and_ "kernel"="2.6", _or_
	  attr "hdd" is set _and_ "kernel"="2.4"

   Further translation:
       2 constraints that give any node a +ve weight of 20.0 to run rsc2
       cons1: attr "cpu" is set and "kernel"="2.6"
       cons2: attr "hdd" is set and "kernel"="2.4"
       
 */
	
	xmlNodePtr attr_exp = xml_obj->children;
	const char *id_lh   =  xmlGetProp(xml_obj, "from");
	const char *mod     = xmlGetProp(xml_obj, "modifier");
	const char *weight  = xmlGetProp(xml_obj, "weight");
	const char *id      = xmlGetProp(attr_exp, "id");
	float weight_f = atof(weight);
	enum con_modifier a_modifier = modifier_none;
	
	resource_t *rsc_lh = pe_find_resource(rsc_list, id_lh);
	if(rsc_lh == NULL) {
		cl_log(LOG_ERR, "No resource (con=%s, rsc=%s)",
		       id, id_lh);
		return FALSE;
	}
			
	if(safe_str_eq(mod, "set")){
		a_modifier = set;
	} else if(safe_str_eq(mod, "inc")){
		a_modifier = inc;
	} else if(safe_str_eq(mod, "dec")){
		a_modifier = dec;
	} else {
		// error
	}		

	if(attr_exp == NULL) {
		cl_log(LOG_WARNING, "no attrs for constraint %s", id);
	}
	
	while(attr_exp != NULL) {
		const char *id_rh = xmlGetProp(attr_exp, "name");
		const char *id = xmlGetProp(attr_exp, "id");
		rsc_constraint_t *new_con = cl_malloc(sizeof(rsc_constraint_t));
		new_con->id = cl_strdup(id);
		new_con->rsc_lh = rsc_lh;
		new_con->type = rsc_to_attr;
		new_con->rsc_rh = NULL;
		new_con->weight = weight_f;
		new_con->modifier = a_modifier;

		new_con->node_list_rh = match_attrs(attr_exp, node_list);
		
		if(new_con->node_list_rh == NULL) {
			// error
			cl_log(LOG_ERR,
			       "node %s (from %s) not found",
			       id_rh, attr_exp->name);
		}
		pdebug_action(print_cons("Added", new_con, FALSE));
		cons_list = g_slist_append(cons_list, new_con);

		/* dont add it to the resource,
		 *  the information is in the resouce's node list
		 */
		attr_exp = attr_exp->next;
	}
	return TRUE;
}

gboolean
update_node_weight(rsc_constraint_t *cons, node_t *node)
{
	node_t *node_rh = pe_find_node(cons->rsc_lh->allowed_nodes,
				       node->details->id);

	if(node_rh == NULL) {
		node_t *node_tmp = pe_find_node(node_list,
						node->details->id);
		node_rh = node_copy(node_tmp);
		cons->rsc_lh->allowed_nodes =
			g_slist_append(cons->rsc_lh->allowed_nodes,
				       node_rh);
	}

	if(node_rh == NULL) {
		// error
		return FALSE;
	}

	if(node_rh->fixed) {
		// warning
		cl_log(LOG_WARNING,
		       "Constraint %s is irrelevant as the"
		       " weight of node %s is fixed as %f.",
		       cons->id,
		       node_rh->details->id,
		       node_rh->weight);
		return TRUE;
	}
	
	pdebug(		      "Constraint %s: node %s weight %s %f.",
		      cons->id,
		      node_rh->details->id,
		      modifier2text(cons->modifier),
		      node_rh->weight);
	
	switch(cons->modifier) {
		case set:
			node_rh->weight = cons->weight;
			node_rh->fixed = TRUE;
			break;
		case inc:
			node_rh->weight += cons->weight;
			break;
		case dec:
			node_rh->weight -= cons->weight;
			break;
		case modifier_none:
			// warning
			break;
	}
	return TRUE;
}

gboolean
process_node_lrm_state(node_t *node, xmlNodePtr lrm_state)
{
	pdebug("here %s", __FUNCTION__);
	
	while(lrm_state != NULL) {
		const char *id    = xmlGetProp(lrm_state, "id");
		const char *rsc_id    = xmlGetProp(lrm_state, "rsc_id");
		const char *node_id   = xmlGetProp(lrm_state, "node_id");
		const char *rsc_state = xmlGetProp(lrm_state, "rsc_state");
		resource_t *rsc_lh = pe_find_resource(rsc_list, rsc_id);
		rsc_lh->cur_node_id = cl_strdup(node_id);

		node->details->running_rsc =
			g_slist_append(node->details->running_rsc, rsc_lh);

		if(node->details->unclean) {
			rsc_lh->runnable = FALSE;
		}
		
		if((safe_str_eq(rsc_state, "starting"))
		   || (safe_str_eq(rsc_state, "started"))) {
			
			node_t *node_rh;
			rsc_constraint_t *new_cons =
				cl_malloc(sizeof(rsc_constraint_t));
			new_cons->id = cl_strdup(id); // genereate one
			new_cons->type = rsc_to_node;
			new_cons->weight = 100.0;
			new_cons->modifier = inc;
			
			new_cons->rsc_lh = rsc_lh;
			node_rh = pe_find_node(node_list, node_id);
			
			new_cons->node_list_rh = g_slist_append(NULL,
								node_rh);
					
			cons_list = g_slist_append(cons_list, new_cons);
			pdebug_action(print_cons("Added", new_cons, FALSE));
			
		} else if(safe_str_eq(rsc_state, "stop_fail")) {
			// do soemthing
		} // else no preference

		lrm_state = lrm_state->next;
	}
	return TRUE;
}

GSListPtr
match_attrs(xmlNodePtr attr_exp, GSListPtr node_list)
{
	int lpc = 0;
	GSListPtr result = NULL;
	slist_iter(
		node, node_t, node_list, lpc,
		xmlNodePtr node_match = attr_exp->children;
		gboolean accept = TRUE;
		
		while(accept && node_match != NULL) {
			const char *type =xmlGetProp(node_match, "type");
			const char *value=xmlGetProp(node_match, "value");
			const char *name =xmlGetProp(node_match, "target");
			node_match = node_match->next;
			
			if(name == NULL || type == NULL) {
				// error
				continue;
			}
			
			const char *h_val = (const char*)
				g_hash_table_lookup(node->details->attrs, name);
			
			if(h_val != NULL && safe_str_eq(type, "has_attr")){
				accept = TRUE;
			} else if(h_val == NULL
				     && safe_str_eq(type, "not_attr")) {
				accept = TRUE;
			} else if(h_val != NULL
				  && safe_str_eq(type, "attr_value")
				  && safe_str_eq(h_val, value)) {
				accept = TRUE;
			} else {
				accept = FALSE;
			}
		}
		
		if(accept) {
			result = g_slist_append(result, node);
			
		}		   
		);
	
	return result;
}

gboolean
unpack_rsc_to_rsc(xmlNodePtr xml_obj)
{
	rsc_constraint_t *new_con = cl_malloc(sizeof(rsc_constraint_t));
	rsc_constraint_t *inverted_con = NULL;
	const char *id_lh =  xmlGetProp(xml_obj, "from");
	const char *id =  xmlGetProp(xml_obj, "id");
	resource_t *rsc_lh = pe_find_resource(rsc_list, id_lh);
	if(rsc_lh == NULL) {
		cl_log(LOG_ERR, "No resource (con=%s, rsc=%s)",
		       id, id_lh);
		return FALSE;
	}

	new_con->id = cl_strdup(id);
	new_con->rsc_lh = rsc_lh;			
	new_con->type = rsc_to_rsc;
	
	const char *strength = xmlGetProp(xml_obj, "strength");
	if(safe_str_eq(strength, "must")) {
		new_con->strength = must;
		
	} else if(safe_str_eq(strength, "should")) {
		new_con->strength = should;
		
	} else if(safe_str_eq(strength, "should_not")) {
		new_con->strength = should_not;
		
	} else if(safe_str_eq(strength, "must_not")) {
		new_con->strength = must_not;
	} else {
		// error
	}
	
	const char *type = xmlGetProp(xml_obj, "type");
	if(safe_str_eq(type, "ordering")) {
		new_con->is_placement = FALSE;
		
	} else if (safe_str_eq(type, "placement")) {
		new_con->is_placement = TRUE;
		
	} else {
		// error
	}
	
	new_con->node_list_rh = NULL;
	const char *id_rh = xmlGetProp(xml_obj, "to");
	resource_t *rsc_rh = pe_find_resource(rsc_list, id_rh);
	if(rsc_rh == NULL) {
		cl_log(LOG_ERR, "No rh resource found with id %s", id_rh);
		return FALSE;
	}
	new_con->rsc_rh = rsc_rh;
	
	inverted_con = invert_constraint(new_con);
	cons_list = g_slist_insert_sorted(cons_list, inverted_con, sort_cons_strength);
	cons_list = g_slist_insert_sorted(cons_list, new_con, sort_cons_strength);

	return TRUE;
}
