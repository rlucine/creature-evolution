/**********************************************************//**
 * @file creature.h
 * @brief Declaration of generic mass-spring creatures
 * as well as their behaviors.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

#ifndef _CREATURE_H_
#define _CREATURE_H_

// Standard library
#include <stdbool.h>        // bool

// This project
#include "vector.h"         // VECTOR

/**********************************************************//**
 * @struct NODE
 * @brief Declares a vertex the creature uses to move around.
 * This holds the position and physical properties.
 **************************************************************/
typedef struct {
    VECTOR initial;         ///< The initial position.
    VECTOR position;        ///< The location of the node.
    VECTOR velocity;        ///< The current node velocity.
    VECTOR acceleration;    ///< Current node acceleration.
    float friction;         ///< Coefficient of friction.
} NODE;

//**************************************************************
// Node amounts
#define MIN_NODES 4         ///< Minimum number of NODEs needed per creature.
#define MAX_NODES 16        ///< Maximum number of NODEs per creature.

// Node properties
#define MIN_POSITION -1.0   ///< Minimum initial XZ position of a NODE.
#define MAX_POSITION 1.0    ///< Maximum initial XZ position of a NODE.
#define MIN_FRICTION 0.1    ///< Minimum friction of a NODE.
#define MAX_FRICTION 2.0    ///< Maximum friction of a NODE.

/**********************************************************//**
 * @struct MUSCLE
 * @brief Connects two NODE structs together.
 **************************************************************/
typedef struct {
    int first;              ///< The index of the first NODE.
    int second;             ///< The index of the second NODE.
    float extended;         ///< Extended muscle length.
    float contracted;       ///< Contracted muscle length.
    float strength;         ///< Stiffness of the muscle.
    bool isContracted;      ///< Whether the muscle is contracting.
} MUSCLE;

//**************************************************************
/// Maximum number of MUSCLEs per creature.
#define MAX_MUSCLES 64

/// @brief The maximum number of actions occurring in
/// one period of a cyclic MOTION.
#define MAX_ACTIONS 64

// Configurations
#define MIN_STRENGTH 1.0    ///< The minumum strength of a MUSCLE.
#define MAX_STRENGTH 20.0   ///< Maximum strength of a MUSCLE.

/// The minimum length of a contracted MUSCLE.
#define MIN_CONTRACTED_LENGTH 0.25

/// The minumum length of an extended MUSCLE.
#define MIN_EXTENDED_LENGTH 0.5

/// The maximum length of a MUSCLE in any state.
#define MAX_MUSCLE_LENGTH 2.0

/**********************************************************//**
 * @struct MOTION
 * @brief Defines a a list of muscle indexes to contract or
 * expand. Each muscle has its state flipped when this occurs.
 * If MUSCLE_NONE is specified, then nothing happens and
 * playback is essentially sustained for that action.
 **************************************************************/
typedef struct {
    /// List of muscle indices to contract or expand.
    unsigned char action[MAX_ACTIONS];
} MOTION;

//**************************************************************
/// Signals that no muscle should contract.
#define MUSCLE_NONE 255

/// The actual time spent to perform a BEHAVIOR in seconds.
#define BEHAVIOR_TIME 1.0

/// The actual time spent to perform one action.
#define ACTION_TIME (BEHAVIOR_TIME/MAX_ACTIONS)

/**********************************************************//**
 * @struct CREATURE
 * @brief Aggregates together all behaviors and physiology
 * for one virtual mass-spring creature.
 **************************************************************/
typedef struct {
    int nNodes;             ///< Number of distinct nodes.
    int nMuscles;           ///< Number of distinct muscles.
    float clock;            ///< The creature's biological clock.
    float energy;           ///< Energy spent by the creature.
    NODE nodes[MAX_NODES];  ///< NODE data for one creature.
    MUSCLE muscles[MAX_MUSCLES];    ///< MUSCLE data for one creature.
    MOTION behavior;        ///< MOTION data for each distinct hehavior.
    float fitness;          ///< Buffered fitness data.
} CREATURE;

//**************************************************************
/// Invalid fitness amount.
#define FITNESS_INVALID -1.0

/**********************************************************//**
 * @brief Generates an entirely random creature.
 * @param creature: Data is stored at this location.
 **************************************************************/
extern void creature_CreateRandom(CREATURE *creature);

/**********************************************************//**
 * @brief Resets the creature state and finds a stable
 * initial position based on the initial node positions.
 * @param creature: The initialized creature data.
 **************************************************************/
extern void creature_Reset(CREATURE *creature);

/**********************************************************//**
 * @brief Applies a random mutation to some aspect of the
 * creature. This can mutate the initial node position,
 * node friction, muscle properties and attachment, and
 * actions found within a behavior.
 * @param creature: The data to mutate.
 **************************************************************/
extern void creature_Mutate(CREATURE *creature);

/**********************************************************//**
 * @brief Recombines the parent properties to create a child
 * CREATURE.
 * @param mother: The data of one parent.
 * @param father: The data of the other parent.
 * @param child: Location to stor the child data at.
 **************************************************************/
extern void creature_Breed(const CREATURE *mother, const CREATURE *father, CREATURE *child);

/**********************************************************//**
 * @brief Updates the creature's mass-spring system. This
 * upsate is discretized to use the given TIME_STEP variable.
 * @param creature: The creature to update.
 * @param dt: The time step in seconds.
 **************************************************************/
extern void creature_Update(CREATURE *creature, float dt);

/**********************************************************//**
 * @brief Plays back the animation for the given behavior.
 * @param creature: The creature to animate.
 * @param dt: The time step in seconds.
 **************************************************************/
extern void creature_Animate(CREATURE *creature, float dt);

/**********************************************************//**
 * @brief Animates the creature without moving muscles until
 * it is at rest.
 * @param creature: The creature to animate.
 * @param dt: The time step in seconds.
 * @return Whether the creature is at rest.
 **************************************************************/
extern bool creature_Rest(CREATURE *creature, float dt);

/**********************************************************//**
 * @brief Gets the fitness of the given behavior. This uses
 * a memo table in each creature to avoid recomputation.
 * @param creature: The creature to inspect.
 * @return The fitness of that behavior.
 **************************************************************/
extern float creature_Fitness(CREATURE *creature);

/**********************************************************//**
 * @brief Draw the creature on the screen.
 * @param creature: The creature to render.
 **************************************************************/
extern void creature_Draw(const CREATURE *creature);

/**********************************************************//**
 * @brief Print the creature information on the screen.
 * @param creature: The creature to inspect.
 **************************************************************/
extern void creature_Print(const CREATURE *creature);

/*============================================================*/
#endif // _CREATURE_H_
