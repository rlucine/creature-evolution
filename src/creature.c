/**********************************************************//**
 * @file creature.c
 * @brief Implementation of generic mass-spring creatures
 * as well as their behaviors.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

// This project
#include "debug.h"			// assert, eprintf
#include "vector.h"			// VECTOR
#include "integral.h"		// INTEGRAL
#include "random.h"			// randint
#include "creature.h"		// CREATURE

/// The system integrator
static INTEGRAL integrate = &EulerMethod;

#define ACTION_DENSITY 0.5

/// Bounciness as a node hits the ground.
#define RESTITUTION 0.6
#define GRAVITY -9.8

#define MIN_STRENGTH 0.001
#define MAX_STRENGTH 100.0

#define MIN_POSITION -1.0
#define MAX_POSITION 1.0

#define MIN_FRICTION 0.0
#define MAX_FRICTION 1.0

#define MIN_CONTRACTED_LENGTH 0.25
#define MIN_EXTENDED_LENGTH 0.5
#define MAX_MUSCLE_LENGTH 2.0

/*============================================================*
 * Random creature generation
 *============================================================*/
void creature_CreateRandom(CREATURE *creature) {
    // Create random nodes and muscles
    creature->nNodes = randint(MIN_NODES, MAX_NODES);
    creature->nMuscles = randint(creature->nNodes, MAX_MUSCLES);
    
    // Generate the random nodes. Assume all nodes reside
    // inside the unit sphere for simplicity.
    for (int i = 0; i < creature->nNodes; i++) {
        NODE *node = &creature->nodes[i];
        
        // Initialize this node's position
        node->position.x = uniform(MIN_POSITION, MAX_POSITION);
        node->position.y = uniform(0.0, MAX_POSITION);
        node->position.z = uniform(MIN_POSITION, MAX_POSITION);
    
        // Set velocity and acceleration to zero
        vector_Set(&node->velocity, 0.0, 0.0, 0.0);
        vector_Set(&node->acceleration, 0.0, 0.0, 0.0);
        
        // Random frictionness
        node->friction = uniform(MIN_FRICTION, MAX_FRICTION);
    }
    
    // Generate the random muscles. This must ensure all
    // the nodes are attached to each other so we don't get
    // a weird degenerate creature.
    for (int i = 0; i < creature->nMuscles; i++) {
        MUSCLE *muscle = &creature->muscles[i];
        
        // Ensure each node is touched by at least one muscle
        if (i < creature->nNodes) {
            muscle->first = i;
        } else {
            muscle->second = randint(0, creature->nNodes-1);
        }
        
        // Random destination node, but no self-muscles
        // as those would do nothing.
        muscle->second = randint(0, creature->nNodes);
        if (muscle->first == muscle->second) {
			// Jump to next if a self-muscle appears
			muscle->second = (muscle->second + 1) % creature->nNodes;
		}
        
        // Get random contract or expand lengths
        muscle->extended = uniform(MIN_EXTENDED_LENGTH, MAX_MUSCLE_LENGTH);
        muscle->contracted = uniform(MIN_CONTRACTED_LENGTH, muscle->extended);
        muscle->isContracted = false;
        
        // Random muscle strength
        muscle->strength = uniform(MIN_STRENGTH, MAX_STRENGTH);
    }
    
    // Generate random behaviors
    for (int b = 0; b < N_BEHAVIORS; b++) {
        MOTION *motion = &creature->behavior[b];
        // Fill action set with nulls
        for (int i = 0; i < MAX_ACTIONS; i++) {
            if (uniform(0.0, 1.0) < ACTION_DENSITY) {
                // Generate a real action
                motion->action[i] = randint(0, creature->nMuscles-1);
            } else {
                // Generate a no-op
                motion->action[i] = MUSCLE_NONE;
            }
        }
    }
}

/**********************************************************//**
 * @struct FREE_VARIABLE
 * @brief Lists all the possible mutations that can occur.
 **************************************************************/
