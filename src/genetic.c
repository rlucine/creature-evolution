/**********************************************************//**
 * @file genetic.c
 * @brief Implementation of a general genetic algorithm.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

// Standard library
#include <stddef.h>         // size_t
#include <stdbool.h>        // bool
#include <stdlib.h>         // malloc
#include <string.h>         // memcpy
#include <math.h>           // INFINITY

// This project
#include "debug.h"          // eprintf
#include "heap.h"           // HEAP
#include "genetic.h"        // GENETIC, GENETIC_REQUEST

/**********************************************************//**
 * @brief Gets the entity associated with the given index.
 * @param data: The GENETIC algorithm data.
 * @param index: The index of the entity sought. This can be
 * from 0 to the population size - 1.
 * @return Pointer to the entity. This could be out of bounds
 * on invalid arguments.
 **************************************************************/
static inline void *Entity(GENETIC *data, int index) {
    return ((char *)data->entities) + index;
}

/**********************************************************//**
 * @brief Gets the index of the given entity.
 * @param data: The GENETIC algorithm data.
 * @param entity: A valid pointer to an entity in the data.
 * @return Index of the entity. This could be out of bounds
 * on invalid arguments.
 **************************************************************/
static inline int Index(const GENETIC *data, const void *entity) {
    ptrdiff_t split = (char *)entity - (char *)data->entities;
    return (int)split / (int)data->entitySize;
}

/**********************************************************//**
 * @brief Gets a pointer to the newborn entity at the given
 * index.
 * @param data: The GENETIC algorithm data.
 * @param index: The index of the entity sought. This can be
 * from 0 to the number of newborns - 1.
 * @return Pointer to the entity.
 **************************************************************/
static inline void *Newborn(GENETIC *data, int index) {
    return ((char *)data->newborn) + index;
}

/**********************************************************//**
 * @brief Gets the number of newborn to generate.
 * @param data: The GENETIC algorithm data.
 * @return Number of newborn per generation.
 **************************************************************/
static inline int NumberNewborn(const GENETIC *data) {
    return 2*(data->populationSize/4);
}

/*============================================================*
 * Creation function
 *============================================================*/
bool genetic_Create(GENETIC *data, const GENETIC_REQUEST *request) {
    // Move request information over
    data->entitySize = request->entitySize;
    data->populationSize = request->populationSize;
    data->random = request->random;
    data->breed = request->breed;
    data->fitness = request->fitness;
    
    // Allocates data for the entity array
    data->entities = malloc(data->entitySize*data->populationSize);
    if (!data->entities) {
        eprintf("Failed to allocate entities array.\n");
        return false;
    }
    
    // Create the fitness sorting heap
    if (!heap_Create(&data->heap, data->populationSize)) {
        eprintf("Failed to create organism heap.\n");
        free(data->entities);
        return false;
    }
    
    // Create the newborn array
    int newbornSize = NumberNewborn(data);
    data->newborn = malloc(data->entitySize*newbornSize);
    if (!data->newborn) {
        eprintf("Failed to create newborn array.\n");
        free(data->entities);
        heap_Destroy(&data->heap);
        return false;
    }
    
    // Initial population generation
    for (int i = 0; i < data->populationSize; i++) {
        void *where = Entity(data, i);
        data->random(where);
    }
    
    // Unrelated initialization
    data->best = NULL;
    data->bestFitness = INFINITY;
    return true;
}

/*============================================================*
 * Computes one generation
 *============================================================*/
void genetic_Generation(GENETIC *data) {
    // Create a heap to sort the population by fitness.
    // We re-use the same allocated heap for efficiency.
    for (int i = 0; i < data->populationSize; i++) {
        heap_Push(&data->heap, i, data->fitness(Entity(data, i)));
    }
    
    // Set the best individual's properties
    const HEAP_ELEMENT *best = heap_Top(&data->heap);
    data->best = Entity(data, best->payload);
    data->bestFitness = best->priority;
    
    // Get the number of newborn to generate.
    // Then generate all the newborn organisms using the breeding
    // function specified. The newborn array is probably full of
    // garbage at this point, we just overwrite it.
    int nBreed = NumberNewborn(data);
    for (int n = 0; n < nBreed; n += 2) {
        // Indicies the parents are the top ones on the heap.
        int motherIndex, fatherIndex;
        heap_Pop(&data->heap, &motherIndex);
        heap_Pop(&data->heap, &fatherIndex);
        void *mother = Entity(data, motherIndex);
        void *father = Entity(data, fatherIndex);
        
        // Get pointers to the newborn data slots
        void *son = Newborn(data, n);
        void *daughter = Newborn(data, n+1);
        data->breed(mother, father, son, daughter);
    }
    
    // Kill the remaining individuals and place newborns in their place
    for (int n = 0; n < nBreed && !heap_IsEmpty(&data->heap); n++) {
        // Pop an individual from the heap who must die.
        int killIndex;
        heap_Pop(&data->heap, &killIndex);
        
        // Copy the newborn into the entity array.
        memcpy(Entity(data, killIndex), Newborn(data, n), data->entitySize);
    }
    
    // If any stragglers left on the heap, just randomize them to keep
    // the same population size.
    while (!heap_IsEmpty(&data->heap)) {
        // Overwrite with a random index
        int randomIndex;
        heap_Pop(&data->heap, &randomIndex);
        data->random(Entity(data, randomIndex));
    }
}

/*============================================================*
 * Useful solver algorithm
 *============================================================*/
int genetic_Solve(GENETIC *data, float fitness, int timeout) {
    int generation = 0;
    while (timeout == TIMEOUT_NONE || generation < timeout) {
        genetic_Generation(data);
        
        // Termination check for fitness
        generation++;
        if (data->bestFitness <= fitness) {
            return generation;
        }
    }
    
    // Only get here if we timed out, otherwise could go forever.
    return timeout;
}

/*============================================================*/
