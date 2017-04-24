/**********************************************************//**
 * @file genetic.h
 * @brief Declaration of a general genetic algorithm.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

#ifndef _GENETIC_H_
#define _GENETIC_H_

// Standard library
#include <stddef.h>         // size_t
#include <stdbool.h>        // bool

// This project
#include "heap.h"           // HEAP

/// @brief Value used for specifying that a genetic algorithm
/// can proceed for infinite generations.
#define TIMEOUT_NONE 0

/**********************************************************//**
 * @typedef RANDOM_FUNCTION
 * @brief Generates a random entity.
 * @param entity: The location the data is stored.
 **************************************************************/
typedef void (*RANDOM_FUNCTION)(void *entity);

/**********************************************************//**
 * @typedef BREEDING_FUNCTION
 * @brief Breeds two entities and creates two children.
 * @param mother: The first parent.
 * @param father: The second parent.
 * @param son: The first child generated.
 * @param daughter: The second child generated.
 **************************************************************/
typedef void (*BREEDING_FUNCTION)(const void *mother, const void *father, void *son, void *daughter);

/**********************************************************//**
 * @typedef FITNESS_FUNCTION
 * @brief Get the fitness of the organism in any order.
 * @param entity: The entity to evaluate.
 * @return The fitness (smaller numbers are more fit).
 **************************************************************/
typedef float (*FITNESS_FUNCTION)(const void *entity);

/**********************************************************//**
 * @struct GENETIC_REQUEST
 * @brief Stores all the information required as user input
 * to construct a GENETIC structure.
 **************************************************************/
typedef struct {
    // Individuals
    size_t entitySize;          ///< The size of each organism's data.
    int populationSize;         ///< The number of entities in the population.
    
    // Functions
    RANDOM_FUNCTION random;     ///< Generates a random entity.
    BREEDING_FUNCTION breed;    ///< Breeds two entities.
    FITNESS_FUNCTION fitness;   ///< Gets the fitness of the entity.
} GENETIC_REQUEST;

/**********************************************************//**
 * @struct GENETIC
 * @brief Stores all information related to solving a problem
 * using a genetic algorithm.
 **************************************************************/
typedef struct {
    // Individuals
    size_t entitySize;          ///< The size of each organism's data.
    int populationSize;         ///< The number of entities in the population.
    
    // Functions
    RANDOM_FUNCTION random;     ///< Generates a random entity.
    BREEDING_FUNCTION breed;    ///< Breeds two entities.
    FITNESS_FUNCTION fitness;   ///< Gets the fitness of the entity.
    
    // Storage information
    void *entities;             ///< The actual creature data stored in any order.
    HEAP heap;                  ///< Heap used to sort organisms.
    void *newborn;              ///< List used to capture all the newborn creatures.
    const void *best;           ///< The best individual in the population.
    float bestFitness;          ///< The fitness of the best individual.
} GENETIC;

/**********************************************************//**
 * @brief Initializes a random population for the given
 * genetic algorithm configuration.
 * @param data: Storage location for algorithm data.
 * @param request: User-specified algorithm parameters.
 * @return Whether the creation suceeded.
 **************************************************************/
extern bool genetic_Create(GENETIC *data, const GENETIC_REQUEST *request);

/**********************************************************//**
 * @brief Get the best individual in the population.
 * @param data: Genetic algorithm data.
 * @return Pointer to the best entity, or NULL if invalid.
 **************************************************************/
static inline const void *genetic_Best(const GENETIC *data) {
    return data->best;
}

/**********************************************************//**
 * @brief Get the best individual in the population.
 * @param data: Genetic algorithm data.
 * @return Fitness of the best entity, or INFINITY if invalid.
 **************************************************************/
static inline float genetic_BestFitness(const GENETIC *data) {
    return data->bestFitness;
}

/**********************************************************//**
 * @brief Runs one generation of the genetic algorithm.
 * @param data: Algorithm data.
 **************************************************************/
extern void genetic_Generation(GENETIC *data);

/**********************************************************//**
 * @brief Runs the algorithm to completion.
 * @param data: Algorithm configuration.
 * @param fitness: The minimum desired fitness of the best
 * indivual. This will run until an individual with fitness
 * less than or equal to this value is found. Must be at least 0.
 * @param timeout: The maximum number of generations to run,
 * or TIMEOUT_NONE if infinite is desired.
 * @return The number of generations required.
 **************************************************************/
extern int genetic_Solve(GENETIC *data, float fitness, int timeout);

/**********************************************************//**
 * @brief Destroy the algorithm data.
 * @param data: Algorithm data to get rid of.
 **************************************************************/
static inline void genetic_Destroy(GENETIC *data) {
    heap_Destroy(&data->heap);
    free(data->entities);
    free(data->newborn);
}

/*============================================================*/
#endif // _GENETIC_H_