typedef enum {
	NODE_POSITION,
	NODE_FRICTION,
	MUSCLE_ANCHOR,
	MUSCLE_EXTENDED,
	MUSCLE_CONTRACTED,
	MUSCLE_STRENGTH,
	BEHAVIOR_ADD,
	BEHAVIOR_REMOVE,
} MUTATION;

/// The number of unique possible mutations.
#define N_MUTATIONS 11
	
/*============================================================*
 * Randomly mutate the creature
 *============================================================*/
void creature_Mutate(CREATURE *creature) {
	// Pick any mutation to occur
	MUTATION mutation = randint(0, N_MUTATIONS-1);
	
	// Pre-generate all the random numbers
	NODE *node = &creature->nodes[randint(0, creature->nNodes-1)];
	MUSCLE *muscle = &creature->muscles[randint(0, creature->nMuscles-1)];
	MOTION *behavior = &creature->behavior[randint(0, N_BEHAVIORS-1)];
	int action = randint(0, MAX_ACTIONS-1);
	
	// Apply the mutations
	switch (mutation) {
	case NODE_POSITION:
		// Change a random node position
		node->position.x = uniform(MIN_POSITION, MAX_POSITION);
		node->position.y = uniform(0.0, MAX_POSITION);
		node->position.z = uniform(MIN_POSITION, MAX_POSITION);
		break;
		
	case NODE_FRICTION:
		// Change a random node friction
		node->friction = uniform(MIN_FRICTION, MAX_FRICTION);
		break;
	
	case MUSCLE_ANCHOR:
		// Changes a random muscle anchor point
		muscle->second = randint(0, creature->nMuscles-1);
		if (muscle->first == muscle->second) {
			muscle->second = (muscle->second + 1) % creature->nNodes;
		}
		break;
	
	case MUSCLE_EXTENDED:
		// Change a muscle extended length
		muscle->extended = uniform(muscle->contracted, MAX_MUSCLE_LENGTH);
		break;
	
	case MUSCLE_CONTRACTED:
		// Change muscle contracted length
		muscle->contracted = uniform(MIN_CONTRACTED_LENGTH, muscle->extended);
		break;
		
	case MUSCLE_STRENGTH:
		// Change muscle strength
		muscle->strength = uniform(MIN_STRENGTH, MAX_STRENGTH);
		break;
	
	case BEHAVIOR_ADD:
		// Add  anew action to the stream
		behavior->action[action] = randint(0, creature->nMuscles-1);
		break;
	
	case BEHAVIOR_REMOVE:
		// Delete an action from the stream
		behavior->action[action] = MUSCLE_NONE;
		break;
	
	default:
		break;
	}
}

/*============================================================*
 * Creature breeding interchange
 *============================================================*/
void creature_Breed(const CREATURE *mother, const CREATURE *father, CREATURE *child) {
	// Cross the genetic information of the mother and father to
	// create child information.
	
	// Inherit body structure from either parent
	if (randint(0, 1)) {
		child->nNodes = mother->nNodes;
		child->nMuscles = mother->nMuscles;
	} else {
		child->nNodes = father->nNodes;
		child->nMuscles = father->nMuscles;
	}
	
	// Inherit actual node properties
	for (int i = 0; i < child->nNodes; i++) {
		// Inherit this node from either parent
		const CREATURE *selected;
		if (randint(0, 1) == 0 && i < mother->nNodes) {
			selected = mother;
		} else {
			selected = father;
		}
		assert(i < selected->nNodes);
		
		// Copy over the node data
		child->nodes[i] = selected->nodes[i];
	}
	
	// Inherit muscles
	for (int i = 0; i < child->nMuscles; i++) {
		// Inherit the muscle from either parent
		const CREATURE *selected;
		if (randint(0, 1) == 0 && i < mother->nMuscles) {
			selected = mother;
		} else {
			selected = father;
		}
		assert(i < selected->nNodes);
		
		// Copy over the muscle data, ensuring that the
		// node indices are actually valid nodes.
		child->muscles[i] = selected->muscles[i];
		
		// This is essentially a mutation: if there
		// are fewer nodes, rebind the muscle to a
		// valid node.
		int first = randint(0, child->nNodes-1);
		int second = randint(0, child->nNodes-1);
		if (first == second) {
			// Jump to next if a self-muscle appears
			second = (second + 1) % child->nNodes;
		}
		child->muscles[i].first = first;
		child->muscles[i].second = second;
	}
	
	// Inherit behaviors: pick a cross-over point within
	// the action stream and copy.
	for (int i = 0; i < N_BEHAVIORS; i++) {
		// Unroll the if-condition for efficiency
		int crossover = randint(0, MAX_ACTIONS-1);
		for (int j = 0; j < crossover; j++) {
			child->behavior[i].action[j] = mother->behavior[i].action[j];
		}
		for (int j = crossover; j < MAX_ACTIONS; j++) {
			child->behavior[i].action[j] = father->behavior[i].action[j];
		}
	}
	
	// Mutate the child at random
	int nMutations = randint(0, MAX_NODES);
	for (int i = 0; i < nMutations; i++) {
		creature_Mutate(child);
	}
}

