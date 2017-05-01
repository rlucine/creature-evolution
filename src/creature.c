/**********************************************************//**
 * @file creature.c
 * @brief Implementation of generic mass-spring creatures
 * as well as their behaviors.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

// External libraries
#ifdef WINDOWS
#include <windows.h>        // OpenGL, GLUT ...
#endif
#include <GL/glew.h>        // OpenGL
#include <GL/glut.h>        // GLUT

// This project
#include "debug.h"          // assert, eprintf
#include "vector.h"         // VECTOR
#include "integral.h"       // INTEGRAL
#include "random.h"         // randint
#include "creature.h"       // CREATURE

/// Forced time step used in discretized updates.
#define TIME_STEP 0.001

/// @brief Probability that a random action stream will contain
/// an actual action as opposed to a wait / stop action.
#define ACTION_DENSITY 0.1

/// Bounciness as a node hits the ground.
#define RESTITUTION 0.1
#define GRAVITY -9.8
#define DAMPING 10

/// Number of trials to evaluate fitness.
#define FITNESS_TRIALS 10

/// The system integrator
static INTEGRAL integrate = &EulerMethod;

/**********************************************************//**
 * @brief Test if the give CREATURE is valid for debugging.
 * @param creature: The creature to test.
 **************************************************************/
static void AssertCreature(const CREATURE *creature) {
    // Proper number of nodes in creature
    assert(creature->nNodes >= MIN_NODES);
    assert(creature->nNodes <= MAX_NODES);
    
    // Proper number of muscles in creature
    assert(creature->nMuscles >= creature->nNodes);
    assert(creature->nMuscles <= MAX_MUSCLES);
    
    // Node verification
    for (int i = 0; i < creature->nNodes; i++) {
        const NODE *node = &creature->nodes[i];
        
        // Friction of the node
        assert(node->friction >= MIN_FRICTION);
        assert(node->friction <= MAX_FRICTION);
        
        // Collision check
        assert(node->initial.y >= 0.0);
        
        // NaN check
        assert(!vector_IsNaN(&node->initial));
        assert(!vector_IsNaN(&node->position));
        assert(!vector_IsNaN(&node->velocity));
        assert(!vector_IsNaN(&node->acceleration));
    }
    
    // Muscle verification
    for (int i = 0; i < creature->nMuscles; i++) {
        const MUSCLE *muscle = &creature->muscles[i];
        
        // Muscle order check
        assert(muscle->first >= 0);
        assert(muscle->first < creature->nMuscles);
        assert(muscle->second >= 0);
        assert(muscle->second < creature->nMuscles);
        
        // Muscle length check
        assert(muscle->contracted <= muscle->extended);
        assert(muscle->contracted >= MIN_CONTRACTED_LENGTH);
        assert(muscle->extended >= MIN_CONTRACTED_LENGTH);
        assert(muscle->contracted <= MAX_MUSCLE_LENGTH);
        assert(muscle->extended <= MAX_MUSCLE_LENGTH);
        
        // Muscle strength check
        assert(muscle->strength >= MIN_STRENGTH);
        assert(muscle->strength <= MAX_STRENGTH);
    }
}

// Turn off this assert function if the DEBUG macro is off
#ifndef DEBUG
#define AssertCreature(c) (void)0
#endif

/**********************************************************//**
 * @brief Print the given creature information to stdout.
 * @param creature: The creature to inspect.
 **************************************************************/
void creature_Print(const CREATURE *creature) {
    // Header information
    printf("Creature 0x%p: ", (void *)creature);
    printf("%d nodes, ", creature->nNodes);
    printf("%d muscles\n", creature->nMuscles);
    
    // Node information
    for (int i = 0; i < creature->nNodes; i++) {
        const NODE *node = &creature->nodes[i];
        printf("  Node %d: ", i);
        printf("at <%.2f, %.2f, %.2f>, ", node->initial.x, node->initial.y, node->initial.z);
        printf("friction %f\n", node->friction);
    }
    printf("\n");
    
    // Muscle information
    for (int i = 0; i < creature->nMuscles; i++) {
        const MUSCLE *muscle = &creature->muscles[i];
        printf("  Muscle %d ", i);
        printf("(%d to %d): ", muscle->first, muscle->second);
        printf("length %.2f to %.2f ", muscle->contracted, muscle->extended);
        printf((muscle->isContracted)? "(contracting)": "(extending)");
        printf(", strength %.2f\n", muscle->strength);
    }
    printf("\n");
}

/*============================================================*
 * Random creature generation
 *============================================================*/
