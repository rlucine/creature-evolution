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

//**************************************************************
/// Forced time step used in discretized updates.
#define TIME_STEP 0.005

/// @brief Probability that a random action stream will contain
/// an actual action as opposed to a wait / stop action.
#define ACTION_DENSITY 0.5

/// Bounciness as a node hits the ground.
#define RESTITUTION 0.6

/// Gravitational acceleration in the Y direction.
#define GRAVITY -1.0

/// Damping force between springs in the creatures.
#define DAMPING 1.5

/// Maximum number of mutations per creature.
#define MAX_MUTATIONS 4

/// Frictional force of the ground.
#define FRICTION 20.0

/// Number of trials to evaluate fitness.
#define FITNESS_TRIALS 10

/// Maximum energy expenditure.
#define MAX_ENERGY 65536

//**************************************************************
/// The system integrator
static INTEGRAL integrate = &MidpointMethod;

/// Gravity vector.
static const VECTOR GRAVITY_VECTOR = {0.0, GRAVITY, 0.0};

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

/**********************************************************//**
 * @brief Generate a random node. Assume all nodes reside
 * inside the unit hemisphere for simplicity.
 * @param creature: The creature to generate for.
 * @param index: The node to generate.
 **************************************************************/
static void GenerateNode(CREATURE *creature, int index) {
    // Node to generate
    NODE *node = &creature->nodes[index];
    
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

/**********************************************************//**
 * @brief Generate a random muscle. This must ensure all
 * the nodes are attached to each other so we don't get
 * a weird degenerate creature.
 * @param creature: The creature to generate for.
 * @param index: The muscle to generate.
 **************************************************************/
static void GenerateMuscle(CREATURE *creature, int index) {
    // Muscle to generate
    MUSCLE *muscle = &creature->muscles[index];
    
    // Forced muscle connection for each node
    if (index < creature->nNodes) {
        muscle->first = index;
        
        // Ensure the creature is fully connected. The
        // proof that this works comes from induction:
        // if the first n nodes are fully connected and
        // we must attach node n+1 to one of the previous
        // n nodes, then the n+1 nodes are fully connected.
        if (muscle->first > 0) {
            muscle->second = randint(0, index-1);
        } else {
            muscle->second = 0;
        }
    } else {
        muscle->first = randint(0, creature->nNodes-1);
        muscle->second = randint(0, creature->nNodes-1);
    }
    
    // Ban self-edges from appearing
    if (muscle->first == muscle->second) {
        // Jump to next if a self-muscle appears
        muscle->second = (muscle->second + 1) % creature->nNodes;
    }
    
    // Get the actual muscle length from the initial state.
    const NODE *first = &creature->nodes[muscle->first];
    const NODE *second = &creature->nodes[muscle->second];
    VECTOR delta = second->initial;
    vector_Subtract(&delta, &first->initial);
    float length = vector_Length(&delta);
    
    // Get random contract or expand lengths
    muscle->extended = length;
    muscle->contracted = uniform(length/2.0, length);
    muscle->isContracted = false;
    
    // Random muscle strength
    muscle->strength = uniform(MIN_STRENGTH, MAX_STRENGTH);
}

/*============================================================*
 * Random creature generation
 *============================================================*/
void creature_CreateRandom(CREATURE *creature) {
    // Create random nodes and muscles
    creature->nNodes = randint(MIN_NODES, MAX_NODES);
    creature->nMuscles = randint(creature->nNodes, MAX_MUSCLES);
    creature->clock = 0.0;
    creature->energy = 0.0;
    
    // Generate initial fitness memo.
    creature->fitness = FITNESS_INVALID;
    
    // Generate the creature parts
    for (int i = 0; i < creature->nNodes; i++) {
        GenerateNode(creature, i);
    }
    for (int i = 0; i < creature->nMuscles; i++) {
        GenerateMuscle(creature, i);
    }
    
    // Make the creature's motion
    for (int i = 0; i < MAX_ACTIONS; i++) {
        if (uniform(0.0, 1.0) < ACTION_DENSITY) {
            // Generate a real action
            creature->behavior.action[i] = randint(0, creature->nMuscles-1);
        } else {
            // Generate a no-op
            creature->behavior.action[i] = MUSCLE_NONE;
        }
    }
}

/*============================================================*
 * Creature initialization
 *============================================================*/
void creature_Reset(CREATURE *creature) {
    // Reset biological clock
    creature->clock = 0.0;
    creature->energy = 0.0;
    
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
}

/**********************************************************//**
 * @enum MUTATION
 * @brief Lists all the possible mutations that can occur.
 **************************************************************/
typedef enum {
    NODE_ADD,               ///< Add an extra node.
    NODE_REMOVE,            ///< Delete one of the nodes.
    NODE_POSITION,          ///< Change the start position of a node.
    NODE_FRICTION,          ///< Change the friction corfficient of the node.
    MUSCLE_ANCHOR,          ///< Change where a muscle is attached.
    MUSCLE_EXTENDED,        ///< Change the muscle extended length.
    MUSCLE_CONTRACTED,      ///< Change the muscle contract length.
    MUSCLE_STRENGTH,        ///< Change the muscle power.
    MUSCLE_ADD,             ///< Add a new muscle.
    MUSCLE_REMOVE,          ///< Remove one muscle.
    BEHAVIOR_ADD,           ///< Add an action to the motion.
    BEHAVIOR_REMOVE,        ///< Remove an action from the motion.
} MUTATION;

/// The number of unique possible mutations.
#define N_MUTATIONS 10

/**********************************************************//**
 * @brief Fix all the muscles in the creature, if they point
 * to nodes that no longer exist in the creature.
 * @param creature: The creature to fix.
 **************************************************************/
static inline void FixMuscles(CREATURE *creature) {
    for (int i = 0; i < creature->nMuscles; i++) {
        MUSCLE *muscle = &creature->muscles[i];
        if (muscle->first >= creature->nNodes || muscle->second >= creature->nNodes) {
            muscle->first = muscle->first % creature->nNodes;
            muscle->second = muscle->second % creature->nNodes;
            if (muscle->first == muscle->second) {
                muscle->second = (muscle->second + 1) % creature->nNodes;
            }
        }
    }
}
    
/*============================================================*
 * Randomly mutate the creature
 *============================================================*/
void creature_Mutate(CREATURE *creature) {
    // Pick any mutation to occur
    MUTATION mutation = randint(0, N_MUTATIONS-1);
    
    // Pre-generate all the random numbers
    NODE *node = &creature->nodes[randint(0, creature->nNodes-1)];
    MUSCLE *muscle = &creature->muscles[randint(0, creature->nMuscles-1)];
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
    
    case NODE_ADD:
        // Add a new node
        if (creature->nNodes < MAX_NODES) {
            GenerateNode(creature, creature->nNodes++);
        }
        break;
        
    case NODE_REMOVE:
        // Add a new node
        if (creature->nNodes > MIN_NODES) {
            creature->nNodes--;
        }
        FixMuscles(creature);
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
    
    case MUSCLE_ADD:
        // Add a muscle
        if (creature->nMuscles < MAX_MUSCLES) {
            GenerateMuscle(creature, creature->nMuscles++);
        }
        break;
    
    case MUSCLE_REMOVE:
        // Remove a muscle - but not a base connection
        // muscle!
        if (creature->nMuscles > creature->nNodes) {
            creature->nMuscles--;
        }
        break;
    
    case BEHAVIOR_ADD:
        // Add  anew action to the stream
        creature->behavior.action[action] = randint(0, creature->nMuscles-1);
        break;
    
    case BEHAVIOR_REMOVE:
        // Delete an action from the stream
        creature->behavior.action[action] = MUSCLE_NONE;
        break;
    
    default:
        // Should never happen
        eprintf("Erroneous mutation: %d\n", mutation);
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
    child->fitness = FITNESS_INVALID;
    
    // Inherit actual node properties.
    for (int i = 0; i < child->nNodes; i++) {
        // Inherit this node from either parent
        const CREATURE *selected;
        if ((randint(0, 1) == 0 && i < mother->nNodes) || i >= father->nNodes) {
            selected = mother;
        } else {
            selected = father;
        }
        
        // Copy over the node data
        child->nodes[i] = selected->nodes[i];
    }
    
    // Inherit muscles. These guarantee the new child is fully connected
    // by the proof of either parent's nodes.
    for (int i = 0; i < child->nMuscles; i++) {
        // Inherit the muscle from either parent
        const CREATURE *selected;
        if ((randint(0, 1) == 0 && i < mother->nMuscles) || i >= father->nMuscles) {
            selected = mother;
        } else {
            selected = father;
        }
        
        // Copy over the muscle data, ensuring that the
        // node indices are actually valid nodes.
        child->muscles[i] = selected->muscles[i];
        child->muscles[i].isContracted = false;
    }
    
    // We might need to fix up the muscles if we accidentally inherited a 
    // muscle that isn't compatible with the child's nodes.
    FixMuscles(child);
    
    // Inherit behaviors: pick a cross-over point within
    // the action stream and copy.
    int crossover = randint(0, MAX_ACTIONS-1);
    for (int j = 0; j < crossover; j++) {
        child->behavior.action[j] = mother->behavior.action[j];
    }
    for (int j = crossover; j < MAX_ACTIONS; j++) {
        child->behavior.action[j] = father->behavior.action[j];
    }
    
    // Mutate the child at random
    int nMutations = randint(0, MAX_MUTATIONS);
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
    for (int i = 0; i < creature->nNodes; i++) {
        creature->nodes[i].acceleration = GRAVITY_VECTOR;
    }
    
    // Compute all the muscle forces and apply them
    // as node accelerations. We have already assumed
    // that all nodes have uniform mass for simplicity.
    for (int i = 0; i < creature->nMuscles; i++) {
        // Get pointers to all relevant data
        const MUSCLE *muscle = &creature->muscles[i];
        NODE *first = &creature->nodes[muscle->first];
        NODE *second = &creature->nodes[muscle->second];
        
        assert(first != second);
        
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
        
        // Energy expenditure from the muscular force.
        if (muscle->isContracted) {
            creature->energy += dt*fabs(forceMagnitude);
        }
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
        vector_Multiply(&friction, -FRICTION*node->friction);
        
        // Project frictional force onto XZ plane
        // only (the ground).
        friction.y = 0.0;
        
        // Apply the frictional force
        vector_Add(&node->acceleration, &friction);
    }

    // Integrate the positions.
    // TODO time to collision with the ground.
    for (int i = 0; i < creature->nNodes; i++) {
        NODE *node = &creature->nodes[i];
        integrate(&node->position, &node->velocity, &node->acceleration, dt);
        
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
    // Forced maximum time step.
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
void creature_Animate(CREATURE *creature, float dt) {
    // Energy death
    if (creature->energy > MAX_ENERGY) {
        // Relax all the muscles
        for (int i = 0; i < MAX_MUSCLES; i++) {
            creature->muscles[i].isContracted = false;
        }
        creature_Update(creature, dt);
        return;
    }
    
    // Get the total number of animation updates within
    // the given time step.
    float timeBefore = ACTION_TIME - fmod(creature->clock, ACTION_TIME);
    int fullActions = (int)(dt / ACTION_TIME);
    float timeAfter = fmod(creature->clock+dt, ACTION_TIME);
    if (dt < timeBefore) {
        timeBefore = dt;
        fullActions = 0;
        timeAfter = 0.0;
    }
    
    // Index into the current behavior
    int animationIndex = (int)(fmod(creature->clock, BEHAVIOR_TIME)/ACTION_TIME);
    
    // Animate the current behavior for the remaining "before"
    // time step, before we flip the action.
    if (!iszero(timeBefore)) {
        creature_Update(creature, timeBefore);
        creature->clock += timeBefore;
    }
    
    // Step all subsequent actions
    while (fullActions >= 0) {
        // Animate the next step by flipping the contract flag of the
        // muscle specified in the action stream.
        int action = creature->behavior.action[animationIndex];
        if (action != MUSCLE_NONE) {
            creature->muscles[action].isContracted = !creature->muscles[action].isContracted;
        }
        
        // Cycle the action index back around
        animationIndex = (animationIndex + 1) % MAX_ACTIONS;
        
        // Determine if this is a full step or not
        if (fullActions > 0) {
            // Doing a full step
            creature_Update(creature, ACTION_TIME);
            creature->clock += ACTION_TIME;
        } else if (!iszero(timeAfter)) {
            creature_Update(creature, timeAfter);
            creature->clock += timeAfter;
        }
        fullActions--;
    }
}

/*============================================================*
 * Rest animation
 *============================================================*/
bool creature_Rest(CREATURE *creature, float dt) {
    creature_Update(creature, dt);
    for (int i = 0; i < creature->nNodes; i++) {
        const VECTOR *velocity = &creature->nodes[i].velocity;
        if (!iszero(velocity->x) || !iszero(velocity->z)) {
            return false;
        }
    }
    return true;
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

/**********************************************************//**
 * @brief Models the creature walking forward using its
 * MOTION. This is repeated FITNESS_TRIALS times for
 * an averaging effect. The fitness is based on the total
 * distance travelled in the X-direction (positive), and is
 * negatively impacted by significant motion in the Y and Z
 * directions.
 * @param creature: The creature to inspect.
 * @return The fitness of the walk animation.
 **************************************************************/
static float WalkFitness(CREATURE *creature) {
    // Allow creature to approach rest so we don;t overestimate
    // on accident.
    while (creature_Rest(creature, BEHAVIOR_TIME));
    
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
        creature_Animate(creature, BEHAVIOR_TIME);
        
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
float creature_Fitness(CREATURE *creature) {
    // Reset the creature for evaluation purposes, so the
    // creature always begins at rest and there are no weird
    // initial spasms.
    creature_Reset(creature);
    
    // Check memoized fitness table
    float fitness = creature->fitness;
    if (fitness != FITNESS_INVALID) {
        return fitness;
    }
    
    // We actually need to evaluate the fitness
    // Store the fitness in the memo table
    fitness = WalkFitness(creature);
    creature->fitness = fitness;
    return fitness;
}

/**********************************************************//**
 * @brief Computes the NODE color.
 * @param creature: The creature to color.
 * @param index: The node's index.
 * @return The RGB color for the node in a VECTOR.
 **************************************************************/
static inline VECTOR NodeColor(const CREATURE *creature, int index) {
    const NODE *node = &creature->nodes[index];
    VECTOR color = {0.0, 0.0, 0.0};
    
    // Red if the creature is dead.
    if (creature->energy >= MAX_ENERGY) {
        color.x = 1.0;
    }
    // Blue if the node is on the ground.
    if (node->position.y < 0.1) {
        color.z = 1.0;
    }
    
    // White else
    if (vector_IsZero(&color)) {
        color.x = 1.0;
        color.y = 1.0;
        color.z = 1.0;
    }
    
    return color;
}

/*============================================================*
 * Drawing function
 *============================================================*/
void creature_Draw(const CREATURE *creature) {
    // Draw the shadow of the muscles on the ground
    glBegin(GL_LINES);
    glColor3f(0.0, 0.0, 0.0);
    for (int i = 0; i < creature->nMuscles; i++) {
        // Gather spring data
        const MUSCLE *muscle = &creature->muscles[i];
        const NODE *first = &creature->nodes[muscle->first];
        const NODE *second = &creature->nodes[muscle->second];
        
        // Plot the muscle
        glVertex3f(first->position.x, -0.01, first->position.z);
        glVertex3f(second->position.x, -0.01, second->position.z);
    }
    glEnd();
    
    // Draw the wire-frame of the muscles
    glBegin(GL_LINES);
    for (int i = 0; i < creature->nMuscles; i++) {
        // Gather spring data
        const MUSCLE *muscle = &creature->muscles[i];
        const NODE *first = &creature->nodes[muscle->first];
        const NODE *second = &creature->nodes[muscle->second];
        
        // Plot the muscle
        VECTOR color;
        color = NodeColor(creature, muscle->first);
        glColor3f(color.x, color.y, color.z);
        glVertex3f(first->position.x, first->position.y, first->position.z);
        
        color = NodeColor(creature, muscle->second);
        glColor3f(color.x, color.y, color.z);
        glVertex3f(second->position.x, second->position.y, second->position.z);
    }
    glEnd();
}

/*============================================================*/