/*============================================================*
 * Creature evaluation
 *============================================================*/
void creature_Update(CREATURE *creature, float dt) {
	// Updates the creature based on the current state of all
	// of its nodes and muscles. This update does not attempt
	// to animate a behavior or make changes to the creature's
	// muscle activation state.
	
	// Zero out all the node accelerations and apply
	// just the gravitational force.
	VECTOR gravity = {0.0, GRAVITY, 0.0};
	for (int i = 0; i < creature->nNodes; i++) {
		creature->nodes[i].acceleration = gravity;
	}
	
	// Compute all the muscle forces and apply them
	// as node accelerations. We have already assumed
	// that all nodes have uniform mass for simplicity.
	for (int i = 0; i < creature->nMuscles; i++) {
		// Get pointers to all relevant data
		MUSCLE *muscle = &creature->muscles[i];
		NODE *first = &creature->nodes[muscle->first];
		NODE *second = &creature->nodes[muscle->second];
		if (iszero(muscle->strength)) {
			// Early exit on this loop
			continue;
		}
		
		// Get the actual muscle length
		VECTOR delta = second->position;
		vector_Subtract(&delta, &first->position);
		float length = vector_Length(&delta);
		if (iszero(length)) {
			// Muscle has no length somehow, this should
			// be impossible but let's ignore it.
			continue;
		}
		
		// Get the muscle force. The strength is per unit
		// of target length, so divide that out too.
		float targetLength = muscle->isContracted? muscle->contracted: muscle->extended;
		float forceMagnitude = (muscle->strength/targetLength)*(targetLength - length);
		
		// Normalize delta and scale by force magnitude all
		// in one multiplication step.
		VECTOR force = delta;
		vector_Multiply(&force, forceMagnitude / length);
		
		//
		// TODO: velocity damping force here?
		//
		
		// Apply the force to each of the endpoints, assuming
		// all the masses are uniform.
		vector_Add(&first->acceleration, &force);
		vector_Subtract(&second->acceleration, &force);
	}
	
	// Apply frictional force based on the contact and
	// current velocity.
	for (int i = 0; i < creature->nNodes; i++) {
		// Skip nodes that aren't in collision with the ground
		NODE *node = &creature->nodes[i];
		if (!iszero(node->position.y) || iszero(node->friction)) {
			// Early loop exit
			continue;
		}
		
		// Get the frictional force, which points opposite of the
		// velocity of the node and is scaled by the frictioniness.
		VECTOR friction = node->velocity;
		if (vector_IsZero(&friction)) {
			// No frictional force
			continue;
		}
		vector_Normalize(&friction);
		vector_Multiply(&friction, -node->friction);
		
		// Apply the frictional force
		vector_Add(&node->acceleration, &friction);
	}

	// Integrate the positions
	for (int i = 0; i < creature->nNodes; i++) {
		NODE *node = &creature->nodes[i];
		integrate(&node->velocity, &node->acceleration, dt);
		integrate(&node->position, &node->velocity, dt);
		
		// Collision check
		if (iszero(node->position.y) || node->position.y < 0.0) {
			node->position.y = 0.0;
			node->velocity.y *= -RESTITUTION;
		}
	}
}
