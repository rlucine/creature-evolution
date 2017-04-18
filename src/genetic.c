/**********************************************************//**
 * @file genetic.c
 * @brief Implementation of general genetic algorithm.
 * @version 1.0
 * @author Alec Shinomiya
 * @date April 2017
 **************************************************************/

// Standard library
#include <stddef.h>     // size_t
#include <stdbool.h>    // bool
#include <stdlib.h>     // malloc
#include <string.h>     // memcpy
#include <math.h>       // INFINITY

// This project
#include "debug.h"      // eprintf
#include "heap.h"       // HEAP
#include "genetic.h"    // GENETIC, GENETIC_REQUEST

/**********************************************************//**
 * @brief Gets the entity associated with the given index.
 * @param data: The GENETIC algorithm data.
 * @param index: The index of the entity sought. This can be
 * from 0 to the population size - 1.
 * @return Pointer to the entity. This could be out of bounds
 * on invalid arguments.
 **************************************************************/
static inline void *genetic_Entity(GENETIC *data, int index) {
    return ((char *)data->entities) + index;
}

/**********************************************************//**
 * @brief Gets the index of the given entity.
 * @param data: The GENETIC algorithm data.
 * @param entity: A valid pointer to an entity in the data.
 * @return Index of the entity. This could be out of bounds
 * on invalid arguments.
 **************************************************************/
static inline int genetic_Index(const GENETIC *data, const void *entity) {
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
static inline void *genetic_Newborn(GENETIC *data, int index) {
    return ((char *)data->newborn) + index;
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
    
    // Allocates data for the genetic algorithm
    data->entities = malloc(data->entitySize*data->populationSize);
    if (!data->entities) {
        eprintf("Failed to allocate entities array.\n");
        return false;
    }
    
    // Create the sorting heap
    if (!heap_Create(&data->heap, data->populationSize)) {
        eprintf("Failed to create organism heap.\n");
        free(data->entities);
        return false;
    }
    
    // Create the newborn list
    data->newborn = malloc(data->entitySize*(data->populationSize/2 + 1));
    if (!data->newborn) {
        eprintf("Failed to create newborn array.\n");
        free(data->entities);
        heap_Destroy(&data->heap);
        return false;
    }
    
    // Initial seed
    for (int i = 0; i < data->populationSize; i++) {
        data->random(genetic_Entity(data, i));
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
    for (int i = 0; i < data->populationSize; i++) {
        heap_Push(&data->heap, i, data->fitness(genetic_Entity(data, i)));
    }
    
    // Set the best individual
    const HEAP_ELEMENT *best = heap_Top(&data->heap);
    data->best = genetic_Entity(data, best->payload);
    data->bestFitness = best->priority;
    
    // Decide what to do with organisms
    // Use this formula for nBreed to ensure half the population
    // gets paired up (multiple of 2).
    int nBreed = 2*(data->populationSize/4);
    
    // Generate all the newborns
    for (int n = 0; n < nBreed; n += 2) {
        int motherIndex;
        int fatherIndex;
        heap_Pop(&data->heap, &motherIndex);
        heap_Pop(&data->heap, &fatherIndex);
        void *mother = genetic_Entity(data, motherIndex);
        void *father = genetic_Entity(data, fatherIndex);
        void *son = genetic_Newborn(data, n);
        void *daughter = genetic_Newborn(data, n+1);
        data->breed(mother, father, son, daughter);
    }
    
    // Kill the remaining ones and place newborns in their place
    for (int n = 0; n < nBreed; n++) {
        int index;
        heap_Pop(&data->heap, &index);
        memcpy(genetic_Entity(data, index), genetic_Newborn(data, n), data->entitySize);
    }
    
    // If any stragglers left on the heap, just randomize them to keep
    // the same population size.
    while (heap_Size(&data->heap) > 0) {
        int index;
        heap_Pop(&data->heap, &index);
        data->random(genetic_Entity(data, index));
    }
}

/*============================================================*
 * Useful solver algorithm
 *============================================================*/
int genetic_Solve(GENETIC *data, float fitness, int timeout) {
    int generation = 0;
    while (generation < timeout || timeout == TIMEOUT_NONE) {
        genetic_Generation(data);
        generation++;
        if (data->bestFitness <= fitness) {
            return generation;
        }
    }
    return timeout;
}

/*============================================================*
 * Destructor
 *============================================================*/
void genetic_Destroy(GENETIC *data) {
    heap_Destroy(&data->heap);
    free(data->entities);
    free(data->newborn);
}

/*============================================================*/