void creature_CreateRandom(CREATURE *creature) {
    // Create random nodes and muscles
    creature->nNodes = randint(MIN_NODES, MAX_NODES);
    creature->nMuscles = randint(creature->nNodes, MAX_MUSCLES);
    creature->clock = 0.0;
    
    // Generate initial fitness table.
    for (int i = 0; i < N_BEHAVIORS; i++) {
        creature->fitness[i] = FITNESS_INVALID;
    }
    
    // Generate the random nodes. Assume all nodes reside
    // inside the unit sphere for simplicity.
    for (int i = 0; i < creature->nNodes; i++) {
        NODE *node = &creature->nodes[i];
        
        // Initialize this node's position
        node->initial.x = uniform(MIN_POSITION, MAX_POSITION);
        node->initial.y = uniform(0.0, MAX_POSITION);
        node->initial.z = uniform(MIN_POSITION, MAX_POSITION);
        
        // Initial position, velocity, acceleration
        node->position = node->initial;
        vector_Set(&node->velocity, 0, 0, 0);
        vector_Set(&node->acceleration, 0, 0, 0);

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
            muscle->first = randint(0, creature->nNodes-1);
        }
        
        // Random destination node, but no self-muscles
        // as those would do nothing.
        muscle->second = randint(0, creature->nNodes-1);
        if (muscle->first == muscle->second) {
            // Jump to next if a self-muscle appears
            muscle->second = (muscle->second + 1) % creature->nNodes;
        }
        
        // Get the actual muscle distance
        const NODE *first = &creature->nodes[muscle->first];
        const NODE *second = &creature->nodes[muscle->second];
        VECTOR delta = second->initial;
        vector_Subtract(&delta, &first->initial);
        float length = vector_Length(&delta);
        
        // Get random contract or expand lengths
        muscle->extended = length;
        muscle->contracted = uniform(length/2, length);
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
 * @struct MUTATION
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
        node->initial.x = uniform(MIN_POSITION, MAX_POSITION);
        node->initial.y = uniform(0.0, MAX_POSITION);
        node->initial.z = uniform(MIN_POSITION, MAX_POSITION);
        break;
        
    case NODE_FRICTION:
        // Change a random node friction
        node->friction = uniform(MIN_FRICTION, MAX_FRICTION);
        break;
    
    case MUSCLE_ANCHOR:
        // Changes a random muscle anchor point
        muscle->second = randint(0, creature->nNodes-1);
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
    child->clock = 0.0;
    
    // Generate initial fitness table.
    for (int i = 0; i < N_BEHAVIORS; i++) {
        child->fitness[i] = FITNESS_INVALID;
    }
    
    // Inherit actual node properties
    for (int i = 0; i < child->nNodes; i++) {
        // Inherit this node from either parent
        const CREATURE *selected;
        if ((randint(0, 1) == 0 && i < mother->nNodes) || i >= father->nNodes) {
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
        if ((randint(0, 1) == 0 && i < mother->nMuscles) || i >= father->nMuscles) {
            selected = mother;
        } else {
            selected = father;
        }
        assert(i < selected->nMuscles);
        
        // Copy over the muscle data, ensuring that the
        // node indices are actually valid nodes.
        child->muscles[i] = selected->muscles[i];
        child->muscles[i].isContracted = false;
        
        // This is essentially a mutation: if there
        // are fewer nodes, rebind the muscle to a
        // valid node.
        if (child->muscles[i].first >= child->nNodes || child->muscles[i].second >= child->nNodes) {
            int first = randint(0, child->nNodes-1);
            int second = randint(0, child->nNodes-1);
            if (first == second) {
                // Jump to next if a self-muscle appears
                second = (second + 1) % child->nNodes;
            }
            child->muscles[i].first = first;
            child->muscles[i].second = second;
        }
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

 /**********************************************************//**
 * @brief Updates the creature's mass-spring system independent
 * of any discretized time step or animation configuration.
 * @param creature: The creature to update.
 * @param dt: The time step in seconds.
 **************************************************************/
static void creature_UpdateFull(CREATURE *creature, float dt) {
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
        
        // No force exterted when muscle has no strength
        if (iszero(muscle->strength)) {
            continue;
        }
        
        // Get the current muscle length and normalize the
        // direction of the muscle.
        VECTOR delta = second->position;
        vector_Subtract(&delta, &first->position);
        float length = vector_Length(&delta);
        vector_Multiply(&delta, 1.0 / length);
        
        // Get the muscle force. The strength is per unit
        // of target length, so divide that out too.
        float targetLength = muscle->isContracted? muscle->contracted: muscle->extended;
        float forceMagnitude = -(muscle->strength/targetLength)*(targetLength - length);
        
        // Get the velocity damping force. These velocities are along 
        // the vector connecting the masses, not in general.
        float firstVelocity = vector_Dot(&delta, &first->velocity);
        float secondVelocity = vector_Dot(&delta, &second->velocity);
        forceMagnitude -= DAMPING*(firstVelocity-secondVelocity);

        // Normalize delta and scale by force magnitude all
        // in one multiplication step.
        VECTOR force = delta;
        vector_Multiply(&force, forceMagnitude);
        
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
        
        // No friction applied to nodes that aren't on the ground
        // or nodes that are frictionless.
        if (!iszero(node->position.y) || iszero(node->friction)) {
            continue;
        }
        
        // Get the frictional force, which points opposite of the
        // velocity of the node and is scaled by the frictioniness.
        VECTOR friction = node->velocity;
        if (vector_IsZero(&friction)) {
            continue;
        }
        vector_Normalize(&friction);
        vector_Multiply(&friction, -node->friction);
        
        // Project frictional force onto XZ plane
        // only (the ground).
        friction.y = 0.0;
        
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

/*============================================================*
 * Creature discretized update
 *============================================================*/
void creature_Update(CREATURE *creature, float dt) {
    int fullSteps = (int)(dt / TIME_STEP);
    float partialStep = fmod(dt, TIME_STEP);
    for (int i = 0; i < fullSteps; i++) {
        creature_UpdateFull(creature, TIME_STEP);
    }
    creature_UpdateFull(creature, partialStep);
}

/*============================================================*
 * Creature evaluation
 *============================================================*/
void creature_Animate(CREATURE *creature, BEHAVIOR behavior, float dt) {
    // Get the total number of animation updates within
    // the given time step.
    float timeBefore = fmod(creature->clock+ACTION_TIME, ACTION_TIME);
    int fullActions = (int)(dt / ACTION_TIME);
    float timeAfter = fmod(creature->clock+dt, ACTION_TIME);
    
    // Index into the current behavior
    int animationIndex = (int)(fmod(creature->clock, BEHAVIOR_TIME)*MAX_ACTIONS/BEHAVIOR_TIME);
    
    // Animate the current behavior for the remaining "before"
    // time step, before we flip the action.
    creature_Update(creature, timeBefore);
    creature->clock += timeBefore;
    
    // Step all subsequent actions
    float endTime = creature->clock + dt;
    while (creature->clock < endTime) {
        // Animate the next step by flipping the contract flag of the
        // muscle specified in the action stream.
        int action = creature->behavior[behavior].action[animationIndex];
        if (action != MUSCLE_NONE) {
            creature->muscles[action].isContracted = !creature->muscles[action].isContracted;
        }
        
        // Determine if this is a full step or not
        if (fullActions > 0) {
            // Doing a full step
            creature_Update(creature, ACTION_TIME);
            creature->clock += ACTION_TIME;
            fullActions--;
        } else {
            creature_Update(creature, timeAfter);
            creature->clock += timeAfter;
        }
        
        // Cycle the action index back around
        animationIndex = (animationIndex + 1) % MAX_ACTIONS;
    }
}

/**********************************************************//**
 * @brief Computes the average NODE position.
 * @param creature: The creature to inspect.
 * @return The average position of the creature's NODEs.
 **************************************************************/
static inline VECTOR AveragePosition(const CREATURE *creature) {
    VECTOR total = {0.0, 0.0, 0.0};
    for (int i = 0; i < creature->nNodes; i++) {
        vector_Add(&total, &creature->nodes[i].position);
    }
    vector_Multiply(&total, 1.0 / creature->nNodes);
    return total;
}

/**********************************************************//**
 * @brief Computes the average NODE velocity.
 * @param creature: The creature to inspect.
 * @return The average velocity of the creature's NODEs.
 **************************************************************/
static inline VECTOR AverageVelocity(const CREATURE *creature) {
    VECTOR total = {0.0, 0.0, 0.0};
    for (int i = 0; i < creature->nNodes; i++) {
        vector_Add(&total, &creature->nodes[i].velocity);
    }
    vector_Multiply(&total, 1.0 / creature->nNodes);
    return total;
}

/*============================================================*
 * Creature initialization
 *============================================================*/
void creature_Reset(CREATURE *creature) {
    // Reset biological clock
    creature->clock = 0.0;
    
    // Deactivate all muscles
    for (int i = 0; i < creature->nMuscles; i++) {
        MUSCLE *muscle = &creature->muscles[i];
        muscle->isContracted = false;
    }
    
    // Reset initial position
    for (int i = 0; i < creature->nNodes; i++) {
        NODE *node = &creature->nodes[i];
        node->position = node->initial;
        vector_Set(&node->velocity, 0.0, 0.0, 0.0);
        vector_Set(&node->acceleration, 0.0, 0.0, 0.0);
    }
    
    // Approach rest by stepping through time until the
    // creature has no velocity.
    VECTOR start = AverageVelocity(creature);
    VECTOR next;
    VECTOR delta;
    do {
        creature_UpdateFull(creature, TIME_STEP);
        next = AverageVelocity(creature);
        delta = next;
        vector_Subtract(&delta, &start);
        start = next;
    } while (!vector_IsZero(&delta));
    
    // Store these new coordinates as the real initial
    // node positions.
    for (int i = 0; i < creature->nNodes; i++) {
        NODE *node = &creature->nodes[i];
        node->initial = node->position;
    }
}

/**********************************************************//**
 * @brief Models the creature walking forward using its
 * FORWARD MOTION. This is repeated FITNESS_TRIALS times for
 * an averaging effect. The fitness is based on the total
 * distance travelled in the X-direction (positive), and is
 * negatively impacted by significant motion in the Y and Z
 * directions.
 * @param creature: The creature to inspect.
 * @return The fitness of the walk animation.
 **************************************************************/
static float WalkFitness(CREATURE *creature) {
    // Evaluate the creature's walking fitness. To do this we
    // will loop the walking animation ten times
    VECTOR start = AveragePosition(creature);
    VECTOR end;
    
    // Count all positive x motions. However, penalize if there
    // is tons of variance in the Y and Z directions: we only
    // want to go forwards (and repeatably so).
    float xMotionTotal = 0.0;
    float yMotionMagnitudeTotal = 0.0;
    float zMotionMagnitudeTotal = 0.0;
    
    // Do the given number of trials subsequently without
    // resetting the creature.
    for (int trial = 0; trial < FITNESS_TRIALS; trial++) {
        // Perform a whole cycle of the animation
        creature_Animate(creature, FORWARD, BEHAVIOR_TIME);
        
        // Sample the difference again
        end = AveragePosition(creature);
        VECTOR delta = end;
        vector_Subtract(&delta, &start);
        xMotionTotal += delta.x;
        yMotionMagnitudeTotal += fabs(delta.y);
        zMotionMagnitudeTotal += fabs(delta.z);
    }
    
    // Get the final fitness
    float totalFitness = xMotionTotal - yMotionMagnitudeTotal - zMotionMagnitudeTotal;
    return totalFitness / FITNESS_TRIALS;
}

/*============================================================*
 * Overall fitness function
 *============================================================*/
float creature_Fitness(CREATURE *creature, BEHAVIOR behavior) {
    // Reset the creature for evaluation purposes, so the
    // creature always begins at rest and there are no weird
    // initial spasms.
    creature_Reset(creature);
    
    // Check memoized fitness table
    float fitness = creature->fitness[behavior];
    if (fitness != FITNESS_INVALID) {
        return fitness;
    }
    
    // We actually need to evaluate the fitness
    switch (behavior) {
    case FORWARD:
        fitness = WalkFitness(creature);
        break;
    
    default:
        fitness = FITNESS_INVALID;
        break;
    }
    
    // Store the fitness in the memo table
    creature->fitness[behavior] = fitness;
    return fitness;
}

/*============================================================*
 * Drawing function
 *============================================================*/
void creature_Draw(const CREATURE *creature) {
    // Draw the creature's nodes
    for (int i = 0; i < creature->nNodes; i++) {
        // Draw one node as a sphere
        const NODE *node = &creature->nodes[i];
        glPushMatrix();
        glTranslatef(node->position.x, node->position.y, node->position.z);
        
        // Set color based on the node friction
        glColor3f(1.0, 1.0-node->friction, 1.0-node->friction);
        glutSolidSphere(0.01, 20, 20);
        glPopMatrix();
    }
    
    // Draw the wire-frame of the muscles
    glBegin(GL_LINES);
    for (int i = 0; i < creature->nMuscles; i++) {
        // Gather spring data
        const MUSCLE *muscle = &creature->muscles[i];
        const NODE *first = &creature->nodes[muscle->first];
        const NODE *second = &creature->nodes[muscle->second];
        
        // Plot the muscle
        if (iszero(first->position.y)) {
            glColor3f(0.0, 0.0, 1.0);
        } else {
            glColor3f(1.0, 1.0, 1.0);
        }
        glVertex3f(first->position.x, first->position.y, first->position.z);
        if (iszero(second->position.y)) {
            glColor3f(0.0, 0.0, 1.0);
        } else {
            glColor3f(1.0, 1.0, 1.0);
        }
        glVertex3f(second->position.x, second->position.y, second->position.z);
    }
    glEnd();
}

/*============================================================*/
