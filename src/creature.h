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
    VECTOR position;        ///< The location of the node.
    VECTOR velocity;        ///< The current node velocity.
    VECTOR acceleration;    ///< Current node acceleration.
    float friction;         ///< Coefficient of friction.
} NODE;

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
    bool isContracted;        ///< Whether the muscle is contracting.
} MUSCLE;

/// @brief Minimum number of NODEs needed per creature. This
/// enables the creature to be nonplanar.
#define MIN_NODES 4

/// @brief Maximum number of NODEs per creature.
#define MAX_NODES 16

/// @brief Maximum number of MUSCLEs per creature.
#define MAX_MUSCLES (MAX_NODES*2)

/// @brief The maximum number of actions occurring in
/// one period of a cyclic MOTION.
#define MAX_ACTIONS (MAX_MUSCLES*MAX_MUSCLES)

/// @brief Signals that no muscle should contract.
#define MUSCLE_NONE -1

/**********************************************************//**
 * @struct MOTION
 * @brief Defines a a list of muscle indexes to contract or
 * expand. Each muscle has its state flipped when this occurs.
 * If MUSCLE_NONE is specified, then nothing happens and
 * playback is essentially sustained for that action.
 **************************************************************/
typedef struct {
    /// List of muscle indices to contract or expand.
    int action[MAX_ACTIONS];
} MOTION;

/**********************************************************//**
 * @enum BEHAVIOR
 * @brief Defines keys for all the independent behaviors a 
 * creature should exhibit.
 **************************************************************/
typedef enum {
    FORWARD,                ///< Creature accelerates forward.
    ROTATE_LEFT,            ///< Creature rotates left in place.
    ROTATE_RIGHT,           ///< Creature rotates right in place.
    JUMP,                   ///< Creature springs off the ground.
    WAIT,                   ///< Creature remains in a nautral position.
} BEHAVIOR;

/// The total number of distinct BEHAVIOR keys.
#define N_BEHAVIORS 4

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
    float clock;			///< The creature's biological clock.
    
    /// NODE data for one creature.
    NODE nodes[MAX_NODES];
    
    /// MUSCLE data for one creature.
    MUSCLE muscles[MAX_NODES];
    
    /// BEHAVIOR data for each distinct hehavior.
    MOTION behavior[N_BEHAVIORS];
} CREATURE;

/**********************************************************//**
 * @brief Generates an entirely random creature.
 * @param creature: Data is stored at this location.
 **************************************************************/
extern void creature_CreateRandom(CREATURE *creature);

/*============================================================*/
#endif // _CREATURE_H_
